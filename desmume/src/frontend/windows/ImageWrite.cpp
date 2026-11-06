#include "ImageWrite.h"

// Decide whether this translation unit can see the WIC headers.  MSVC always
// ships them with the Windows SDK -- including the *_xp toolsets, which predate
// __has_include -- so enable WIC there unconditionally.  MinGW-w64 ships them
// too, but older toolchains may not, so probe with __has_include.  The build
// can force either decision with DESMUME_ENABLE_WIC=0/1.
#if !defined(DESMUME_ENABLE_WIC)
# if defined(_MSC_VER)
#  define DESMUME_ENABLE_WIC 1
# elif defined(__has_include)
#  if __has_include(<wincodec.h>) && __has_include(<initguid.h>) && __has_include(<ocidl.h>)
#   define DESMUME_ENABLE_WIC 1
#  endif
# endif
#endif
#if !defined(DESMUME_ENABLE_WIC)
# define DESMUME_ENABLE_WIC 0
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if DESMUME_ENABLE_WIC
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Emit the WIC GUIDs (container formats, CLSIDs, IIDs) into this translation
// unit so we do not depend on uuid.lib / windowscodecs.lib exporting them.
#include <initguid.h>
#include <wincodec.h>
#include <ocidl.h>

// The SDK declares these two IIDs without a value and leaves windowscodecs.lib
// to supply them.  Referencing that library would put windowscodecs.dll into
// the import table of the exe, and the exe would then refuse to start on XP,
// where the DLL does not exist at all.  Defining them here keeps WIC a run time
// dependency: on XP CoCreateInstance simply fails and the caller falls back.
DEFINE_GUID(IID_IWICImagingFactory,
            0xec5ec8a9, 0xc395, 0x4314, 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70);
DEFINE_GUID(IID_IWICBitmapEncoderInfo,
            0x94c9b4ee, 0xa09f, 0x4f92, 0x8a, 0x1e, 0x4a, 0x9b, 0xce, 0x7e, 0x76, 0xfb);

#ifdef _MSC_VER
#pragma comment(lib, "ole32.lib")
#endif
#elif defined(_WIN32)
// Only needed for the debug output fallback in logMessage().
#include <windows.h>
#endif

namespace
{

ImageWriteLogFunc s_logger = NULL;

// Set by the last write: true when the codec itself is the problem, not the
// image or the file.
bool s_encoderUnavailable = false;

void logMessage(const char *format, ...)
{
	char    message[512];
	va_list args;

	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	message[sizeof(message) - 1] = '\0';

	if (s_logger != NULL)
	{
		s_logger(message);
		return;
	}

#ifdef _WIN32
	OutputDebugStringA("ImageWrite: ");
	OutputDebugStringA(message);
	OutputDebugStringA("\n");
#endif
}

} // namespace

void imageWriteSetLogger(ImageWriteLogFunc logger)
{
	s_logger = logger;
}

bool imageWriteEncoderUnavailable()
{
	return s_encoderUnavailable;
}

#if DESMUME_ENABLE_WIC

