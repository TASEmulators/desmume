extern unsigned long dwInterpolation;

#include "foobar2000/SDK/foobar2000.h"

#include "../pversion.h"

#include "../loadpe/loadpe.h"
#include "xsfdrv.h"

#include "tagget.h"
#include "xsfcfg.h"

enum
{
	xsf_bits_per_sample = 16,
	xsf_channels = 2,
#ifndef XSFDRIVER_SAMPLERATE
	xsf_sample_rate = 44100,
#else
	xsf_sample_rate = XSFDRIVER_SAMPLERATE,
#endif

	xsf_bytes_per_sample = xsf_bits_per_sample / 8,
	xsf_total_sample_width = xsf_bytes_per_sample * xsf_channels
};

#define DEFAULT_BUFFER_SIZE (1024)

#define CHECK_SILENCE_BIAS 0x8000000
#ifndef CHECK_SILENCE_LEVEL
#define CHECK_SILENCE_LEVEL 7
#endif

typedef signed short xsfsample_t;

static HMODULE hDLL;

class ReaderHolder
{
	void Free()
	{
		if (ptr)
		{
			ptr->reader_release();
			ptr = 0;
		}
	}
public:
	reader *ptr;
	ReaderHolder(reader *p = 0)
		: ptr(p)
	{
	}
	~ReaderHolder()
	{
		Free();
	}
	reader *GetReader()
	{
		return ptr;
	}
	void SetReader(reader * p)
	{
		Free();
		ptr = p;
	}
	bool Open(const xsfc::TString &fn)
	{
		SetReader(file::g_open(xsfc::TStringM(true, fn), reader::MODE_READ));
		return ptr != 0;
	}
};

class xsf_drv
{
protected:
	void *lpDrv;
	IXSFDRV *lpif;
	xsfc::TString m_libpath;
	bool m_genok;

	unsigned detectedSilenceSec;
	unsigned detectedSilenceSample;
	unsigned skipSilenceOnStartSec;
	unsigned long prevSampleL;
	unsigned long prevSampleR;

public:
	bool isUTF8;

	xsf_drv()
	{
		m_genok = false;
		isUTF8 = false;
		lpDrv = 0;
		lpif = 0;
	}

	~xsf_drv()
	{
		freeDrv();
	}
	void setlibpath(const xsfc::TString &p_libpath)
	{
		m_libpath = p_libpath;
	}

	void start(void *p, DWORD l)
	{
		if (loadDrv()) 
		{
			if (lpif->dwInterfaceVersion >= 3)
			{
				lpif->SetExtendParam(1, CFGGetExtendParam1());
				lpif->SetExtendParam(2, CFGGetExtendParam2());
			}
			m_genok = !lpif->Start(p, l);
		}

		skipSilenceOnStartSec = CFGGetSkipSilenceOnStartSec();

		detectedSilenceSec = 0;
		detectedSilenceSample = 0;

		prevSampleL = CHECK_SILENCE_BIAS;
		prevSampleR = CHECK_SILENCE_BIAS;
	}

	unsigned long get_detect_silence()
	{
		return detectedSilenceSec;
	}

