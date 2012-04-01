#ifdef __cplusplus

#include "xsfc.h"
class XSFTag
{
protected:
	XSFTag()
	{
	}

	~XSFTag()
	{
	}

	static unsigned long GetWordLE(const void *pData)
	{
		const unsigned char *pdata = static_cast<const unsigned char *>(pData);
		return pdata[0] | (unsigned long(pdata[1]) << 8) | (unsigned long(pdata[2]) << 16) | (unsigned long(pdata[3]) << 24);
	}

public:

	static unsigned long SearchRaw(const void *pData, size_t dwSize)
	{
		const char *pdata = static_cast<const char *>(pData);
		unsigned long dwPos;
		unsigned long dwReservedAreaSize;
		unsigned long dwProgramLength;
		unsigned long dwProgramCRC;
		if (dwSize < 16 + 5 + 1) return 0;
		if (pdata[0] != 'P') return 0;
		if (pdata[1] != 'S') return 0;
		if (pdata[2] != 'F') return 0;
		dwReservedAreaSize = GetWordLE(pdata + 4);
		dwProgramLength = GetWordLE(pdata + 8);
		dwProgramCRC = GetWordLE(pdata + 12);
		dwPos = 16 + dwReservedAreaSize + dwProgramLength;
		if (dwPos >= dwSize) return 0;
		return dwPos;
	}

	static int Search(unsigned long *pdwRet, const void *pData, size_t dwSize)
	{
		const char *pdata = static_cast<const char *>(pData);
		unsigned long dwPos = SearchRaw(pdata, dwSize);
		if (dwSize < dwPos + 5) return 0;
		if (memcmp(pdata + dwPos, "[TAG]", 5)) return 0;
		*pdwRet = dwPos + 5;
		return 1;
	}

	enum enum_callback_returnvalue
	{
		enum_continue = 0,
		enum_break = 1
	};

	typedef enum enum_callback_returnvalue (*pfnenum_callback_t)(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd);
	static int EnumRaw(pfnenum_callback_t pCallBack, void *pWork, const void *pData, size_t dwSize)
	{
		const char *pdata = static_cast<const char *>(pData);
		unsigned long dwPos = 0;
		while (dwPos < dwSize)
		{
			unsigned long dwNameTop;
			unsigned long dwNameEnd;
			unsigned long dwValueTop;
			unsigned long dwValueEnd;
			if (dwPos < dwSize && pdata[dwPos] == 0x0a) dwPos++;
			while (dwPos < dwSize && pdata[dwPos] != 0x0a && 0x00 <= pdata[dwPos] && pdata[dwPos] <= 0x20)
				dwPos++;
			if (dwPos >= dwSize || pdata[dwPos] == 0x0a) continue;
			dwNameTop = dwPos;
			while (dwPos < dwSize && pdata[dwPos] != 0x0a && pdata[dwPos] != '=')
				dwPos++;
			if (dwPos >= dwSize || pdata[dwPos] == 0x0a) continue;
			dwNameEnd = dwPos;
			while (dwNameTop < dwNameEnd &&  0x00 <= pdata[dwNameEnd - 1] && pdata[dwNameEnd - 1] <= 0x20)
				dwNameEnd--;
			if (dwPos < dwSize && pdata[dwPos] == '=') dwPos++;
			while (dwPos < dwSize && pdata[dwPos] != 0x0a && 0x00 <= pdata[dwPos] && pdata[dwPos] <= 0x20)
				dwPos++;
			dwValueTop = dwPos;
			while (dwPos < dwSize && pdata[dwPos] != 0x0a)
				dwPos++;
			dwValueEnd = dwPos;
			while (dwValueTop < dwValueEnd &&  0x00 <= pdata[dwValueEnd - 1] && pdata[dwValueEnd - 1] <= 0x20)
				dwValueEnd--;

			if (pCallBack)
			{
				if (enum_continue != pCallBack(pWork, pdata + dwNameTop, pdata + dwNameEnd, pdata + dwValueTop, pdata + dwValueEnd))
					return -1;
			}
		}
		return 1;
	}

	static int Enum(pfnenum_callback_t pCallBack, void *pWork, const void *pData, size_t dwSize)
	{
		const char *pdata = static_cast<const char *>(pData);
		unsigned long dwPos = 0;
		if (!Search(&dwPos, pdata, dwSize))
			return 0;
		return EnumRaw(pCallBack, pWork, pdata + dwPos, dwSize - dwPos);
	}

protected:

	typedef struct
	{
		int taglen;
		const char *tag;
		bool ret;
	} xsf_tagexists_work_t;