namespace
{

const int kMaxFormats = 16;

struct FormatEntry
{
	GUID container;
	char name[64];
	char extensions[256];
};

// Filled once by enumerateFormats(); the count stays negative until then.
FormatEntry s_formats[kMaxFormats];
int         s_formatCount = -1;
int         s_capture     = -1;

// Whether this Windows has WIC at all; XP does not.
bool s_wicAvailable = false;

// The WIC facility has almost no message table, so a bare code is all Windows
// would give us; naming the codes it does define is what makes them searchable.
const char *hresultName(HRESULT hr)
{
	switch ((long)hr)
	{
	case WINCODEC_ERR_WRONGSTATE:                       return "WINCODEC_ERR_WRONGSTATE";
	case WINCODEC_ERR_VALUEOUTOFRANGE:                  return "WINCODEC_ERR_VALUEOUTOFRANGE";
	case WINCODEC_ERR_UNKNOWNIMAGEFORMAT:               return "WINCODEC_ERR_UNKNOWNIMAGEFORMAT";
	case WINCODEC_ERR_UNSUPPORTEDVERSION:               return "WINCODEC_ERR_UNSUPPORTEDVERSION";
	case WINCODEC_ERR_NOTINITIALIZED:                   return "WINCODEC_ERR_NOTINITIALIZED";
	case WINCODEC_ERR_ALREADYLOCKED:                    return "WINCODEC_ERR_ALREADYLOCKED";
	case WINCODEC_ERR_PROPERTYNOTFOUND:                 return "WINCODEC_ERR_PROPERTYNOTFOUND";
	case WINCODEC_ERR_PROPERTYNOTSUPPORTED:             return "WINCODEC_ERR_PROPERTYNOTSUPPORTED";
	case WINCODEC_ERR_PROPERTYSIZE:                     return "WINCODEC_ERR_PROPERTYSIZE";
	case WINCODEC_ERR_CODECPRESENT:                     return "WINCODEC_ERR_CODECPRESENT";
	case WINCODEC_ERR_CODECNOTHUMBNAIL:                 return "WINCODEC_ERR_CODECNOTHUMBNAIL";
	case WINCODEC_ERR_PALETTEUNAVAILABLE:               return "WINCODEC_ERR_PALETTEUNAVAILABLE";
	case WINCODEC_ERR_CODECTOOMANYSCANLINES:            return "WINCODEC_ERR_CODECTOOMANYSCANLINES";
	case WINCODEC_ERR_INTERNALERROR:                    return "WINCODEC_ERR_INTERNALERROR";
	case WINCODEC_ERR_SOURCERECTDOESNOTMATCHDIMENSIONS: return "WINCODEC_ERR_SOURCERECTDOESNOTMATCHDIMENSIONS";
	case WINCODEC_ERR_COMPONENTNOTFOUND:                return "WINCODEC_ERR_COMPONENTNOTFOUND";
	case WINCODEC_ERR_IMAGESIZEOUTOFRANGE:              return "WINCODEC_ERR_IMAGESIZEOUTOFRANGE";
	case WINCODEC_ERR_TOOMUCHMETADATA:                  return "WINCODEC_ERR_TOOMUCHMETADATA";
	case WINCODEC_ERR_BADIMAGE:                         return "WINCODEC_ERR_BADIMAGE";
	case WINCODEC_ERR_BADHEADER:                        return "WINCODEC_ERR_BADHEADER";
	case WINCODEC_ERR_FRAMEMISSING:                     return "WINCODEC_ERR_FRAMEMISSING";
	case WINCODEC_ERR_BADMETADATAHEADER:                return "WINCODEC_ERR_BADMETADATAHEADER";
	case WINCODEC_ERR_BADSTREAMDATA:                    return "WINCODEC_ERR_BADSTREAMDATA";
	case WINCODEC_ERR_STREAMWRITE:                      return "WINCODEC_ERR_STREAMWRITE";
	case WINCODEC_ERR_STREAMREAD:                       return "WINCODEC_ERR_STREAMREAD";
	case WINCODEC_ERR_STREAMNOTAVAILABLE:               return "WINCODEC_ERR_STREAMNOTAVAILABLE";
	case WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT:           return "WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT";
	case WINCODEC_ERR_UNSUPPORTEDOPERATION:             return "WINCODEC_ERR_UNSUPPORTEDOPERATION";
	case WINCODEC_ERR_INVALIDREGISTRATION:              return "WINCODEC_ERR_INVALIDREGISTRATION";
	case WINCODEC_ERR_COMPONENTINITIALIZEFAILURE:       return "WINCODEC_ERR_COMPONENTINITIALIZEFAILURE";
	case WINCODEC_ERR_INSUFFICIENTBUFFER:               return "WINCODEC_ERR_INSUFFICIENTBUFFER";
	case WINCODEC_ERR_DUPLICATEMETADATAPRESENT:         return "WINCODEC_ERR_DUPLICATEMETADATAPRESENT";
	case WINCODEC_ERR_PROPERTYUNEXPECTEDTYPE:           return "WINCODEC_ERR_PROPERTYUNEXPECTEDTYPE";
	case WINCODEC_ERR_UNEXPECTEDSIZE:                   return "WINCODEC_ERR_UNEXPECTEDSIZE";
	case WINCODEC_ERR_INVALIDQUERYREQUEST:              return "WINCODEC_ERR_INVALIDQUERYREQUEST";
	case WINCODEC_ERR_UNEXPECTEDMETADATATYPE:           return "WINCODEC_ERR_UNEXPECTEDMETADATATYPE";
	case WINCODEC_ERR_REQUESTONLYVALIDATMETADATAROOT:   return "WINCODEC_ERR_REQUESTONLYVALIDATMETADATAROOT";
	case WINCODEC_ERR_INVALIDQUERYCHARACTER:            return "WINCODEC_ERR_INVALIDQUERYCHARACTER";
	case WINCODEC_ERR_WIN32ERROR:                       return "WINCODEC_ERR_WIN32ERROR";
	case WINCODEC_ERR_INVALIDPROGRESSIVELEVEL:          return "WINCODEC_ERR_INVALIDPROGRESSIVELEVEL";
	}

	return NULL;
}

// The code, its WIC name when it has one, and whatever text Windows has for it.
void formatHresult(HRESULT hr, char *out, size_t outSize)
{
	const char *name    = hresultName(hr);
	char        text[256] = "";

	if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	                   NULL, (DWORD)hr, 0, text, sizeof(text), NULL) != 0)
	{
		size_t length = strlen(text);
		while (length > 0 && (text[length - 1] == '\r' || text[length - 1] == '\n' ||
		                      text[length - 1] == '.'  || text[length - 1] == ' '))
			text[--length] = '\0';
	}

	if (name != NULL && text[0] != '\0')
		snprintf(out, outSize, "0x%08X %s (%s)", (unsigned int)hr, name, text);
	else if (name != NULL)
		snprintf(out, outSize, "0x%08X %s", (unsigned int)hr, name);
	else if (text[0] != '\0')
		snprintf(out, outSize, "0x%08X (%s)", (unsigned int)hr, text);
	else
		snprintf(out, outSize, "0x%08X", (unsigned int)hr);
}

