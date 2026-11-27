#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include "filter/videofilter.h"

static const char *typeName(VideoFilterTypeID t)
{
	return VideoFilter::GetTypeStringByID(t);
}

static void fillSource(VideoFilter &vf, int w, int h)
{
	uint32_t *src = vf.GetSrcBufferPtr();
	for (int i = 0; i < w * h; i++)
	{
		src[i] = 0xFF000000u | (uint32_t)((i * 2654435761u) & 0x00FFFFFFu);
	}
}

// Some filters duplicate the first/last row of their input surface when
// interpolating, so splitting the image across threads changes the pixels
// right next to each thread boundary. Compute the set of destination rows
// that may legitimately differ, using the same distribution as VideoFilter.
static std::vector<bool> boundaryRows(int srcHeight, int dstHeight, size_t threadCount, int blockSize, int scaleMultiply)
{
	std::vector<bool> allowed(dstHeight, false);
	if (srcHeight <= 0 || threadCount == 0)
	{
		return allowed;
	}

	if (blockSize < 1) blockSize = 1;
	const int totalBlocks = srcHeight / blockSize;
	const int blocksPerThread = (threadCount > 1) ? totalBlocks / (int)threadCount : totalBlocks;
	const int remainingLinesAtEnd = srcHeight % blockSize;
	int remainingBlocks = totalBlocks;
	int srcStart = 0;
	const int tolerance = (scaleMultiply > 2) ? scaleMultiply * 2 : 4;

	for (size_t i = 0; i < threadCount; i++)
	{
		int blockCount = (blocksPerThread < remainingBlocks) ? blocksPerThread : remainingBlocks;
		remainingBlocks -= blockCount;
		if (i == threadCount - 1)
		{
			blockCount += remainingBlocks;
		}

		int srcLines = blockCount * blockSize;
		if (i == threadCount - 1)
		{
			srcLines += remainingLinesAtEnd;
		}

		const int dstBoundary = (int)(((long long)srcStart * dstHeight) / srcHeight);
		for (int r = dstBoundary - tolerance; r <= dstBoundary + tolerance; r++)
		{
			if (r >= 0 && r < dstHeight)
			{
				allowed[r] = true;
			}
		}

		srcStart += srcLines;
	}

	return allowed;
}