	int gen(void *pbuf, unsigned bufsize)
	{
		if (!lpif) return 0;
		if (!m_genok) return 0;

		xsfsample_t *ptop = static_cast<xsfsample_t *>(pbuf); 
		unsigned detectSilence = CFGGetDetectSilenceSec();
		unsigned pos = 0;

		if (lpif->dwInterfaceVersion >= 2)
		{
			bool output = false;
			for (int i = 0; i < 4; i++)
			{
				unsigned long mute = CFGGetChannelMute(i);
				output |= (mute != ~unsigned long(0));
				lpif->SetChannelMute(i, mute);
			}
			if (!output) detectSilence = 0;
		}
		if (lpif->dwInterfaceVersion >= 4)
		{
			lpif->SetExtendParamImmediate(EXTEND_PARAM_IMMEDIATE_INTERPOLATION,&dwInterpolation);
		}

		while (pos < bufsize)
		{
			unsigned ofs;
			xsfsample_t *pblk = ptop + (pos << 1);
			unsigned remain = bufsize - pos;
			lpif->Gen(pblk, remain);
			if (detectSilence || skipSilenceOnStartSec)
			{
				xsfsample_t *pskp = 0;
				xsfsample_t *pcur = pblk;
				for (ofs = 0; ofs < remain; ofs++)
				{
					long smpl = pcur[0];
					long smpr = pcur[1];
					bool silence = (((unsigned long)(smpl + CHECK_SILENCE_BIAS + CHECK_SILENCE_LEVEL)) - prevSampleL <= (CHECK_SILENCE_LEVEL) * 2) && (((unsigned long)(smpr + CHECK_SILENCE_BIAS + CHECK_SILENCE_LEVEL)) - prevSampleR <= (CHECK_SILENCE_LEVEL) * 2);

					if (silence)
					{
						if (++detectedSilenceSample >= xsf_sample_rate)
						{
							detectedSilenceSample -= xsf_sample_rate;
							detectedSilenceSec++;
							if (skipSilenceOnStartSec && detectedSilenceSec >= skipSilenceOnStartSec)
							{
								skipSilenceOnStartSec = 0;
								detectedSilenceSec = 0;
								if (pblk != pcur) pskp = pcur;
							}
						}
					}
					else
					{
						detectedSilenceSample = 0;
						detectedSilenceSec = 0;
						if (skipSilenceOnStartSec)
						{
							skipSilenceOnStartSec = 0;
							if (pblk != pcur) pskp = pcur;
						}
					}
					prevSampleL = smpl + CHECK_SILENCE_BIAS;
					prevSampleR = smpr + CHECK_SILENCE_BIAS;
					pcur += 2;
				}
				if (skipSilenceOnStartSec)
				{
				}
				else if (pskp)
				{
					while (pskp < pcur)
					{
						*(pblk++)= *(pskp++);
						*(pblk++)= *(pskp++);
						pos++;
					}
				}
				else
				{
					pos += remain;
				}
			}
			else
			{
				pos += remain;
			}
		}
		return bufsize;
	}


	void stop()
	{
		if (!lpif) return;
		lpif->Term();
		m_genok = false;
	}


protected:
	bool aogetlib(xsfc::TString filename, void **ppBuffer, DWORD *pdwSize)
	{
		if (!lpif) return false;
		ReaderHolder xsflib;
		xsfc::TString path = xsfc::TWin32::CanonicalizePath(m_libpath + filename);
		if (!xsflib.Open(path)) return false;
		__int64 filesize64 = xsflib.GetReader()->get_length();
		DWORD dwfilesize = DWORD(filesize64);
		if (filesize64 == -1 || filesize64 != dwfilesize) return false;
		void *ret = lpif->LibAlloc(dwfilesize);
		if (!ret) return false;
		xsflib.GetReader()->read(ret, dwfilesize);
		*ppBuffer = ret;
		*pdwSize = dwfilesize;
		return true;
	}
	static int PASCAL XSFGETLIBCALLBACK(void *lpWork, LPSTR lpszFilename, void **ppBuffer, DWORD *pdwSize)
	{
		xsf_drv *pthis = static_cast<xsf_drv *>(lpWork);
		xsfc::TString filename;
		if (pthis->isUTF8)
		{
			filename = xsfc::TString(true, lpszFilename);
		}
		else
		{
			filename = xsfc::TString(false, lpszFilename);
		}
		return pthis->aogetlib(filename, ppBuffer, pdwSize) ? 0 : 1;
	}
	bool loadDrv(void)
	{
		if (lpif) return true;
		xsfc::TString binpath = xsfc::TWin32::ExtractPath(xsfc::TWin32::ModulePath(hDLL)) + XSFDRIVER_MODULENAME;

		if (xsfc::TWin32::IsUnicodeSupportedOS())
		{
			lpDrv = XLoadLibraryW(binpath);
		}
		else
		{
			lpDrv = XLoadLibraryA(xsfc::TStringM(binpath));
		}
		if (!lpDrv) return false;

		LPFNXSFDRVSETUP xsfsetup = (LPFNXSFDRVSETUP)XGetProcAddress(lpDrv, XSFDRIVER_ENTRYNAME);
		if (!xsfsetup)
		{
			XFreeLibrary(lpDrv);
			lpDrv = 0;
			return false;
		}

		lpif = xsfsetup(XSFGETLIBCALLBACK, this);
		return true;
	}

	void freeDrv(void)
	{
		if (lpif)
		{
			lpif->Term();
			lpif = 0;
		}
		if (lpDrv)
		{
			XFreeLibrary(lpDrv);
			lpDrv = 0;
		}
	}
};

class input_xsf : public input_pcm
{
protected:
	typedef signed short audio_sample_t;
	typedef __int64 t_size;
	typedef unsigned __int64  t_uint64;
	xsf_drv drv;
	array_t<BYTE> m_filebuffer;
	t_size m_filesize;
	t_uint64 cur_smp;
	t_uint64 len_smp;
	t_uint64 fad_smp;
	t_uint64 end_smp;