// Codes that mean the codec is missing or cannot start, rather than the image
// or the file being wrong.
bool isEncoderUnavailable(HRESULT hr)
{
	return hr == WINCODEC_ERR_COMPONENTNOTFOUND ||
	       hr == WINCODEC_ERR_COMPONENTINITIALIZEFAILURE ||
	       hr == WINCODEC_ERR_INVALIDREGISTRATION ||
	       hr == REGDB_E_CLASSNOTREG ||
	       hr == HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND) ||
	       hr == HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
}

// Records a failure in the trace: the file, the stage that failed and the code
// are what make an encoder refusal readable.
void failWrite(const char *fileName, const char *stage, HRESULT hr)
{
	char code[320];

	formatHresult(hr, code, sizeof(code));
	logMessage("ImageWrite: %s failed for '%s': %s", stage, fileName, code);

	if (isEncoderUnavailable(hr))
	{
		s_encoderUnavailable = true;
		logMessage("ImageWrite: the codec for this format is missing or will not start");
	}
}

void failWriteText(const char *fileName, const char *stage, const char *text)
{
	logMessage("ImageWrite: %s failed for '%s': %s", stage, fileName, text);
}

// Names for the pixel formats that turn up when negotiating with an encoder,
// with the raw GUID as the fallback.
const char *pixelFormatName(const GUID &format, char *buffer, size_t size)
{
	struct Known
	{
		const GUID *format;
		const char *name;
	};

	static const Known known[] =
	{
		{ &GUID_WICPixelFormat24bppBGR,    "24bppBGR"    },
		{ &GUID_WICPixelFormat24bppRGB,    "24bppRGB"    },
		{ &GUID_WICPixelFormat32bppBGR,    "32bppBGR"    },
		{ &GUID_WICPixelFormat32bppBGRA,   "32bppBGRA"   },
		{ &GUID_WICPixelFormat32bppPBGRA,  "32bppPBGRA"  },
		{ &GUID_WICPixelFormat48bppRGB,    "48bppRGB"    },
		{ &GUID_WICPixelFormat64bppRGBA,   "64bppRGBA"   },
		{ &GUID_WICPixelFormat16bppGray,   "16bppGray"   },
		{ &GUID_WICPixelFormat8bppIndexed, "8bppIndexed" },
		{ &GUID_WICPixelFormat8bppGray,    "8bppGray"    },
		{ &GUID_WICPixelFormat8bppY,       "8bppY"       },
		{ &GUID_WICPixelFormat8bppCb,      "8bppCb"      },
		{ &GUID_WICPixelFormat8bppCr,      "8bppCr"      },
	};

	for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); ++i)
	{
		if (IsEqualGUID(format, *known[i].format))
			return known[i].name;
	}

	snprintf(buffer, size, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
	         (unsigned long)format.Data1, format.Data2, format.Data3,
	         format.Data4[0], format.Data4[1], format.Data4[2], format.Data4[3],
	         format.Data4[4], format.Data4[5], format.Data4[6], format.Data4[7]);

	return buffer;
}

