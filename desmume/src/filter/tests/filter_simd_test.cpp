#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "filter/filter.h"
#include "types.h"

extern int systemRedShift;
extern int systemGreenShift;
extern int systemBlueShift;

struct TestSize
{
	int width;
	int height;
};

static u32 channel(u32 pixel, int shift)
{
	return (pixel >> shift) & 0xFF;
}

static u32 rgb(u32 red, u32 green, u32 blue)
{
	return (red << systemRedShift) | (green << systemGreenShift) |
	       (blue << systemBlueShift);
}

static void fillSource(std::vector<u32> &source, int srcStride, int width,
                       int height, int pattern)
{
	for (size_t i = 0; i < source.size(); i++)
		source[i] = 0x9E3779B9u * static_cast<u32>(i + 1);

	if (pattern == 0)
		return;

	static const u32 palette[] = {
		0xFF102030u, 0xFF405060u, 0xFF708090u, 0xFFA0B0C0u
	};
	for (int y = -1; y <= height; y++)
	{
		const int blockY = (y < 0) ? -1 : y / 2;
		for (int x = -1; x <= width; x++)
		{
			const int blockX = (x < 0) ? -1 : x / 2;
			const int color = (blockX + blockY + 8) % 4;
			source[(y + 1) * srcStride + x + 1] = palette[color];
		}
	}
}

static void referenceBilinear(const u32 *src, int srcStride, int width, int height,
                              u32 *dst, int dstStride)
{
	for (int y = 0; y < height; y++)
	{
		const u32 *row = src + y * srcStride;
		const u32 *next = (y + 1 < height) ? row + srcStride : row;
		u32 *dstEven = dst + (y * 2) * dstStride;
		u32 *dstOdd = dstEven + dstStride;
		for (int x = 0; x < width; x++)
		{
			const u32 a = row[x];
			const u32 b = (x + 1 < width) ? row[x + 1] : a;
			const u32 c = next[x];
			const u32 d = (x + 1 < width) ? next[x + 1] : c;
			const u32 ar = channel(a, systemRedShift);
			const u32 ag = channel(a, systemGreenShift);
			const u32 ab = channel(a, systemBlueShift);
			const u32 br = channel(b, systemRedShift);
			const u32 bg = channel(b, systemGreenShift);
			const u32 bb = channel(b, systemBlueShift);
			const u32 cr = channel(c, systemRedShift);
			const u32 cg = channel(c, systemGreenShift);
			const u32 cb = channel(c, systemBlueShift);
			const u32 dr = channel(d, systemRedShift);
			const u32 dg = channel(d, systemGreenShift);
			const u32 db = channel(d, systemBlueShift);

			dstEven[x * 2] = rgb(ar, ag, ab);
			dstEven[x * 2 + 1] = rgb((ar + br) >> 1, (ag + bg) >> 1, (ab + bb) >> 1);
			dstOdd[x * 2] = rgb((ar + cr) >> 1, (ag + cg) >> 1, (ab + cb) >> 1);
			dstOdd[x * 2 + 1] = rgb((ar + br + cr + dr) >> 2,
			                         (ag + bg + cg + dg) >> 2,
			                         (ab + bb + cb + db) >> 2);
		}
	}
}

static bool referenceEPX(const u32 *src, int srcStride, int width, int height,
                         u32 *dst, int dstStride)
{
	bool selectedCorner = false;
	for (int y = 0; y < height; y++)
	{
		const u32 *row = src + y * srcStride;
		const u32 *above = row - srcStride;
		const u32 *below = row + srcStride;
		u32 *dstEven = dst + (y * 2) * dstStride;
		u32 *dstOdd = dstEven + dstStride;
		for (int x = 0; x < width; x++)
		{
			const u32 left = row[x - 1];
			const u32 center = row[x];
			const u32 right = row[x + 1];
			const u32 top = above[x];
			const u32 bottom = below[x];
			if (left != right && top != bottom)
			{
				selectedCorner |= left == top || right == top ||
				                  left == bottom || bottom == right;
				dstEven[x * 2] = (top == left) ? top : center;
				dstEven[x * 2 + 1] = (right == top) ? right : center;
				dstOdd[x * 2] = (left == bottom) ? left : center;
				dstOdd[x * 2 + 1] = (bottom == right) ? bottom : center;
			}
			else
			{
				dstEven[x * 2] = center;
				dstEven[x * 2 + 1] = center;
				dstOdd[x * 2] = center;
				dstOdd[x * 2 + 1] = center;
			}
		}
	}
	return selectedCorner;
}

