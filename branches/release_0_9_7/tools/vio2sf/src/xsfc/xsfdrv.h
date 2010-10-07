#ifdef __cplusplus
extern "C" {
#endif

typedef int (PASCAL * LPFNGETLIB_XSFDRV)(void *lpWork, LPSTR lpszFilename, void **ppBuffer, DWORD *pdwSize);
typedef struct
{
	/* V1 */
	void * (PASCAL * LibAlloc)(DWORD dwSize);
	void (PASCAL * LibFree)(void *lpPtr);
	int (PASCAL * Start)(void *lpPtr, DWORD dwSize);
	void (PASCAL * Gen)(void *lpPtr, DWORD dwSamples);
	void (PASCAL * Term)(void);

	/* V2 */
	DWORD dwInterfaceVersion;
	void (PASCAL * SetChannelMute)(DWORD dwPage, DWORD dwMute);

	/* V3 */
	void (PASCAL * SetExtendParam)(DWORD dwId, LPCWSTR lpPtr);

	/* V4 */
	void (PASCAL * SetExtendParamImmediate)(DWORD dwId, LPVOID lpPtr);
} IXSFDRV;

#define EXTEND_PARAM_IMMEDIATE_INTERPOLATION 0
#define EXTEND_PARAM_IMMEDIATE_INTERPOLATION_NONE 0
#define EXTEND_PARAM_IMMEDIATE_INTERPOLATION_LINEAR 1
#define EXTEND_PARAM_IMMEDIATE_INTERPOLATION_COSINE 2

#define EXTEND_PARAM_IMMEDIATE_ADDINSTRUMENT			1
#define EXTEND_PARAM_IMMEDIATE_ISINSTRUMENTMUTED		2
#define EXTEND_PARAM_IMMEDIATE_GETINSTRUMENTVOLUME		3
#define EXTEND_PARAM_IMMEDIATE_OPENSOUNDVIEW			4

typedef void (*TYPE_EXTEND_PARAM_IMMEDIATE_ADDINSTRUMENT)(unsigned long addr, int type);
typedef BOOL (*TYPE_EXTEND_PARAM_IMMEDIATE_ISINSTRUMENTMUTED)(unsigned long addr);
typedef unsigned long (*TYPE_EXTEND_PARAM_IMMEDIATE_GETINSTRUMENTVOLUME)(unsigned long addr);
typedef unsigned long (*TYPE_EXTEND_PARAM_IMMEDIATE_OPENSOUNDVIEW)(void* callback);

typedef IXSFDRV * (PASCAL * LPFNXSFDRVSETUP)(LPFNGETLIB_XSFDRV lpfn, void *lpWork);
/* IXSFDRV * PASCAL XSFDRVSetup(LPFNGETLIB_XSFDRV lpfn, void *lpWork); */

#ifdef __cplusplus
}
#endif