// The emulator does not otherwise initialise COM on these threads; tolerate a
// caller that already did.  *uninit is set when this call is the one that has
// to be balanced with CoUninitialize().
bool beginCom(bool *uninit)
{
	*uninit = false;

	const HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (hr == S_OK || hr == S_FALSE)
	{
		*uninit = true;
		return true;
	}

	return hr == RPC_E_CHANGED_MODE;
}

bool isSeparator(char c)
{
	return c == ',' || c == ';' || c == '|';
}

void toAnsi(const WCHAR *wide, char *out, size_t outSize)
{
	if (outSize == 0)
		return;

	if (wide == NULL || WideCharToMultiByte(CP_ACP, 0, wide, -1, out, (int)outSize, NULL, NULL) <= 0)
		out[0] = '\0';
}

void firstExtension(const char *list, char *out, size_t outSize)
{
	if (outSize == 0)
		return;

	size_t n = 0;
	while (list[n] != '\0' && !isSeparator(list[n]) && n + 1 < outSize)
	{
		out[n] = list[n];
		++n;
	}
	out[n] = '\0';
}

bool sameExtension(const char *a, size_t aLength, const char *b)
{
	const size_t bLength = strlen(b);
	if (aLength != bLength)
		return false;

	for (size_t i = 0; i < aLength; ++i)
	{
		char ca = a[i];
		char cb = b[i];
		if (ca >= 'A' && ca <= 'Z')
			ca = (char)(ca - 'A' + 'a');
		if (cb >= 'A' && cb <= 'Z')
			cb = (char)(cb - 'A' + 'a');
		if (ca != cb)
			return false;
	}

	return true;
}

// True when the comma separated `list` contains `extension`.
bool extensionListContains(const char *list, const char *extension)
{
	if (extension == NULL || *extension == '\0')
		return false;

	for (const char *p = list; *p != '\0'; )
	{
		const char *end = p;
		while (*end != '\0' && !isSeparator(*end))
			++end;

		const char *token = p;
		size_t      length = (size_t)(end - p);
		while (length > 0 && token[0] == ' ')
		{
			++token;
			--length;
		}
		while (length > 0 && token[length - 1] == ' ')
			--length;

		if (sameExtension(token, length, extension))
			return true;

		p = (*end == '\0') ? end : end + 1;
	}

	return false;
}