	array_t<unsigned char> m_buffer;

	double m_volume;
	bool m_hasvolume;
	bool m_haslength;

	bool m_write_UTF8;
	unsigned m_flags;
	
	typedef struct
	{
		class input_xsf *pThis;
		file_info * p_info;
		bool isUTF8;
	} tagloadcbwork;

	void tagloadsub(file_info & p_info, const char *name, const xsfc::TStringM &value)
	{
		if (!stricmp_utf8(name, "game"))
			p_info.meta_add("album", value);
		else if (!stricmp_utf8(name, "year"))
			p_info.meta_add("date", value);
		else if (!_strnicmp(name, "_lib", 4) || !stricmp_utf8(name, "fade") || !stricmp_utf8(name, "length") || !stricmp_utf8(name, "volume"))
			p_info.info_set(name, value);
		else if (!stricmp_utf8(name, "replaygain_track_gain"))
		{
			p_info.info_set_replaygain_track_gain(pfc_string_to_float(value));
		}
		else if (!stricmp_utf8(name, "replaygain_track_peak"))
		{
			p_info.info_set_replaygain_track_peak(pfc_string_to_float(value));
		}
		else if (!stricmp_utf8(name, "replaygain_album_gain"))
		{
			p_info.info_set_replaygain_album_gain(pfc_string_to_float(value));
		}
		else if (!stricmp_utf8(name, "replaygain_album_peak"))
		{
			p_info.info_set_replaygain_album_peak(pfc_string_to_float(value));
		}
		else
			p_info.meta_add(name, value);
	}

	static enum XSFTag::enum_callback_returnvalue tagloadcb(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd)
	{
		if (pNameTop == pNameEnd) return XSFTag::enum_continue;

		tagloadcbwork *pcbwork = static_cast<tagloadcbwork *>(pWork);

		xsfc::TString name;
		xsfc::TString value;

		if (pcbwork->isUTF8)
		{
			name = xsfc::TString(true, pNameTop, pNameEnd - pNameTop);
			value = xsfc::TString(true, pValueTop, pValueEnd - pValueTop);
		}
		else
		{
			name = xsfc::TString(false, pNameTop, pNameEnd - pNameTop);
			value = xsfc::TString(false, pValueTop, pValueEnd - pValueTop);
		}

		pcbwork->pThis->tagloadsub(*pcbwork->p_info, xsfc::TStringM(true, name), xsfc::TStringM(true, value));
		return XSFTag::enum_continue;
	}
	
	void tagload(file_info & p_info)
	{
		BYTE *pData = &m_filebuffer[0];
		DWORD dwSize = DWORD(m_filesize);
		bool isUTF8 = XSFTag::Exists("utf8", pData, dwSize);
		drv.isUTF8 = isUTF8;

		tagloadcbwork cbwork;
		cbwork.isUTF8 = isUTF8;
		cbwork.pThis = this;
		cbwork.p_info= &p_info;
		XSFTag::Enum(tagloadcb, &cbwork, pData, dwSize);
		return;
	}

	bool tag_writel(const char *p, reader *r)
	{
		string8 value;
		value.convert_to_lower_ascii(p);
		const char *pL = value.get_ptr();
		t_size lL = strlen(p);
		return r->write(pL, lL) == lL;
	}

	bool tag_writeA(const char *p, t_size l, reader *r)
	{
		xsfc::TString value(true, p, l);
		xsfc::TStringM valueA(false, value);
		const char *pA = valueA;
		t_size lA = strlen(pA);
		return r->write(pA, lA) == lA;
	}

	bool tag_writeU(const char *p, t_size l, reader *r)
	{
		return r->write(p, l) == l;
	}

	bool tag_write(const char *p, reader *r)
	{
		t_size l = strlen(p);
		array_t<char> lbuf;
		if (!lbuf.resize(l)) return false;
		char *d = &lbuf[0];
		for (t_size i = 0; i < l; i++)
		{
			if (p[i] == 0x0a)
				d[i] = ';';
			else if (p[i] > 0x00 && p[i] < 0x20)
				d[i] = ' ';
			else
				d[i] = p[i];
		}
		return (m_write_UTF8) ? tag_writeU(d, l, r) : tag_writeA(d, l, r);
	}

