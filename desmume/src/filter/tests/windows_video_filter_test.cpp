#include <stdio.h>
#include <string.h>
#include <vector>

#include "common.h"
#include "frontend/windows/video.h"
#include "frontend/windows/record_compositor.h"

int main()
{
	const size_t width = 256;
	const size_t screenHeight = 192;
	std::vector<u32> input(width * screenHeight * 2);
	for (size_t i = 0; i < input.size(); ++i)
		input[i] = 0xFF000000u | ((i * 2654435761u) & 0x00FFFFFFu);

	VideoInfo video;
	video.prescaleHD = 0;
	video.prescalePost = 0;
	video.currentfilter = VideoFilterTypeID_None;
	video.InitFilter();
	video.SetPrescale(1, 1);

	RecordPicture picture = {};
	picture.mode = RECORD_PICTURE_COMPOSED;
	picture.size = RECORD_SIZE_1X;
	RecordCompositor compositor;

	const int threadCounts[] = {0, 4, 0};
	for (int threads : threadCounts)
	for (int filter = 0; filter < VideoFilterTypeIDCount; ++filter)
	{
		video.SetFilterThreadCount(threads);
		video.setfilter(filter);
		if (!video.ResizeBuffers())
			return 1;
		ColorspaceConvertBuffer888xTo8888Opaque<true, false>(
			input.data(), video.buffer, input.size());
		if (filter != VideoFilterTypeID_None)
			video.filter();

		picture.filter = filter;
		picture.filterThreads = threads;
		if (!compositor.Configure(picture, width, screenHeight) ||
		    compositor.OutputWidth() != (size_t)video.width ||
		    compositor.OutputHeight() != (size_t)video.height)
		{
			printf("FAIL dimensions: %s, threads %d\n", recordCompose::FilterName(filter), threads);
			return 1;
		}

		compositor.BeginFrame(input.data(), NDSColorFormat_BGR888_Rev, 1.0f, 1.0f);
		std::vector<u8> output((size_t)video.width * video.height * 3);
		compositor.ComposeRows(output.data(), 0, video.height - 1);
		const u32 *expected = filter == VideoFilterTypeID_None ? video.buffer : video.filteredbuffer;
		for (int y = 0; y < video.height; ++y)
		{
			const u8 *row = &output[(size_t)(video.height - 1 - y) * video.width * 3];
			for (int x = 0; x < video.width; ++x)
			{
				const u32 pixel = expected[(size_t)y * video.width + x];
				if (row[x * 3] != (u8)pixel ||
				    row[x * 3 + 1] != (u8)(pixel >> 8) ||
				    row[x * 3 + 2] != (u8)(pixel >> 16))
				{
					printf("FAIL pixels: %s, threads %d at %d,%d\n", recordCompose::FilterName(filter), threads, x, y);
					return 1;
				}
			}
		}
	}

	video.setfilter(-1);
	if (video.currentfilter != VideoFilterTypeID_None)
		return 1;
	video.setfilter(VideoFilterTypeIDCount);
	if (video.currentfilter != VideoFilterTypeID_None)
		return 1;

	video.SetFilterThreadCount(0);
	video.SetPrescale(2, 1);
	const int scaledFilters[] = {VideoFilterTypeID_Nearest1_5X, VideoFilterTypeID_HQ3X};
	for (int filter : scaledFilters)
	{
		video.setfilter(filter);
		if (!video.ResizeBuffers())
			return 1;
		const VideoFilterAttributes attr = VideoFilter::GetAttributesByID((VideoFilterTypeID)filter);
		if ((size_t)video.width != 512 * attr.scaleMultiply / attr.scaleDivide ||
		    (size_t)video.height != 768 * attr.scaleMultiply / attr.scaleDivide)
			return 1;
		const size_t sourcePixels = (size_t)video.prefilterWidth * video.prefilterHeight;
		for (size_t i = 0; i < sourcePixels; ++i)
			video.buffer[i] = 0xFF000000u | ((i * 2654435761u) & 0x00FFFFFFu);
		std::vector<u32> expected((size_t)video.width * video.height);
		VideoFilter::RunFilterCustomByID(video.buffer, expected.data(),
			video.prefilterWidth, video.prefilterHeight, (VideoFilterTypeID)filter);
		video.filter();
		if (memcmp(video.filteredbuffer, expected.data(), expected.size() * sizeof(u32)) != 0)
		{
			printf("FAIL prescale: %s\n", recordCompose::FilterName(filter));
			return 1;
		}
	}
	video.ShutdownFilter();
	printf("PASS: all %d filters, 0 and 4 threads, and 2x GPU prescale\n", VideoFilterTypeIDCount);
	return 0;
}