// Collects the containers the local WIC can encode.  PNG is moved to the front
// so the default leads the menu.
void enumerateFormats()
{
	if (s_formatCount >= 0)
		return;

	s_formatCount = 0;

	bool uninitCom = false;
	if (!beginCom(&uninitCom))
	{
		logMessage("ImageWrite: no WIC encoders, COM is not usable on this thread");
		return;
	}

	IWICImagingFactory *factory = NULL;
	if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
	                               IID_IWICImagingFactory, (void **)&factory)))
	{
		s_wicAvailable = true;

		IEnumUnknown *enumerator = NULL;
		if (SUCCEEDED(factory->CreateComponentEnumerator(WICEncoder, WICComponentEnumerateDefault,
		                                                 &enumerator)))
		{
			IUnknown *unknown = NULL;
			ULONG     got     = 0;

			while (s_formatCount < kMaxFormats && enumerator->Next(1, &unknown, &got) == S_OK)
			{
				IWICBitmapEncoderInfo *info = NULL;
				if (SUCCEEDED(unknown->QueryInterface(IID_IWICBitmapEncoderInfo, (void **)&info)))
				{
					FormatEntry &entry = s_formats[s_formatCount];
					WCHAR       wide[256];
					UINT        length = 0;

					if (SUCCEEDED(info->GetContainerFormat(&entry.container)) &&
					    SUCCEEDED(info->GetFriendlyName(256, wide, &length)))
					{
						toAnsi(wide, entry.name, sizeof(entry.name));
						if (SUCCEEDED(info->GetFileExtensions(256, wide, &length)))
						{
							toAnsi(wide, entry.extensions, sizeof(entry.extensions));

							// WIC reports some of these in upper case (".JXL"),
							// which looks wrong next to the rest of the names.
							for (char *p = entry.extensions; *p != '\0'; ++p)
								if (*p >= 'A' && *p <= 'Z')
									*p = (char)(*p - 'A' + 'a');
						}
						else
							entry.extensions[0] = '\0';

						// Without an extension neither naming nor selecting a
						// capture would work, so leave such encoders out.
						if (entry.extensions[0] != '\0')
							++s_formatCount;
					}

					info->Release();
				}

				unknown->Release();
			}

			enumerator->Release();
		}
		else
			logMessage("ImageWrite: the WIC encoder enumerator could not be created");

		factory->Release();
	}
	else
		logMessage("ImageWrite: CLSID_WICImagingFactory could not be created, this Windows has no WIC (XP has none)");

	if (uninitCom)
		CoUninitialize();

	for (int i = 1; i < s_formatCount; ++i)
	{
		if (IsEqualGUID(s_formats[i].container, GUID_ContainerFormatPng))
		{
			const FormatEntry swap = s_formats[0];
			s_formats[0]           = s_formats[i];
			s_formats[i]           = swap;
			break;
		}
	}

	logMessage("ImageWrite: %d encoder(s) available", s_formatCount);
	for (int i = 0; i < s_formatCount; ++i)
		logMessage("ImageWrite:   %d %s [%s]", i, s_formats[i].name, s_formats[i].extensions);

	imageCaptureSelect(".png");
}

// The container the writers use.  PNG when nothing could be enumerated.
const GUID &captureContainer()
{
	enumerateFormats();
	return s_capture >= 0 ? s_formats[s_capture].container : GUID_ContainerFormatPng;
}

// The name of the container above, for the trace.
const char *captureContainerName()
{
	enumerateFormats();
	return s_capture >= 0 ? s_formats[s_capture].name : "PNG";
}