	bool tagsave(reader *r, const file_info & p_info)
	{
		BYTE *pData = &m_filebuffer[0];
		DWORD dwSize = DWORD(m_filesize);

		DWORD dwPos = XSFTag::SearchRaw(pData, dwSize);
		if (!dwPos || dwPos >= dwSize) return false;

		if (!r->seek(dwPos)) return false;

		m_write_UTF8 = p_info.meta_get_count_by_name("utf8") > 0;

		tag_write("[TAG]", r);
		if (m_write_UTF8)
			tag_writel("utf8=1\x0a", r);

		/* write play info */
		t_size icnt = p_info.info_get_count();
		for (t_size i = 0; i < icnt; i++)
		{
			const char *name = p_info.info_enum_name(i);
			if (!_strnicmp(name, "_lib", 4) || !stricmp_utf8(name, "fade") || !stricmp_utf8(name, "length") || !stricmp_utf8(name, "volume") || !stricmp_utf8(name, "replaygain_album_gain")  || !stricmp_utf8(name, "replaygain_album_peak")  || !stricmp_utf8(name, "replaygain_track_gain")  || !stricmp_utf8(name, "replaygain_track_peak"))
			{
				const char *value = p_info.info_enum_value(i);
				if (*value)
				{
					tag_writel(name, r);
					tag_writel("=", r);
					tag_write(value, r);
					tag_writel("\x0a", r);
				}
			}
		}

		/* write meta data */
		t_size mcnt = p_info.meta_get_count();
		for (t_size m = 0; m < mcnt; m++)
		{
			const char *name = p_info.meta_enum_name(m);
			if (!stricmp_utf8(name, "utf8"))
				continue;
			else if (!stricmp_utf8(name, "album"))
				name = "game";
			else if (!stricmp_utf8(name, "date"))
				name = "year";
			const char *value = p_info.meta_enum_value(m);
			if (*value)
			{
				tag_writel(name, r);
				tag_writel("=", r);
				tag_write(value, r);
				tag_writel("\x0a", r);
			}
		}

		r->set_eof();
		return true;
	}

	void xsf_restart()
	{
		drv.start(&m_filebuffer[0], DWORD(m_filebuffer.size()));
		cur_smp = 0;
	}

	bool xsf_reopen(reader *r)
	{
		m_filesize = r->get_length();
		t_size l = t_size(m_filesize);
		if (!m_filebuffer.resize(t_size(m_filesize))) return false;
		return r->read(&m_filebuffer[0], l) == l;
	}

	void xsf_reload(void)
	{
		xsfc::TString tagvolume = XSFTag::Get("volume", &m_filebuffer[0], size_t(m_filesize)) ;

		unsigned length = XSFTag::GetLengthMS(&m_filebuffer[0], size_t(m_filesize), CFGGetDefaultLength());
		unsigned fade = XSFTag::GetFadeMS(&m_filebuffer[0], size_t(m_filesize), CFGGetDefaultFade());

		m_haslength = XSFTag::Exists("length", &m_filebuffer[0], size_t(m_filesize));

		m_volume = 1.0;
		m_hasvolume = false;
		if (tagvolume[0])
		{
			m_volume = tagvolume.GetFloat();
			m_hasvolume = (m_volume != 1.0);
		}

		len_smp = t_uint64(length) * xsf_sample_rate / 1000;
		fad_smp = t_uint64(fade) * xsf_sample_rate / 1000;
		end_smp = len_smp + fad_smp;
	}

	void xsf_seek(t_uint64 &seek_smp)
	{
		t_size bufsize = m_buffer.size() / xsf_total_sample_width;
		if (bufsize < xsf_total_sample_width)
		{
			bufsize = DEFAULT_BUFFER_SIZE;
			m_buffer.resize(bufsize * xsf_total_sample_width);
		}
		if (seek_smp < cur_smp)
		{
			xsf_restart();
		}
		while (seek_smp - cur_smp > bufsize)
		{
			drv.gen(&m_buffer[0], DWORD(bufsize));
			cur_smp += bufsize;
		}
		if (seek_smp - cur_smp > 0)
		{
			drv.gen(&m_buffer[0], DWORD(seek_smp - cur_smp));
			cur_smp = seek_smp;
		}
	}

public:
	void get_info(file_info & p_info)
	{
		p_info.set_length(::MulDiv(end_smp, 1, xsf_sample_rate));

		p_info.info_set_int("samplerate",xsf_sample_rate);
		p_info.info_set_int("channels",xsf_channels);
		p_info.info_set_int("bitspersample",xsf_bits_per_sample);
		p_info.info_set("encoding",FOOBAR2000COMPONENT_ENCODING);
		p_info.info_set_bitrate((xsf_bits_per_sample * xsf_channels * xsf_sample_rate + 500 /* rounding for bps to kbps*/ ) / 1000 /* bps to kbps */);

		tagload(p_info);
	}

