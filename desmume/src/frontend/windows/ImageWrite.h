#ifndef DESMUME_IMAGE_WRITE_H
#define DESMUME_IMAGE_WRITE_H

// Screenshot encoding through the Windows Imaging Component (WIC).
//
// WIC is a run time dependency, not a link time one: ImageWrite.cpp defines the
// GUIDs it needs itself instead of linking windowscodecs.lib, so the executable
// still starts on XP (where WIC is absent) and simply reports that no WIC
// factory could be created.  Callers must fall back to the bundled rpng/rbmp
// writers when imageWriteAvailable() is false or a write fails.
//
// The format list is whatever WIC reports at run time, not a hardcoded set, so
// a screenshot can be saved in any format the machine has an encoder for.
//
// DeSmuME does NOT use utilWriteImageFromDisplay() from the original standalone
// writer: that helper assumes a display buffer with an over-read border row and
// a +4 byte row pitch.  GPU.h's NDSDisplayInfo::masterCustomBuffer is a tightly
// packed customWidth * customHeight * pixelBytes buffer with no border, so the
// front end builds a top-down 24bppBGR buffer with its own colorspacehandler
// and calls utilWriteImageBGR().  That also keeps WIC screenshots pixel-identical
// to the ones the bundled writers produce.

// One container a WIC encoder can write.
struct ImageWriteFormatInfo
{
	char name[64];         // localized friendly name, e.g. "PNG Encoder"
	char extensions[256];  // comma separated, e.g. ".jpeg,.jpe,.jpg"
};

// Enumerates the WIC encoders, building the list on first use and caching it.
// Returns the number of formats (all of them when `out` is NULL) and fills at
// most `max` entries otherwise.  PNG is moved to the front as the default.
int imageWriteFormats(ImageWriteFormatInfo *out, int max);

// False when this Windows has no WIC at all (XP), or when WIC headers were not
// available at build time.  Callers grey the extra formats out and fall back.
bool imageWriteAvailable();

// Index into the enumeration above of the capture format, or -1 when no format
// could be selected (which makes the writer fall back to PNG).
int imageCaptureFormatIndex();

// The extension used to name captures, e.g. ".png".
const char *imageCaptureExtension();

// Selects the capture format by any of its extensions (e.g. ".png", ".jpeg").
// Returns false and keeps the current selection if no encoder claims it.
bool imageCaptureSelect(const char *extension);

// Selects the capture format by its index in the enumeration above.
bool imageCaptureSelectIndex(int index);

// Writes a top-down 24-bit BGR image in the currently selected capture format.
bool utilWriteImageBGR(const char *fileName, int width, int height,
                       const unsigned char *bgr, int stride);

// Diagnostics.  Encoders are picky (GIF only takes indexed 8bpp, HEIF wants
// planar YCbCr, ...), so every failing step reports through this callback.
// Without a logger the messages go to OutputDebugStringA on Windows.
typedef void (*ImageWriteLogFunc)(const char *message);
void imageWriteSetLogger(ImageWriteLogFunc logger);

// True when the last write failed because the codec for the chosen format is
// missing or would not start, as opposed to a bad path or a refused pixel
// format.
bool imageWriteEncoderUnavailable();

#endif // DESMUME_IMAGE_WRITE_H