	static enum enum_callback_returnvalue enum_callback_tagexists(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd)
	{
		xsf_tagexists_work_t *pwork = static_cast<xsf_tagexists_work_t *>(pWork);
		(void)pValueTop, pValueEnd;
		if (pwork->taglen == pNameEnd - pNameTop && !_strnicmp(pNameTop, pwork->tag, pwork->taglen))
		{
			pwork->ret = true;
			return enum_break;
		}
		return enum_continue;
	}

public:

	static bool ExistsRaw(const char *tag, const void *pData, size_t dwSize)
	{
		const char *pdata = static_cast<const char *>(pData);
		xsf_tagexists_work_t work;
		work.ret = false;
		work.tag = tag;
		work.taglen = (unsigned long)strlen(tag);
		EnumRaw(enum_callback_tagexists, &work, pdata, dwSize);
		return work.ret;
	}

	static bool Exists(const char *tag, const void *pData, size_t dwSize)
	{
		const char *pdata = static_cast<const char *>(pData);
		xsf_tagexists_work_t work;
		work.ret = false;
		work.tag = tag;
		work.taglen = (unsigned long)strlen(tag);
		Enum(enum_callback_tagexists, &work, pdata, dwSize);
		return work.ret;
	}

protected:

	typedef struct
	{
		int taglen;
		const char *tag;
		xsfc::TString ret;
		bool futf8;
	} xsf_tagget_work_t;

	static enum enum_callback_returnvalue enum_callback_tagget(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd)
	{
		xsf_tagget_work_t *pwork = static_cast<xsf_tagget_work_t *>(pWork);
		if (pwork->taglen == pNameEnd - pNameTop && !_strnicmp(pNameTop, pwork->tag, pwork->taglen))
		{
			pwork->ret = xsfc::TString(pwork->futf8, pValueTop, pValueEnd - pValueTop);
			return enum_break;
		}
		return enum_continue;
	}

public:

	static xsfc::TString GetRaw(bool fUTF8, const char *tag, const void *pData, size_t dwSize)
	{
		const char *pdata = static_cast<const char *>(pData);
		xsf_tagget_work_t work;
		xsfc::TString ret;
		work.futf8 = fUTF8;
		work.tag = tag;
		work.taglen = (unsigned long)strlen(tag);
		EnumRaw(enum_callback_tagget, &work, pdata, dwSize);
		ret = work.ret;
		return ret;
	}
	static xsfc::TString GetRaw(const char *tag, const void *pData, size_t dwSize)
	{
		return GetRaw(ExistsRaw("utf8", pData, dwSize), tag, pData, dwSize);
	}

	static xsfc::TString Get(bool fUTF8, const char *tag, const void *pData, size_t dwSize)
	{
		const char *pdata = static_cast<const char *>(pData);
		xsf_tagget_work_t work;
		xsfc::TString ret;
		work.futf8 = fUTF8;
		work.tag = tag;
		work.taglen = (unsigned long)strlen(tag);
		Enum(enum_callback_tagget, &work, pdata, dwSize);
		ret = work.ret;
		return ret;
	}
	static xsfc::TString Get(const char *tag, const void *pData, size_t dwSize)
	{
		return Get(Exists("utf8", pData, dwSize), tag, pData, dwSize);
	}


	static int GetInt(const char *tag, const void *pData, size_t dwSize, int value_default)
	{
		const char *pdata = static_cast<const char *>(pData);
		int ret = value_default;
		xsfc::TString value = Get(tag, pdata, dwSize);
		if (value[0]) ret = atoi(xsfc::TStringM(value));
		return ret;
	}

	static double GetFloat(const char *tag, const void *pData, size_t dwSize, double value_default)
	{
		const char *pdata = static_cast<const char *>(pData);
		double ret = value_default;
		xsfc::TString value = Get(tag, pdata, dwSize);
		if (value[0]) ret = atof(xsfc::TStringM(value));
		return ret;
	}

	static unsigned long ToMS(const wchar_t *p)
	{
		int f = 0;
		unsigned long b = 0;
		unsigned long r = 0;
		for (;*p; p++)
		{
			if (*p >= L'0' && *p <= L'9')
			{
				if (f < 1000)
				{
					r = r * 10 + *p - L'0';
					if (f) f *= 10;
					continue;
				}
				break;
			}
			if (*p == L'.')
			{
				f = 1;
				continue;
			}
			if (*p == L':')
			{
				b = (b + r) * 60;
				r = 0;
				continue;
			}
			break;
		}
		if (f < 10)
			r *= 1000;
		else if (f == 10)
			r *= 100;
		else if (f == 100)
			r *= 10;
		r += b * 1000;
		return r;
	}