	bool open(reader * r,file_info * info,unsigned flags)
	{
		m_flags = flags;
		xsf_reopen(r);
		xsf_reload();

		if( flags & input::OPEN_FLAG_GET_INFO)
			get_info(*info);

		if (flags & input::OPEN_FLAG_DECODE)
		{
			const char *playpathutf8 = info->get_file_path();
			xsfc::TString playpath(true, playpathutf8);
			xsfc::TString libpath = xsfc::TWin32::ExtractPath(playpath);
			drv.setlibpath(libpath);
			xsf_restart();
		}
		return true;
	}

	inline audio_sample_t clip_mul(signed s1, double s2)
	{
		double r = s1 * s2;
		if (r > 0x7fff)
			r = 0x7fff;
		else if (r < -0x8000)
			r = -0x8000;
		return audio_sample_t(r);
	}

	int get_samples_pcm(void ** out_buffer,int * out_size, int * srate, int * bps, int * nch)
	{
		bool fPlayInfinitely = CFGGetPlayInfinitely() && !(m_flags & OPEN_FLAG_NO_LOOPING);

		if ((cur_smp >= end_smp && !fPlayInfinitely) || (!m_haslength && CFGGetDetectSilenceSec() && CFGGetDetectSilenceSec() <= drv.get_detect_silence()))
		{
			return false;
		}
		unsigned reqssize = DEFAULT_BUFFER_SIZE * xsf_total_sample_width; 
		if (m_buffer.size() < reqssize && !m_buffer.resize(reqssize))
			return false;		

		t_size bufsize = m_buffer.size() / xsf_total_sample_width;
		drv.gen(&m_buffer[0], DWORD(bufsize));

		if (!fPlayInfinitely && cur_smp + bufsize > end_smp)
			bufsize = t_size(end_smp - cur_smp);

		*out_buffer = &m_buffer[0];
		*out_size = bufsize * xsf_total_sample_width;
		*srate = xsf_sample_rate;
		*bps = xsf_bits_per_sample;
		*nch = xsf_channels;

		cur_smp += bufsize;

		double cfgvolume = 1;
		bool hascfgvolume = CFGGetVolume(cfgvolume);
		if (hascfgvolume || m_hasvolume || (!fPlayInfinitely && (fad_smp && cur_smp + bufsize >= len_smp)))
		{
			audio_sample_t *psmp = (audio_sample_t *)&m_buffer[0];
			t_uint64 i;
			double volume = m_volume * cfgvolume;
			for (i = cur_smp; i < cur_smp +  bufsize; i++)
			{
				if (fPlayInfinitely || i < len_smp)
				{
					psmp[0] = clip_mul(psmp[0] , volume);
					psmp[1] = clip_mul(psmp[1] , volume);
				}
				else if (i < end_smp)
				{
					double scale = volume * double(__int64(len_smp + fad_smp - i)) / double(__int64(fad_smp));
					psmp[0] = clip_mul(psmp[0] , scale);
					psmp[1] = clip_mul(psmp[1] , scale);
				}
				else
				{
					psmp[0] = 0;
					psmp[1] = 0;
				}
				psmp += 2;
			}
		}

		return true;
	}

	bool can_seek() {return true;}

	bool seek(double p_seconds)
	{
		if (can_seek())
		{
			t_uint64 seek_pos(t_uint64(p_seconds * xsf_sample_rate));
			xsf_seek(seek_pos);
			return true;
		}
		return false;
	}

	bool is_our_content_type(const char * url,const char * type) { (void)url, type; return false;}
	bool test_filename(const char * p_path,const char * p_extension) { (void)p_path, p_extension; return FOOBAR2000COMPONENT_EXT_CHECK; }

	set_info_t set_info(reader *r, const file_info * info)
	{
		xsf_reopen(r);
		return tagsave(r, *info) ? SET_INFO_SUCCESS : SET_INFO_FAILURE;
	}

};


static input_factory<input_xsf> g_input_xsf_factory;

DECLARE_COMPONENT_VERSION(FOOBAR2000COMPONENT_NAME,FOOBAR2000COMPONENT_VERSION,FOOBAR2000COMPONENT_ABOUT);
DECLARE_FILE_TYPE(FOOBAR2000COMPONENT_TYPE,FOOBAR2000COMPONENT_EXTS);

extern "C" void fb2k_config_init(HINSTANCE hinstDLL);

BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		hDLL = hModule;
		fb2k_config_init(hDLL);
	}
	else if (ul_reason_for_call == DLL_PROCESS_DETACH)
	{
	}
    return TRUE;
}