// Writes the frame in `container`.  Every step is reported: which one failed
// and with what code is the only thing that makes an encoder refusal readable.
bool writeBGRToFile(const char *fileName, const GUID &container, const char *containerName,
                    int width, int height, const unsigned char *bgr, int stride)
{
	s_encoderUnavailable = false;

	if (fileName == NULL || bgr == NULL || width <= 0 || height <= 0 || stride <= 0)
	{
		logMessage("ImageWrite: refusing to write '%s': invalid image (%dx%d, stride %d)",
		           fileName ? fileName : "(null)", width, height, stride);
		return false;
	}

	logMessage("ImageWrite: writing '%s' %dx%d as %s", fileName, width, height, containerName);

	bool uninitCom = false;
	if (!beginCom(&uninitCom))
	{
		failWriteText(fileName, "CoInitializeEx", "COM is not usable on this thread");
		return false;
	}

	// The project targets the MultiByte character set, so convert the path.
	wchar_t *wfileName = NULL;
	{
		const int wlen = MultiByteToWideChar(CP_ACP, 0, fileName, -1, NULL, 0);
		if (wlen > 0)
		{
			wfileName = new wchar_t[wlen];
			if (MultiByteToWideChar(CP_ACP, 0, fileName, -1, wfileName, wlen) == 0)
			{
				delete[] wfileName;
				wfileName = NULL;
			}
		}
	}

	IWICImagingFactory *    factory   = NULL;
	IWICStream *            stream    = NULL;
	IWICBitmapEncoder *     encoder   = NULL;
	IWICBitmapFrameEncode * frame     = NULL;
	IPropertyBag2 *         props     = NULL;
	IWICBitmap *            bitmap    = NULL;
	IWICFormatConverter *   converter = NULL;
	IWICPalette *           palette   = NULL;

	bool ok = false;

	do
	{
		if (wfileName == NULL)
		{
			failWriteText(fileName, "MultiByteToWideChar",
			              "the file name cannot be converted to UTF-16");
			break;
		}

		HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
		                              IID_IWICImagingFactory, (void **)&factory);
		if (FAILED(hr))
		{
			failWrite(fileName, "CoCreateInstance(CLSID_WICImagingFactory)", hr);
			break;
		}

		hr = factory->CreateStream(&stream);
		if (FAILED(hr))
		{
			failWrite(fileName, "IWICImagingFactory::CreateStream", hr);
			break;
		}

		hr = stream->InitializeFromFilename(wfileName, GENERIC_WRITE);
		if (FAILED(hr))
		{
			failWrite(fileName, "IWICStream::InitializeFromFilename", hr);
			break;
		}

		hr = factory->CreateEncoder(container, NULL, &encoder);
		if (FAILED(hr))
		{
			failWrite(fileName, "IWICImagingFactory::CreateEncoder", hr);
			break;
		}

		hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
		if (FAILED(hr))
		{
			failWrite(fileName, "IWICBitmapEncoder::Initialize", hr);
			break;
		}

		hr = encoder->CreateNewFrame(&frame, &props);
		if (FAILED(hr))
		{
			failWrite(fileName, "IWICBitmapEncoder::CreateNewFrame", hr);
			break;
		}

		hr = frame->Initialize(props);
		if (FAILED(hr))
		{
			failWrite(fileName, "IWICBitmapFrameEncode::Initialize", hr);
			break;
		}

		if (IsEqualGUID(container, GUID_ContainerFormatJpeg))
		{
			static OLECHAR qualityName[] = L"ImageQuality";
			PROPBAG2       option;
			VARIANT        value;
			memset(&option, 0, sizeof(option));
			option.pstrName = qualityName;
			VariantInit(&value);
			value.vt     = VT_R4;
			value.fltVal = 0.95f;
			props->Write(1, &option, &value);
			VariantClear(&value);
		}

		hr = frame->SetSize((UINT)width, (UINT)height);
		if (FAILED(hr))
		{
			failWrite(fileName, "IWICBitmapFrameEncode::SetSize", hr);
			break;
		}

		// The encoder answers with the closest pixel format it can take, which
		// is not always the one asked for: GIF only takes indexed 8bpp, DDS and
		// JPEG XL want their own RGB layout, HEIF wants planar YCbCr.  Convert
		// when the format is merely different, and say what was wanted when it
		// cannot be produced at all.
		WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat24bppBGR;
		hr = frame->SetPixelFormat(&pixelFormat);
		if (FAILED(hr))
		{
			failWrite(fileName, "IWICBitmapFrameEncode::SetPixelFormat", hr);
			break;
		}

		if (IsEqualGUID(pixelFormat, GUID_WICPixelFormat24bppBGR))
		{
			hr = frame->WritePixels((UINT)height, (UINT)stride, (UINT)(stride * height),
			                        (BYTE *)bgr);
			if (FAILED(hr))
			{
				failWrite(fileName, "IWICBitmapFrameEncode::WritePixels", hr);
				break;
			}
		}
		else
		{
			char wanted[80];
			pixelFormatName(pixelFormat, wanted, sizeof(wanted));
			logMessage("ImageWrite: the encoder asks for %s", wanted);

			hr = factory->CreateBitmapFromMemory((UINT)width, (UINT)height,
			                                     GUID_WICPixelFormat24bppBGR,
			                                     (UINT)stride, (UINT)(stride * height),
			                                     (BYTE *)bgr, &bitmap);
			if (FAILED(hr))
			{
				failWrite(fileName, "IWICImagingFactory::CreateBitmapFromMemory", hr);
				break;
			}

			hr = factory->CreateFormatConverter(&converter);
			if (FAILED(hr))
			{
				failWrite(fileName, "IWICImagingFactory::CreateFormatConverter", hr);
				break;
			}

			if (IsEqualGUID(pixelFormat, GUID_WICPixelFormat8bppIndexed))
			{
				// Indexed output needs a palette for the conversion and again
				// for the frame; let WIC pick an optimal one from the image.
				hr = factory->CreatePalette(&palette);
				if (FAILED(hr))
				{
					failWrite(fileName, "IWICImagingFactory::CreatePalette", hr);
					break;
				}

				hr = palette->InitializeFromBitmap(bitmap, 256, FALSE);
				if (FAILED(hr))
				{
					failWrite(fileName, "IWICPalette::InitializeFromBitmap", hr);
					break;
				}

				hr = converter->Initialize(bitmap, pixelFormat, WICBitmapDitherTypeErrorDiffusion,
				                           palette, 0.0, WICBitmapPaletteTypeCustom);
				if (FAILED(hr))
				{
					failWrite(fileName, "IWICFormatConverter::Initialize", hr);
					break;
				}

				hr = frame->SetPalette(palette);
				if (FAILED(hr))
				{
					failWrite(fileName, "IWICBitmapFrameEncode::SetPalette", hr);
					break;
				}
			}
			else
			{
				BOOL canConvert = FALSE;
				hr = converter->CanConvert(GUID_WICPixelFormat24bppBGR, pixelFormat, &canConvert);
				if (FAILED(hr) || !canConvert)
				{
					char text[256];
					snprintf(text, sizeof(text),
					         "the encoder only takes %s, which cannot be made from 24bppBGR",
					         wanted);
					failWriteText(fileName, "IWICFormatConverter::CanConvert", text);
					break;
				}

				hr = converter->Initialize(bitmap, pixelFormat, WICBitmapDitherTypeNone,
				                           NULL, 0.0, WICBitmapPaletteTypeCustom);
				if (FAILED(hr))
				{
					failWrite(fileName, "IWICFormatConverter::Initialize", hr);
					break;
				}
			}

			hr = frame->WriteSource(converter, NULL);
			if (FAILED(hr))
			{
				failWrite(fileName, "IWICBitmapFrameEncode::WriteSource", hr);
				break;
			}
		}

		hr = frame->Commit();
		if (FAILED(hr))
		{
			failWrite(fileName, "IWICBitmapFrameEncode::Commit", hr);
			break;
		}

		hr = encoder->Commit();
		if (FAILED(hr))
		{
			failWrite(fileName, "IWICBitmapEncoder::Commit", hr);
			break;
		}

		ok = true;
	}
	while (false);

	if (palette != NULL)
		palette->Release();
	if (converter != NULL)
		converter->Release();
	if (bitmap != NULL)
		bitmap->Release();
	if (frame != NULL)
		frame->Release();
	if (props != NULL)
		props->Release();
	if (encoder != NULL)
		encoder->Release();
	if (stream != NULL)
		stream->Release();
	if (factory != NULL)
		factory->Release();
	delete[] wfileName;

	if (uninitCom)
		CoUninitialize();

	if (ok)
		logMessage("ImageWrite: wrote '%s' as %s", fileName, containerName);

	return ok;
}

} // namespace

