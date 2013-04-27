#ifdef __cplusplus
extern "C"
{
#endif

unsigned CFGGetPlayInfinitely(void);
unsigned CFGGetSkipSilenceOnStartSec(void);
unsigned CFGGetDetectSilenceSec(void);
unsigned CFGGetDefaultLength(void);
unsigned CFGGetDefaultFade(void);
bool CFGGetVolume(double &vol);
unsigned long CFGGetChannelMute(int page);
void CFGSetChannelMute(int ch, bool mute);
const wchar_t *CFGGetExtendParam1(void);
const wchar_t *CFGGetExtendParam2(void);

#ifdef __cplusplus
}

#include "xsfc.h"

#if _MSC_VER >= 1200
#pragma warning(push)
#pragma warning(disable:4290)
#endif

class IConfigIO
{
protected:
	IConfigIO() {}
public:
	virtual ~IConfigIO() {}

	virtual void SetULong(const wchar_t *name, const unsigned long value) throw() = 0;
	virtual unsigned long GetULong(const wchar_t *name, const unsigned long defaultvalue = 0) throw() = 0;
	virtual void SetFloat(const wchar_t *name, const double value) throw() = 0;
	virtual double GetFloat(const wchar_t *name, const double defaultvalue = 0) throw() = 0;
	virtual void SetString(const wchar_t *name, const wchar_t *value) throw() = 0;
	virtual xsfc::TString GetString(const wchar_t *name, const wchar_t *defaultvalue = 0) throw(xsfc::EShortOfMemory) = 0;
};
typedef IConfigIO *LPIConfigIO;

class NullConfig : public IConfigIO
{
protected:
	NullConfig() throw() {}
public:
	~NullConfig() throw() {}

	void SetULong(const wchar_t *name, const unsigned long value) throw() { (void)name, value; }
	unsigned long GetULong(const wchar_t *name, const unsigned long defaultvalue = 0) throw() { (void)name; return defaultvalue; }
	void SetFloat(const wchar_t *name, const double value) throw() { (void)name, value; }
	double GetFloat(const wchar_t *name, const double defaultvalue = 0) throw() { (void)name; return defaultvalue; }
	void SetString(const wchar_t *name, const wchar_t *value) throw() { (void)name, value; }
	xsfc::TString GetString(const wchar_t *name, const wchar_t *defaultvalue = 0) throw(xsfc::EShortOfMemory) { (void)name; return xsfc::TString(defaultvalue); }

	static LPIConfigIO Create() throw()
	{
		static NullConfig singleton;
		return &singleton;
	}
};

void CFGDefault(void);
void CFGLoad(LPIConfigIO pcfg);
void CFGReset(LPIConfigIO pcfg, void *hwndDlg);
void CFGUpdate(LPIConfigIO pcfg, void *hwndDlg);
void CFGSave(LPIConfigIO pcfg);
void CFGMuteChange(void *hwndDlg, int itm);

#if _MSC_VER >= 1200
#pragma warning(pop)
#endif

#endif