static bool matches(const char *filter, const TestSize &size,
                    const std::vector<u32> &actual,
                    const std::vector<u32> &expected,
                    int width, int height, int stride)
{
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			const size_t index = static_cast<size_t>(y) * stride + x;
			if (actual[index] != expected[index])
			{
				std::fprintf(stderr, "%s mismatch at %dx%d (%d,%d): %08X != %08X\n",
				             filter, size.width, size.height, x, y,
				             actual[index], expected[index]);
				return false;
			}
		}
		for (int x = width; x < stride; x++)
		{
			const size_t index = static_cast<size_t>(y) * stride + x;
			if (actual[index] != 0xCDCDCDCDu)
			{
				std::fprintf(stderr, "%s padding modified at %dx%d (%d,%d)\n",
				             filter, size.width, size.height, x, y);
				return false;
			}
		}
	}
	return true;
}

int main()
{
	const TestSize sizes[] = {
		{1, 1}, {2, 1}, {4, 2}, {5, 3}, {6, 5}, {7, 4}, {8, 3},
		{15, 9}, {16, 8}, {17, 7}, {31, 17}, {256, 192}
	};
	const size_t sizeCount = sizeof(sizes) / sizeof(sizes[0]);
	bool epxSelectionExercised = false;

	for (size_t s = 0; s < sizeCount; s++)
	{
		const TestSize size = sizes[s];
		const int srcStride = size.width + 2;
		std::vector<u32> source((size.height + 2) * srcStride);
		u32 *src = source.data() + srcStride + 1;

		const int dstWidth = size.width * 2;
		const int dstHeight = size.height * 2;
		const int dstStride = dstWidth + 4;
		std::vector<u32> actual(dstHeight * dstStride, 0xCDCDCDCDu);
		std::vector<u32> expected(dstHeight * dstStride, 0xCDCDCDCDu);
		SSurface srcSurface;
		std::memset(&srcSurface, 0, sizeof(srcSurface));
		srcSurface.Surface = reinterpret_cast<unsigned char *>(src);
		srcSurface.Pitch = srcStride * 2;
		srcSurface.Width = size.width;
		srcSurface.Height = size.height;
		SSurface dstSurface;
		std::memset(&dstSurface, 0, sizeof(dstSurface));
		dstSurface.Surface = reinterpret_cast<unsigned char *>(actual.data());
		dstSurface.Pitch = dstStride * 2;
		dstSurface.Width = dstWidth;
		dstSurface.Height = dstHeight;

		for (int pattern = 0; pattern < 2; pattern++)
		{
			fillSource(source, srcStride, size.width, size.height, pattern);
			RenderBilinear(srcSurface, dstSurface);
			referenceBilinear(src, srcStride, size.width, size.height,
			                  expected.data(), dstStride);
			if (!matches("Bilinear32", size, actual, expected,
			             dstWidth, dstHeight, dstStride))
				return 1;

			std::fill(actual.begin(), actual.end(), 0xCDCDCDCDu);
			std::fill(expected.begin(), expected.end(), 0xCDCDCDCDu);
			RenderEPX(srcSurface, dstSurface);
			epxSelectionExercised |= referenceEPX(
				src, srcStride, size.width, size.height, expected.data(), dstStride);
			if (!matches("EPX", size, actual, expected,
			             dstWidth, dstHeight, dstStride))
				return 1;
		}
	}

	if (!epxSelectionExercised)
	{
		std::fputs("EPX corner-selection case was not exercised.\n", stderr);
		return 1;
	}

	std::puts("Bilinear32 and EPX SIMD output matches scalar reference.");
	return 0;
}