bool imageWriteAvailable()
{
	enumerateFormats();
	return s_wicAvailable;
}

int imageWriteFormats(ImageWriteFormatInfo *out, int max)
{
	enumerateFormats();

	if (out == NULL || max <= 0)
		return s_formatCount;

	int count = s_formatCount < max ? s_formatCount : max;
	for (int i = 0; i < count; ++i)
	{
		strncpy(out[i].name, s_formats[i].name, sizeof(out[i].name) - 1);
		out[i].name[sizeof(out[i].name) - 1] = '\0';
		strncpy(out[i].extensions, s_formats[i].extensions, sizeof(out[i].extensions) - 1);
		out[i].extensions[sizeof(out[i].extensions) - 1] = '\0';
	}

	return count;
}

int imageCaptureFormatIndex()
{
	enumerateFormats();
	return s_capture;
}

const char *imageCaptureExtension()
{
	enumerateFormats();

	static char extension[64];
	if (s_capture >= 0)
		firstExtension(s_formats[s_capture].extensions, extension, sizeof(extension));
	else
		strcpy(extension, ".png");

	return extension;
}

bool imageCaptureSelect(const char *extension)
{
	enumerateFormats();

	if (extension == NULL || *extension == '\0')
		return false;

	for (int i = 0; i < s_formatCount; ++i)
	{
		if (extensionListContains(s_formats[i].extensions, extension))
		{
			s_capture = i;
			return true;
		}
	}

	logMessage("ImageWrite: no encoder claims '%s', keeping '%s'", extension,
	           s_capture >= 0 ? s_formats[s_capture].extensions : ".png");

	return false;
}

