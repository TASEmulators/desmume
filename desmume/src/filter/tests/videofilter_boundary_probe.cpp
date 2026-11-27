#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <set>
#include "filter/videofilter.h"

static void fillSource(VideoFilter &vf, int w, int h)
{
	uint32_t *src = vf.GetSrcBufferPtr();
	for (int i = 0; i < w * h; i++)
		src[i] = 0xFF000000u | (uint32_t)((i * 2654435761u) & 0x00FFFFFFu);
}

int main(int argc, char **argv)
{
	const int w = 256, h = 384;
	const int typeID = (argc > 1) ? atoi(argv[1]) : VideoFilterTypeID_HQ2X;
	const int threads = (argc > 2) ? atoi(argv[2]) : 2;

	VideoFilter ref(w, h, VideoFilterTypeID_None, 1);
	ref.ChangeFilterByID((VideoFilterTypeID)typeID);
	fillSource(ref, w, h);
	uint32_t *refDst = ref.RunFilter();
	const size_t rw = ref.GetDstWidth(), rh = ref.GetDstHeight();
	uint32_t *refCopy = (uint32_t *)malloc(rw * rh * 4);
	memcpy(refCopy, refDst, rw * rh * 4);

	VideoFilter vf(w, h, VideoFilterTypeID_None, threads);
	vf.ChangeFilterByID((VideoFilterTypeID)typeID);
	fillSource(vf, w, h);
	uint32_t *dst = vf.RunFilter();

	std::set<size_t> rows;
	size_t diffCount = 0;
	for (size_t i = 0; i < rw * rh; i++)
	{
		if (dst[i] != refCopy[i])
		{
			rows.insert(i / rw);
			diffCount++;
		}
	}

	printf("%s %dx%d threads=%d: %zu differing pixels in %zu rows:",
	       VideoFilter::GetTypeStringByID((VideoFilterTypeID)typeID), w, h, threads,
	       diffCount, rows.size());
	size_t shown = 0;
	for (std::set<size_t>::iterator it = rows.begin(); it != rows.end() && shown < 40; ++it, ++shown)
		printf(" %zu", *it);
	if (rows.size() > shown) printf(" ...");
	printf("\n");
	return 0;
}
