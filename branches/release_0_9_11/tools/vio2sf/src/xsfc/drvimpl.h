#define XSF_FALSE (0)
#define XSF_TRUE (!XSF_FALSE)

#ifdef __cplusplus
extern "C" {
#endif

int xsf_start(void *pfile, unsigned bytes);
int xsf_gen(void *pbuffer, unsigned samples);
int xsf_get_lib(char *pfilename, void **ppbuffer, unsigned *plength);
void xsf_term(void);
void xsf_set_extend_param(unsigned dwId, const wchar_t *lpPtr);
extern unsigned long dwChannelMute;
extern unsigned long dwChannelMute;

#ifdef __cplusplus
}
#endif