bool imageCaptureSelectIndex(int index)
{
	enumerateFormats();

	if (index < 0 || index >= s_formatCount)
	{
		logMessage("ImageWrite: format %d is out of range, keeping '%s'", index,
		           s_capture >= 0 ? s_formats[s_capture].extensions : ".png");
		return false;
	}

	s_capture = index;
	logMessage("ImageWrite: capture format is now %s", s_formats[s_capture].name);

	return true;
}

bool utilWriteImageBGR(const char *fileName, int width, int height,
                       const unsigned char *bgr, int stride)
{
	return writeBGRToFile(fileName, captureContainer(), captureContainerName(),
	                      width, height, bgr, stride);
}

#else // !DESMUME_ENABLE_WIC

// No WIC headers at build time (or explicitly disabled).  Keep the API so the
// front end needs no conditional compilation: every call reports "unavailable"
// and the caller falls back to the bundled rpng/rbmp writers.

bool imageWriteAvailable()
{
	return false;
}

int imageWriteFormats(ImageWriteFormatInfo *out, int max)
{
	if (out != NULL && max > 0)
		out[0].name[0] = '\0';
	return 0;
}

int imageCaptureFormatIndex()
{
	return -1;
}

const char *imageCaptureExtension()
{
	return ".png";
}

bool imageCaptureSelect(const char *extension)
{
	return false;
}

bool imageCaptureSelectIndex(int index)
{
	return false;
}

bool utilWriteImageBGR(const char *fileName, int width, int height,
                       const unsigned char *bgr, int stride)
{
	logMessage("ImageWrite: WIC was not built in, falling back for '%s'",
	           fileName ? fileName : "(null)");
	return false;
}

#endif // DESMUME_ENABLE_WIC
