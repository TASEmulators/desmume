#include "leakchk.h"

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
	xsf_total_sample_width = xsf_bytes_per_sample * xsf_channels,
};

#define DEFAULT_BUFFER_SIZE (1024)

#define CHECK_SILENCE_BIAS 0x8000000
#ifndef CHECK_SILENCE_LEVEL
#define CHECK_SILENCE_LEVEL 7
#endif

typedef signed short xsfsample_t;

static HMODULE hDLL;

class xsf_drv
{
protected:
	void *lpDrv;
	IXSFDRV *lpif;
	pfc::string8 m_libpath;
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
	void setlibpath(pfc::string8 &p_libpath)
	{
		m_libpath.set_string(p_libpath.get_ptr());
	}

	void start(void *p, DWORD l)
	{
		if (loadDrv()) 
			m_genok = !lpif->Start(p, l);

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
	bool aogetlib(pfc::string8 &filename, void **ppBuffer, DWORD *pdwSize)
	{
		try
		{
			if (!lpif) return false;
			service_ptr_t<file> xsflib;
			abort_callback_impl abort_cb;
			xsfc::TStringM path(true, xsfc::TWin32::CanonicalizePath(xsfc::TString(true, m_libpath.get_ptr()) + xsfc::TString(true, filename.get_ptr())));
			pfc::string8 xsfpath(path.GetM());
			filesystem::g_open_read(xsflib, xsfpath, abort_cb);
			DWORD dwfilesize = DWORD(xsflib->get_size_ex(abort_cb));
			void *ret = lpif->LibAlloc(dwfilesize);
			if (!ret) return false;
			xsflib->read(ret, dwfilesize, abort_cb);
			*ppBuffer = ret;
			*pdwSize = dwfilesize;
			return true;
		} catch (xsfc::EShortOfMemory e) {
		} catch (exception_io e) {
		}
		return false;
	}
	static int PASCAL XSFGETLIBCALLBACK(void *lpWork, LPSTR lpszFilename, void **ppBuffer, DWORD *pdwSize)
	{
		xsf_drv *pthis = static_cast<xsf_drv *>(lpWork);
		pfc::string8 filename;
		if (pthis->isUTF8)
		{
			filename.set_string(static_cast<const char *>(lpszFilename));
		}
		else
		{
			pfc::stringcvt::string_utf8_from_ansi valuea(static_cast<const char *>(lpszFilename));
			filename.set_string(valuea.get_ptr());
		}
		return pthis->aogetlib(filename, ppBuffer, pdwSize) ? 0 : 1;
	}
	bool loadDrv(void)
	{
		if (lpif) return true;
		pfc::string8 dllpath;
		uGetModuleFileName(hDLL, dllpath);
		pfc::string8 binpath(dllpath, pfc::scan_filename(dllpath));
		binpath.add_string(XSFDRIVER_MODULENAME);

		if (xsfc::TWin32::IsUnicodeSupportedOS())
		{
			pfc::stringcvt::string_wide_from_utf8 dllpathw(binpath);
			lpDrv = XLoadLibraryW(dllpathw.get_ptr());
		}
		else
		{
			pfc::stringcvt::string_ansi_from_utf8 dllpatha(binpath);
			lpDrv = XLoadLibraryA(dllpatha.get_ptr());
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

class input_xsf
{
protected:
	xsf_drv drv;
	pfc::array_t<t_uint8> m_filebuffer;
	foobar2000_io::t_filesize m_filesize;
	t_uint64 cur_smp;
	t_uint64 len_smp;
	t_uint64 fad_smp;
	t_uint64 end_smp;

	service_ptr_t<file> m_file;
	pfc::array_t<t_uint8> m_buffer;

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

	void tagloadsub(file_info & p_info, pfc::string8 &name, pfc::string8 &value)
	{
		if (!stricmp_utf8(name.get_ptr(), "game"))
			p_info.meta_add("album", value.get_ptr());
		else if (!stricmp_utf8(name.get_ptr(), "year"))
			p_info.meta_add("date", value.get_ptr());
		else if (!_strnicmp(name.get_ptr(), "_lib", 4) || !stricmp_utf8(name.get_ptr(), "fade") || !stricmp_utf8(name.get_ptr(), "length") || !stricmp_utf8(name.get_ptr(), "volume"))
			p_info.info_set(name.get_ptr(), value.get_ptr());
		else if (!stricmp_utf8(name.get_ptr(), "replaygain_track_gain"))
		{
			replaygain_info rg = p_info.get_replaygain();
			rg.set_track_gain_text(value.get_ptr());
			p_info.set_replaygain(rg);
		}
		else if (!stricmp_utf8(name.get_ptr(), "replaygain_track_peak"))
		{
			replaygain_info rg = p_info.get_replaygain();
			rg.set_track_peak_text(value.get_ptr());
			p_info.set_replaygain(rg);
		}
		else if (!stricmp_utf8(name.get_ptr(), "replaygain_album_gain"))
		{
			replaygain_info rg = p_info.get_replaygain();
			rg.set_album_gain_text(value.get_ptr());
			p_info.set_replaygain(rg);
		}
		else if (!stricmp_utf8(name.get_ptr(), "replaygain_album_peak"))
		{
			replaygain_info rg = p_info.get_replaygain();
			rg.set_album_peak_text(value.get_ptr());
			p_info.set_replaygain(rg);
		}
		else
			p_info.meta_add(name.get_ptr(), value.get_ptr());
	}

	static enum XSFTag::enum_callback_returnvalue tagloadcb(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd)
	{
		if (pNameTop == pNameEnd) return XSFTag::enum_continue;

		tagloadcbwork *pcbwork = static_cast<tagloadcbwork *>(pWork);

		pfc::string8 name;
		name.set_string(pNameTop, pNameEnd - pNameTop);

		pfc::string8 value;
		if (pcbwork->isUTF8)
		{
			value.set_string(pValueTop, pValueEnd - pValueTop);
		}
		else
		{
			pfc::stringcvt::string_utf8_from_ansi valuea(pValueTop, pValueEnd - pValueTop);
			value.set_string(valuea.get_ptr());
		}

		pcbwork->pThis->tagloadsub(*pcbwork->p_info, name, value);
		return XSFTag::enum_continue;
	}
	
	void tagload(file_info & p_info)
	{
		BYTE *pData = m_filebuffer.get_ptr();
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

	void tagsave(const file_info & p_info, abort_callback & p_abort)
	{
		BYTE *pData = m_filebuffer.get_ptr();
		DWORD dwSize = DWORD(m_filesize);

		DWORD dwPos = XSFTag::SearchRaw(pData, dwSize);
		if (!dwPos || dwPos >= dwSize) return;

		m_file->truncate(dwPos, p_abort);
		m_file->seek(dwPos, p_abort);

		m_write_UTF8 = p_info.meta_exists("utf8");

		tag_write("[TAG]", p_abort);
		if (m_write_UTF8)
			tag_writel("utf8=1\x0a", p_abort);

		/* write play info */
		t_size icnt = p_info.info_get_count();
		for (t_size i = 0; i < icnt; i++)
		{
			const char *name = p_info.info_enum_name(i);
			if (!_strnicmp(name, "_lib", 4) || !stricmp_utf8(name, "fade") || !stricmp_utf8(name, "length") || !stricmp_utf8(name, "volume"))
			{
				const char *value = p_info.info_enum_value(i);
				if (*value)
				{
					tag_writel(name, p_abort);
					tag_writel("=", p_abort);
					tag_write(value, p_abort);
					tag_writel("\x0a", p_abort);
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
			t_size ncnt = p_info.meta_enum_value_count(m);
			for (t_size n = 0; n < ncnt; n++)
			{
				const char *value = p_info.meta_enum_value(m, n);
				if (*value)
				{
					tag_writel(name, p_abort);
					tag_writel("=", p_abort);
					tag_write(value, p_abort);
					tag_writel("\x0a", p_abort);
				}
			}
		}

		/* write replay gain */
		pfc::array_t<char> rgbuf;
		replaygain_info rg = p_info.get_replaygain();
		rgbuf.set_size(rg.text_buffer_size);

		if (rg.m_album_gain > rg.gain_invalid)
		{
			rg.format_album_gain(rgbuf.get_ptr());
			tag_writel("replaygain_album_gain", p_abort);
			tag_writel("=", p_abort);
			tag_write(rgbuf.get_ptr(), p_abort);
			tag_writel("\x0a", p_abort);
		}
		if (rg.m_album_peak > rg.peak_invalid)
		{
			rg.format_album_peak(rgbuf.get_ptr());
			tag_writel("replaygain_album_peak", p_abort);
			tag_writel("=", p_abort);
			tag_write(rgbuf.get_ptr(), p_abort);
			tag_writel("\x0a", p_abort);
		}
		if (rg.m_track_gain > rg.gain_invalid)
		{
			rg.format_track_gain(rgbuf.get_ptr());
			tag_writel("replaygain_track_gain", p_abort);
			tag_writel("=", p_abort);
			tag_write(rgbuf.get_ptr(), p_abort);
			tag_writel("\x0a", p_abort);
		}
		if (rg.m_track_peak > rg.peak_invalid)
		{
			rg.format_track_peak(rgbuf.get_ptr());
			tag_writel("replaygain_track_peak", p_abort);
			tag_writel("=", p_abort);
			tag_write(rgbuf.get_ptr(), p_abort);
			tag_writel("\x0a", p_abort);
		}
		m_file->set_eof(p_abort);
	}

	void tag_writel(const char *p, abort_callback & p_abort)
	{
		t_size l = strlen(p);
		pfc::array_t<char> lbuf;
		lbuf.set_size(l);
		char *d = lbuf.get_ptr();
		for (t_size i = 0; i < l; i++)
			d[i] = pfc::ascii_tolower(p[i]);
		m_file->write(d, l, p_abort);
	}

	void tag_writeA(const char *p, t_size l, abort_callback & p_abort)
	{
		pfc::stringcvt::string_ansi_from_utf8 valuea(p, l);
		m_file->write(valuea.get_ptr(), valuea.length(), p_abort);
	}

	void tag_write(const char *p, abort_callback & p_abort)
	{
		t_size l = strlen(p);
		pfc::array_t<char> lbuf;
		lbuf.set_size(l);
		char *d = lbuf.get_ptr();
		for (t_size i = 0; i < l; i++)
		{
			if (p[i] == 0x0a)
				d[i] = ';';
			else if (p[i] > 0x00 && p[i] < 0x20)
				d[i] = ' ';
			else
				d[i] = p[i];
		}
		if (m_write_UTF8)
			m_file->write(d, l, p_abort);
		else
			tag_writeA(d, l, p_abort);
	}

	void xsf_restart()
	{
		drv.start(m_filebuffer.get_ptr(), DWORD(m_filebuffer.get_size()));
		cur_smp = 0;
	}

	void xsf_reopen(service_ptr_t<file> & p_file, abort_callback & p_abort)
	{
		m_filesize = p_file->get_size_ex(p_abort);
		m_filebuffer.set_size(t_size(m_filesize));
		p_file->read(m_filebuffer.get_ptr(), t_size(m_filesize), p_abort);
		xsf_reload();
	}

	void xsf_reload(void)
	{
		xsfc::TString tagvolume = XSFTag::Get("volume", m_filebuffer.get_ptr(), size_t(m_filesize));

		t_uint32 length = XSFTag::GetLengthMS(m_filebuffer.get_ptr(), size_t(m_filesize), CFGGetDefaultLength());
		t_uint32 fade = XSFTag::GetFadeMS(m_filebuffer.get_ptr(), size_t(m_filesize), CFGGetDefaultFade());

		m_haslength = XSFTag::Exists("length", m_filebuffer.get_ptr(), size_t(m_filesize));

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

	void xsf_seek(t_uint64 &seek_smp, abort_callback & p_abort)
	{
		t_size bufsize = m_buffer.get_size() / xsf_total_sample_width;
		if (bufsize < xsf_total_sample_width)
		{
			bufsize = DEFAULT_BUFFER_SIZE;
			m_buffer.set_size(bufsize * xsf_total_sample_width);
		}
		if (seek_smp < cur_smp)
		{
			xsf_restart();
		}
		while (seek_smp - cur_smp > bufsize)
		{
			if (p_abort.is_aborting())
				return;
			drv.gen(m_buffer.get_ptr(), DWORD(bufsize));
			cur_smp += bufsize;
		}
		if (seek_smp - cur_smp > 0)
		{
			drv.gen(m_buffer.get_ptr(), DWORD(seek_smp - cur_smp));
			cur_smp = seek_smp;
		}
	}

public:
	void open(service_ptr_t<file> p_filehint,const char * p_path,t_input_open_reason p_reason,abort_callback & p_abort)
	{

		m_file = p_filehint;//p_filehint may be null, hence next line
		input_open_file_helper(m_file,p_path,p_reason,p_abort);//if m_file is null, opens file with appropriate privileges for our operation (read/write for writing tags, read-only otherwise).

		pfc::string8 libpath(p_path, pfc::scan_filename(p_path));
		drv.setlibpath(libpath);

		xsf_reopen(m_file, p_abort);

		if (p_reason == input_open_decode)
			xsf_restart();
	}

	void get_info(file_info & p_info,abort_callback & p_abort)
	{
		(void)p_abort;
		p_info.set_length(audio_math::samples_to_time( end_smp, xsf_sample_rate));

		p_info.info_set_int("samplerate",xsf_sample_rate);
		p_info.info_set_int("channels",xsf_channels);
		p_info.info_set_int("bitspersample",xsf_bits_per_sample);
		p_info.info_set("encoding",FOOBAR2000COMPONENT_ENCODING);
		p_info.info_set_bitrate((xsf_bits_per_sample * xsf_channels * xsf_sample_rate + 500 /* rounding for bps to kbps*/ ) / 1000 /* bps to kbps */);

		tagload(p_info);
	}
	t_filestats get_file_stats(abort_callback & p_abort) {return m_file->get_stats(p_abort);}

	void decode_initialize(unsigned p_flags,abort_callback & p_abort)
	{
		(void)p_abort;
		m_flags = p_flags;
	}

	bool decode_run(audio_chunk & p_chunk,abort_callback & p_abort)
	{
		(void)p_abort;
		bool fPlayInfinitely = CFGGetPlayInfinitely() && !(m_flags & input_flag_no_looping) && (m_flags & input_flag_playback);

		if ((cur_smp >= end_smp && !fPlayInfinitely) || (!m_haslength && CFGGetDetectSilenceSec() && CFGGetDetectSilenceSec() <= drv.get_detect_silence()))
		{
			return false;
		}
		m_buffer.set_size(DEFAULT_BUFFER_SIZE * xsf_total_sample_width);
		t_size bufsize = m_buffer.get_size() / xsf_total_sample_width;
		drv.gen(m_buffer.get_ptr(), DWORD(bufsize));

		if (!fPlayInfinitely && cur_smp + bufsize > end_smp)
			bufsize = t_size(end_smp - cur_smp);

		cur_smp += bufsize;

		p_chunk.set_data_fixedpoint(m_buffer.get_ptr(),bufsize * xsf_total_sample_width,xsf_sample_rate,xsf_channels,xsf_bits_per_sample,audio_chunk::g_guess_channel_config(xsf_channels));

		double cfgvolume = 1;
		bool hascfgvolume = (m_flags & input_flag_playback) && CFGGetVolume(cfgvolume);
		if (hascfgvolume || m_hasvolume || (!fPlayInfinitely && (fad_smp && cur_smp + bufsize >= len_smp)))
		{
			audio_sample *psmp = p_chunk.get_data();
			t_uint64 i;
			double volume = m_volume * cfgvolume;
			for (i = cur_smp; i < cur_smp +  bufsize; i++)
			{
				if (fPlayInfinitely || i < len_smp)
				{
					psmp[0] = float(psmp[0] * volume);
					psmp[1] = float(psmp[1] * volume);
				}
				else if (i < end_smp)
				{
					double scale = volume * double(len_smp + fad_smp - i) / double(fad_smp);
					psmp[0] = float(psmp[0] * scale);
					psmp[1] = float(psmp[1] * scale);
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
	void decode_seek(double p_seconds,abort_callback & p_abort)
	{
		if (!decode_can_seek())
		{
			throw exception_io_object_not_seekable();
		}
		else
		{
			t_uint64 seek_pos(t_uint64(p_seconds * xsf_sample_rate));
			xsf_seek(seek_pos, p_abort);
		}
	}
	bool decode_can_seek() {return true;}
	bool decode_get_dynamic_info(file_info & p_out, double & p_timestamp_delta) { (void)p_out, p_timestamp_delta; return false; }
	bool decode_get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta) { (void)p_out, p_timestamp_delta; return false; }
	void decode_on_idle(abort_callback & p_abort) { m_file->on_idle(p_abort); }

	void retag(const file_info & p_info,abort_callback & p_abort)
	{
		tagsave(p_info, p_abort);

		m_file->reopen(p_abort);
		xsf_reopen(m_file, p_abort);
	}
	
	static bool g_is_our_content_type(const char * p_content_type) { (void)p_content_type; return false;}
	static bool g_is_our_path(const char * p_path,const char * p_extension) { (void)p_path, p_extension; return FOOBAR2000COMPONENT_EXT_CHECK; }
};


static input_singletrack_factory_t<input_xsf> g_input_xsf_factory;

DECLARE_COMPONENT_VERSION(FOOBAR2000COMPONENT_NAME,FOOBAR2000COMPONENT_VERSION,FOOBAR2000COMPONENT_ABOUT);
DECLARE_FILE_TYPE(FOOBAR2000COMPONENT_TYPE,FOOBAR2000COMPONENT_EXTS);

extern "C" void fb2k_config_init(HINSTANCE hinstDLL);

BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
#if defined(_MSC_VER) && defined(_DEBUG)
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
		hDLL = hModule;
		fb2k_config_init(hDLL);
	}
	else if (ul_reason_for_call == DLL_PROCESS_DETACH)
	{
	}
    return TRUE;
}