	static unsigned GetLengthMS(const void *idata, size_t isize, unsigned defaultlength)
	{
		unsigned length = 0;
		xsfc::TString taglength = XSFTag::Get("length", idata, isize);
		if (taglength[0]) length = XSFTag::ToMS(taglength);
		if (!length) length = defaultlength;
		return length;
	}
	static unsigned GetLengthMS(xsfc::TAutoBuffer<char> idata, unsigned defaultlength)
	{
		return GetLengthMS(idata.Ptr(), idata.Len(), defaultlength);
	}

	static unsigned GetFadeMS(const void *idata, size_t isize, unsigned defaultfade)
	{
		unsigned fade = defaultfade;
		xsfc::TString tagfade = XSFTag::Get("fade", idata, isize);
		if (tagfade[0]) fade = XSFTag::ToMS(tagfade);
		return fade;
	}
	static unsigned GetFadeMS(xsfc::TAutoBuffer<char> idata, unsigned defaultfade)
	{
		return GetFadeMS(idata.Ptr(), idata.Len(), defaultfade);
	}

	static bool GetVolume(float *pvolume, const char *tag, xsfc::TAutoBuffer<char> idata, int isgain)
	{
		bool hasvolume = false;
		xsfc::TString tagvolume = XSFTag::Get(tag, idata.Ptr(), idata.Len());
		if (tagvolume[0])
		{
			float value = float(tagvolume.GetFloat());
			if (isgain)
				*pvolume *= float(pow(2.0, value / 6.0));
			else
				*pvolume = value;
			hasvolume = (*pvolume != 1.0);
		}
		return !hasvolume;
	}

};
#else

static DWORD getdwordle(const BYTE *pData)
{
	return pData[0] | (((DWORD)pData[1]) << 8) | (((DWORD)pData[2]) << 16) | (((DWORD)pData[3]) << 24);
}

static DWORD xsf_tagsearchraw(const void *pData, DWORD dwSize)
{
	const BYTE *pdata = (const BYTE *)pData;
	DWORD dwPos;
	DWORD dwReservedAreaSize;
	DWORD dwProgramLength;
	DWORD dwProgramCRC;
	if (dwSize < 16 + 5 + 1) return 0;
	if (pdata[0] != 'P') return 0;
	if (pdata[1] != 'S') return 0;
	if (pdata[2] != 'F') return 0;
	dwReservedAreaSize = getdwordle(pdata + 4);
	dwProgramLength = getdwordle(pdata + 8);
	dwProgramCRC = getdwordle(pdata + 12);
	dwPos = 16 + dwReservedAreaSize + dwProgramLength;
	if (dwPos >= dwSize) return 0;
	return dwPos;
}
static int xsf_tagsearch(DWORD *pdwRet, const BYTE *pData, DWORD dwSize)
{
	DWORD dwPos = xsf_tagsearchraw(pData, dwSize);
	if (dwSize < dwPos + 5) return 0;
	if (memcmp(pData + dwPos, "[TAG]", 5)) return 0;
	*pdwRet = dwPos + 5;
	return 1;
}

enum xsf_tagenum_callback_returnvalue
{
	xsf_tagenum_callback_returnvaluecontinue = 0,
	xsf_tagenum_callback_returnvaluebreak = 1
};
typedef int (*pfnxsf_tagenum_callback_t)(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd);
static int xsf_tagenumraw(pfnxsf_tagenum_callback_t pCallBack, void *pWork, const void *pData, DWORD dwSize)
{
	const char *pdata = (const char *)pData;
	DWORD dwPos = 0;
	while (dwPos < dwSize)
	{
		DWORD dwNameTop;
		DWORD dwNameEnd;
		DWORD dwValueTop;
		DWORD dwValueEnd;
		if (dwPos < dwSize && pdata[dwPos] == 0x0a) dwPos++;
		while (dwPos < dwSize && pdata[dwPos] != 0x0a && 0x00 <= pdata[dwPos] && pdata[dwPos] <= 0x20)
			dwPos++;
		if (dwPos >= dwSize || pdata[dwPos] == 0x0a) continue;
		dwNameTop = dwPos;
		while (dwPos < dwSize && pdata[dwPos] != 0x0a && pdata[dwPos] != '=')
			dwPos++;
		if (dwPos >= dwSize || pdata[dwPos] == 0x0a) continue;
		dwNameEnd = dwPos;
		while (dwNameTop < dwNameEnd &&  0x00 <= pdata[dwNameEnd - 1] && pdata[dwNameEnd - 1] <= 0x20)
			dwNameEnd--;
		if (dwPos < dwSize && pdata[dwPos] == '=') dwPos++;
		while (dwPos < dwSize && pdata[dwPos] != 0x0a && 0x00 <= pdata[dwPos] && pdata[dwPos] <= 0x20)
			dwPos++;
		dwValueTop = dwPos;
		while (dwPos < dwSize && pdata[dwPos] != 0x0a)
			dwPos++;
		dwValueEnd = dwPos;
		while (dwValueTop < dwValueEnd &&  0x00 <= pdata[dwValueEnd - 1] && pdata[dwValueEnd - 1] <= 0x20)
			dwValueEnd--;

		if (pCallBack)
		{
			if (xsf_tagenum_callback_returnvaluecontinue != pCallBack(pWork, (const char *)pdata + dwNameTop, (const char *)pdata + dwNameEnd, (const char *)pdata + dwValueTop, (const char *)pdata + dwValueEnd))
				return -1;
		}
	}
	return 1;
}