int main()
{
	static const int sizes[][2] = {
		{256, 384}, {256, 192}, {128, 96}, {250, 380}, {254, 382}, {320, 480}, {256, 1}, {256, 7}
	};
	static const int threadCounts[] = {0, 1, 2, 3, 4, 7, 8, 10, 16};
	int failures = 0;
	int runs = 0;

	for (int t = 1; t < VideoFilterTypeIDCount; t++)
	{
		for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
		{
			const int w = sizes[s][0];
			const int h = sizes[s][1];

			// Reference output from a single worker thread.
			VideoFilter ref(w, h, VideoFilterTypeID_None, 1);
			ref.ChangeFilterByID((VideoFilterTypeID)t);
			fillSource(ref, w, h);
			uint32_t *refDst = ref.RunFilter();
			if (refDst == NULL)
			{
				printf("FAIL reference %s %dx%d\n", typeName((VideoFilterTypeID)t), w, h);
				failures++;
				continue;
			}
			const size_t refW = ref.GetDstWidth();
			const size_t refH = ref.GetDstHeight();
			uint32_t *refCopy = (uint32_t *)malloc(refW * refH * sizeof(uint32_t));
			memcpy(refCopy, refDst, refW * refH * sizeof(uint32_t));

			for (size_t c = 0; c < sizeof(threadCounts) / sizeof(threadCounts[0]); c++)
			{
				const int tc = threadCounts[c];

				VideoFilter vf(w, h, VideoFilterTypeID_None, tc);
				if (vf.GetThreadCount() != 0)
				{
					printf("FAIL lazy-create %s %dx%d tc=%d -> %zu\n", typeName((VideoFilterTypeID)t), w, h, tc, vf.GetThreadCount());
					failures++;
				}

				vf.ChangeFilterByID((VideoFilterTypeID)t);
				const size_t expectedThreads = (tc > 0) ? (size_t)tc : 0;
				if (vf.GetThreadCount() != expectedThreads)
				{
					printf("FAIL create %s %dx%d tc=%d -> %zu\n", typeName((VideoFilterTypeID)t), w, h, tc, vf.GetThreadCount());
					failures++;
				}

				const VideoFilterAttributes attr = vf.GetAttributes();
				const size_t expW = (size_t)w * attr.scaleMultiply / attr.scaleDivide;
				const size_t expH = (size_t)h * attr.scaleMultiply / attr.scaleDivide;
				const std::vector<bool> allowed = boundaryRows(h, (int)expH, expectedThreads, (int)attr.scaleDivide, (int)attr.scaleMultiply);

				fillSource(vf, w, h);
				uint32_t *dst = vf.RunFilter();
				if (dst == NULL || vf.GetDstWidth() != expW || vf.GetDstHeight() != expH)
				{
					printf("FAIL run %s %dx%d tc=%d dst=%zux%zu exp=%zux%zu\n",
					       typeName((VideoFilterTypeID)t), w, h, tc,
					       vf.GetDstWidth(), vf.GetDstHeight(), expW, expH);
					failures++;
				}
				else if (expW == refW && expH == refH)
				{
					for (size_t i = 0; i < refW * refH; i++)
					{
						if (dst[i] != refCopy[i] && !allowed[i / refW])
						{
							printf("FAIL pixels %s %dx%d tc=%d at pixel %zu (%zu,%zu)\n",
							       typeName((VideoFilterTypeID)t), w, h, tc, i, i % refW, i / refW);
							failures++;
							break;
						}
					}
				}

				// Change the worker count while the filter is active, then re-run.
				const int newThreadCount = (tc == 5) ? 3 : 5;
				vf.SetThreadCount((size_t)newThreadCount);
				if (vf.GetThreadCount() != (size_t)newThreadCount)
				{
					printf("FAIL resize-threads %s %dx%d tc=%d -> %zu\n", typeName((VideoFilterTypeID)t), w, h, tc, vf.GetThreadCount());
					failures++;
				}

				const std::vector<bool> allowedResized = boundaryRows(h, (int)expH, (size_t)newThreadCount, (int)attr.scaleDivide, (int)attr.scaleMultiply);
				fillSource(vf, w, h);
				dst = vf.RunFilter();
				if (dst == NULL || vf.GetDstWidth() != expW || vf.GetDstHeight() != expH)
				{
					printf("FAIL run-after-resize %s %dx%d tc=%d\n", typeName((VideoFilterTypeID)t), w, h, tc);
					failures++;
				}
				else if (expW == refW && expH == refH)
				{
					for (size_t i = 0; i < refW * refH; i++)
					{
						if (dst[i] != refCopy[i] && !allowedResized[i / refW])
						{
							printf("FAIL pixels-after-resize %s %dx%d tc=%d at pixel %zu (%zu,%zu)\n",
							       typeName((VideoFilterTypeID)t), w, h, tc, i, i % refW, i / refW);
							failures++;
							break;
						}
					}
				}

				// Toggle back to None and run again.
				vf.ChangeFilterByID(VideoFilterTypeID_None);
				if (vf.GetThreadCount() != 0)
				{
					printf("FAIL destroy %s %dx%d tc=%d -> %zu\n", typeName((VideoFilterTypeID)t), w, h, tc, vf.GetThreadCount());
					failures++;
				}
				dst = vf.RunFilter();
				if (dst == NULL || vf.GetDstWidth() != (size_t)w || vf.GetDstHeight() != (size_t)h)
				{
					printf("FAIL run-none %s %dx%d tc=%d\n", typeName((VideoFilterTypeID)t), w, h, tc);
					failures++;
				}

				// Re-select the filter: the remembered thread count must come back.
				vf.ChangeFilterByID((VideoFilterTypeID)t);
				if (vf.GetThreadCount() != (size_t)newThreadCount)
				{
					printf("FAIL restore %s %dx%d tc=%d -> %zu (want %d)\n", typeName((VideoFilterTypeID)t), w, h, tc, vf.GetThreadCount(), newThreadCount);
					failures++;
				}

				runs++;
			}

			free(refCopy);
		}
	}

	printf("runs=%d failures=%d\n", runs, failures);
	return failures != 0;
}