static int xsf_tagenum(pfnxsf_tagenum_callback_t pCallBack, void *pWork, const void *pData, DWORD dwSize)
{
	const BYTE *pdata = (const BYTE *)pData;
	DWORD dwPos = 0;
	if (!xsf_tagsearch(&dwPos, pdata, dwSize))
		return 0;
	return xsf_tagenumraw(pCallBack, pWork, pdata + dwPos, dwSize - dwPos);
}

typedef struct
{
	int taglen;
	const char *tag;
	char *ret;
} xsf_tagget_work_t;

static int xsf_tagenum_callback_tagget(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd)
{
	xsf_tagget_work_t *pwork = (xsf_tagget_work_t *)pWork;
	if (pwork->taglen == pNameEnd - pNameTop && !_strnicmp(pNameTop, pwork->tag, pwork->taglen))
	{
		char *ret = (char *)malloc(pValueEnd - pValueTop + 1);
		if (!ret) return xsf_tagenum_callback_returnvaluecontinue;
		memcpy(ret, pValueTop, pValueEnd - pValueTop);
		ret[pValueEnd - pValueTop] = 0;
		pwork->ret = ret;
		return xsf_tagenum_callback_returnvaluebreak;
	}
	return xsf_tagenum_callback_returnvaluecontinue;
}

static char *xsf_taggetraw(const char *tag, const void *pData, DWORD dwSize)
{
	const BYTE *pdata = (const BYTE *)pData;
	xsf_tagget_work_t work;
	work.ret = 0;
	work.tag = tag;
	work.taglen = (DWORD)strlen(tag);
	xsf_tagenumraw(xsf_tagenum_callback_tagget, &work, pdata, dwSize);
	return work.ret;
}

static char *xsf_tagget(const char *tag, const void *pData, DWORD dwSize)
{
	const BYTE *pdata = (const BYTE *)pData;
	xsf_tagget_work_t work;
	work.ret = 0;
	work.tag = tag;
	work.taglen = (DWORD)strlen(tag);
	xsf_tagenum(xsf_tagenum_callback_tagget, &work, pdata, dwSize);
	return work.ret;
}

static int xsf_tagget_exist(const char *tag, const void *pData, DWORD dwSize)
{
	const BYTE *pdata = (const BYTE *)pData;
	int exists;
	char *value = xsf_tagget(tag, pdata, dwSize);
	if (value)
	{
		exists = 1;
		free(value);
	}
	else
	{
		exists = 0;
	}
	return exists;
}

static int xsf_tagget_int(const char *tag, const BYTE *pData, DWORD dwSize, int value_default)
{
	int ret = value_default;
	char *value = xsf_tagget(tag, pData, dwSize);
	if (value)
	{
		if (*value) ret = atoi(value);
		free(value);
	}
	return ret;
}

static double xsf_tagget_float(const char *tag, const BYTE *pData, DWORD dwSize, double value_default)
{
	double ret = value_default;
	char *value = xsf_tagget(tag, pData, dwSize);
	if (value)
	{
		if (*value) ret = atof(value);
		free(value);
	}
	return ret;
}

static DWORD tag2ms(const char *p)
{
	int f = 0;
	DWORD b = 0;
	DWORD r = 0;
	for (;*p; p++)
	{
		if (*p >= '0' && *p <= '9')
		{
			if (f < 1000)
			{
				r = r * 10 + *p - '0';
				if (f) f *= 10;
				continue;
			}
			break;
		}
		if (*p == '.')
		{
			f = 1;
			continue;
		}
		if (*p == ':')
		{
			b = (b + r) * 60;
			r = 0;
			continue;
		}
		break;
	}
	if (f < 10)
		r *= 1000;
	else if (f == 10)
		r *= 100;
	else if (f == 100)
		r *= 10;
	r += b * 1000;
	return r;
}

#endif
