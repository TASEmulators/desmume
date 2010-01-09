#include "utf8api.h"
#include <imagehlp.h>

typedef BOOL (__stdcall * t_SymInitialize)(IN HANDLE hProcess,IN LPSTR UserSearchPath,IN BOOL fInvadeProcess);
typedef BOOL (__stdcall * t_SymCleanup)(IN HANDLE hProcess);
typedef BOOL (__stdcall * t_SymGetModuleInfo)(IN HANDLE hProcess,IN DWORD dwAddr,OUT PIMAGEHLP_MODULE ModuleInfo);
typedef BOOL (__stdcall * t_SymGetSymFromAddr)(IN HANDLE hProcess,IN DWORD dwAddr,OUT PDWORD pdwDisplacement,OUT PIMAGEHLP_SYMBOL Symbol);
typedef BOOL (__stdcall * t_SymEnumerateModules)(IN HANDLE hProcess,IN PSYM_ENUMMODULES_CALLBACK EnumModulesCallback,IN PVOID UserContext);

static t_SymInitialize p_SymInitialize;
static t_SymCleanup p_SymCleanup;
static t_SymGetModuleInfo p_SymGetModuleInfo;
static t_SymGetSymFromAddr p_SymGetSymFromAddr;
static t_SymEnumerateModules p_SymEnumerateModules;

static HMODULE hImageHlp;

static critical_section g_imagehelp_sync;

static string_simple version_string;

static long crash_no;

static __declspec(thread) char g_thread_call_stack[1024];
static __declspec(thread) unsigned g_thread_call_stack_length;

static bool load_imagehelp()
{
	static bool error;
	if (hImageHlp) return true;
	if (error) return false;
	hImageHlp = LoadLibraryA("imagehlp.dll");
	if (hImageHlp==0) {error = true; return false; }
	p_SymInitialize = (t_SymInitialize)GetProcAddress(hImageHlp,"SymInitialize");
	p_SymCleanup = (t_SymCleanup)GetProcAddress(hImageHlp,"SymCleanup");
	p_SymGetModuleInfo = (t_SymGetModuleInfo)GetProcAddress(hImageHlp,"SymGetModuleInfo");
	p_SymGetSymFromAddr = (t_SymGetSymFromAddr)GetProcAddress(hImageHlp,"SymGetSymFromAddr");
	p_SymEnumerateModules = (t_SymEnumerateModules)GetProcAddress(hImageHlp,"SymEnumerateModules");

	if (!p_SymInitialize || !p_SymCleanup || !p_SymGetModuleInfo || !p_SymGetSymFromAddr || !p_SymEnumerateModules)
	{
		FreeLibrary(hImageHlp);
		hImageHlp = 0;
		error = true;
		return false;
	}

	return true;
}

#define LOG_PATH_LEN (MAX_PATH + 32)

static HANDLE create_failure_log(char * filename_used,TCHAR * fullpath_used)
{
	bool rv = false;
	TCHAR path[LOG_PATH_LEN];
	path[0]=0;
	GetModuleFileName(0,path,MAX_PATH);
	path[MAX_PATH-1]=0;
	{
		TCHAR * ptr = path + _tcslen(path);
		while(ptr>path && *ptr!='\\') ptr--;
		ptr[1]=0;
	}
	TCHAR * fn_out = path + _tcslen(path);
	HANDLE hFile = INVALID_HANDLE_VALUE;
	unsigned attempts = 0;
	for(;;)
	{
		if (*fn_out==0)
		{
			_tcscpy(fn_out,TEXT("failure.txt"));
			if (filename_used) strcpy(filename_used,"failure.txt");
		}
		else
		{
			attempts++;
			wsprintf(fn_out,TEXT("failure_%08u.txt"),attempts);
			if (filename_used) wsprintfA(filename_used,"failure_%08u.txt",attempts);
		}
		hFile = CreateFile(path,GENERIC_WRITE,0,0,CREATE_NEW,0,0);
		if (hFile!=INVALID_HANDLE_VALUE) break;
		if (attempts > 1000) break;
	}
	if (hFile!=INVALID_HANDLE_VALUE && fullpath_used)
	{
		_tcscpy(fullpath_used,path);
	}
	return hFile;
}

static void WriteFileString_internal(HANDLE hFile,const char * ptr,unsigned len)
{
	DWORD bah;
	WriteFile(hFile,ptr,len,&bah,0);
}

static void WriteFileString(HANDLE hFile,const char * str)
{
	const char * ptr = str;
	for(;;)
	{
		const char * start = ptr;
		ptr = strchr(ptr,'\n');
		if (ptr)
		{
			if (ptr>start) WriteFileString_internal(hFile,start,ptr-start);
			WriteFileString_internal(hFile,"\x0d\x0a",2);
			ptr++;
		}
		else
		{
			WriteFileString_internal(hFile,start,strlen(start));
			break;
		}
	}
}

static char * hexdump8(char * out,unsigned address,const char * msg,int from,int to)
{
	unsigned max = (to-from)*16;
	if (!IsBadReadPtr((const void*)(address+(from*16)),max))
	{
		out += sprintf(out,"%s (%08Xh):",msg,address);
		unsigned n;
		const unsigned char * src = (const unsigned char*)(address)+(from*16);
		
		for(n=0;n<max;n++)
		{
			if (n%16==0)
			{
				out += sprintf(out,"\n%08Xh: ",src);
			}

			out += sprintf(out," %02X",*(src++));
		}
		*(out++) = '\n';
		*out=0;
	}
	return out;
}

static char * hexdump32(char * out,unsigned address,const char * msg,int from,int to)
{
	unsigned max = (to-from)*16;
	if (!IsBadReadPtr((const void*)(address+(from*16)),max))
	{
		out += sprintf(out,"%s (%08Xh):",msg,address);
		unsigned n;
		const unsigned char * src = (const unsigned char*)(address)+(from*16);
		
		for(n=0;n<max;n+=4)
		{
			if (n%16==0)
			{
				out += sprintf(out,"\n%08Xh: ",src);
			}

			out += sprintf(out," %08X",*(long*)src);
			src += 4;
		}
		*(out++) = '\n';
		*out=0;
	}
	return out;
}

static bool read_int(unsigned src,unsigned * out)
{
	__try
	{
		*out = *(unsigned*)src;
		return true;
	}
	__except(1) {return false;}
}

static bool test_address(unsigned address)
{
	__try
	{
		unsigned temp = *(unsigned*)address;
		return temp != 0;
	}
	__except(1) {return false;}
}

static void call_stack_parse(unsigned address,HANDLE hFile,char * temp,HANDLE hProcess)
{
	bool inited = false;
	unsigned data;
	unsigned count_done = 0;
	while(count_done<256 && read_int(address, &data))
	{
		unsigned data;
		if (read_int(address, &data))
		{
			if (test_address(data))
			{
				bool found = false;
				{
					IMAGEHLP_MODULE mod;
					memset(&mod,0,sizeof(mod));
					mod.SizeOfStruct = sizeof(mod);
					if (p_SymGetModuleInfo(hProcess,data,&mod))
					{
						if (!inited)
						{
							WriteFileString(hFile,"\nStack dump analysis:\n");
							inited = true;
						}
						sprintf(temp,"Address: %08Xh, location: \"%s\", loaded at %08Xh - %08Xh\n",data,mod.ModuleName,mod.BaseOfImage,mod.BaseOfImage+mod.ImageSize);
						WriteFileString(hFile,temp);
						found = true;
					}
				}


				if (found)
				{
					union
					{
						char buffer[128];
						IMAGEHLP_SYMBOL symbol;
					};
					memset(buffer,0,sizeof(buffer));
					symbol.SizeOfStruct = sizeof(symbol);
					symbol.MaxNameLength = buffer + sizeof(buffer) - symbol.Name;
					DWORD offset = 0;
					if (p_SymGetSymFromAddr(hProcess,data,&offset,&symbol))
					{
						buffer[tabsize(buffer)-1]=0;
						if (symbol.Name[0])
						{
							sprintf(temp,"Symbol: \"%s\" (+%08Xh)\n",symbol.Name,offset);
							WriteFileString(hFile,temp);
						}
					}
				}
			}
		}
		address += 4;
		count_done++;
	}
}

static BOOL CALLBACK EnumModulesCallback(LPSTR ModuleName,ULONG BaseOfDll,PVOID UserContext)
{
	IMAGEHLP_MODULE mod;
	memset(&mod,0,sizeof(mod));
	mod.SizeOfStruct = sizeof(mod);
	if (p_SymGetModuleInfo(GetCurrentProcess(),BaseOfDll,&mod))
	{
		char temp[1024];
		char temp2[tabsize(mod.ModuleName)+1];
		strcpy(temp2,mod.ModuleName);

		{
			unsigned ptr;
			for(ptr=strlen(temp2);ptr<tabsize(temp2)-1;ptr++)
				temp2[ptr]=' ';
			temp2[ptr]=0;
		}

		sprintf(temp,"%s loaded at %08Xh - %08Xh\n",temp2,mod.BaseOfImage,mod.BaseOfImage+mod.ImageSize);
		WriteFileString((HANDLE)UserContext,temp);
	}
	return TRUE;
}


UTF8API_EXPORT void uDumpCrashInfo(LPEXCEPTION_POINTERS param)
{
	char temp[2048];
	long number = InterlockedIncrement(&crash_no);
	TCHAR log_path[LOG_PATH_LEN];
	HANDLE hFile = create_failure_log(0,log_path);
	if (hFile!=INVALID_HANDLE_VALUE)
	{
		unsigned address = (unsigned)param->ExceptionRecord->ExceptionAddress;
		sprintf(temp,"Illegal operation:\nCode: %08Xh, flags: %08Xh, address: %08Xh\n",param->ExceptionRecord->ExceptionCode,param->ExceptionRecord->ExceptionFlags,address);
		WriteFileString(hFile,temp);

		if (param->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION && param->ExceptionRecord->NumberParameters>=2)
		{
			sprintf(temp,"Access violation, operation: %s, address: %08Xh\n",param->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",param->ExceptionRecord->ExceptionInformation[1]);
			WriteFileString(hFile,temp);
		}

		WriteFileString(hFile,"Call path:\n");
		WriteFileString(hFile,g_thread_call_stack);
		WriteFileString(hFile,"\n");
		if (number==1)
		{
			WriteFileString(hFile,"This is the first crash logged by this instance.\n");
		}
		else
		{
			const char * meh;
			switch(number%10)
			{
			case 1:
				meh = "st";
				break;
			case 2:
				meh = "nd";
				break;
			case 3:
				meh = "rd";
				break;
			default:
				meh = "th";
				break;
			}
			sprintf(temp,"This is your %u-%s crash. When reporting the problem to a developer, please try to post info about the first crash instead.\n",number,meh);
			WriteFileString(hFile,temp);
		}

		hexdump8(temp,address,"Code bytes",-4,+4);
		WriteFileString(hFile,temp);
		hexdump32(temp,param->ContextRecord->Esp,"Stack",-2,+18);
		WriteFileString(hFile,temp);
		sprintf(temp,"Registers:\nEAX: %08X, EBX: %08X, ECX: %08X, EDX: %08X\nESI: %08X, EDI: %08X, EBP: %08X, ESP: %08X\n",param->ContextRecord->Eax,param->ContextRecord->Ebx,param->ContextRecord->Ecx,param->ContextRecord->Edx,param->ContextRecord->Esi,param->ContextRecord->Edi,param->ContextRecord->Ebp,param->ContextRecord->Esp);
		WriteFileString(hFile,temp);

		{
			bool imagehelp_succeeded = false;
			insync(g_imagehelp_sync);
			if (load_imagehelp())
			{
				HANDLE hProcess = GetCurrentProcess();
				char exepath[MAX_PATH];
				exepath[0]=0;
				GetModuleFileNameA(0,exepath,tabsize(exepath));
				exepath[tabsize(exepath)-1]=0;
				{
					char * ptr = exepath + strlen(exepath);
					while(ptr>exepath && *ptr!='\\') ptr--;
					*ptr=0;
				}
				if (p_SymInitialize(hProcess,exepath,TRUE))
				{
					{
						IMAGEHLP_MODULE mod;
						memset(&mod,0,sizeof(mod));
						mod.SizeOfStruct = sizeof(mod);
						if (p_SymGetModuleInfo(hProcess,address,&mod))
						{
							sprintf(temp,"Crash location: \"%s\", loaded at %08Xh - %08Xh\n",mod.ModuleName,mod.BaseOfImage,mod.BaseOfImage+mod.ImageSize);
							WriteFileString(hFile,temp);
						}
						else
						{
							sprintf(temp,"Unable to identify crash location\n");
							WriteFileString(hFile,temp);
						}
					}				

					{
						union
						{
							char buffer[128];
							IMAGEHLP_SYMBOL symbol;
						};
						memset(buffer,0,sizeof(buffer));
						symbol.SizeOfStruct = sizeof(symbol);
						symbol.MaxNameLength = buffer + sizeof(buffer) - symbol.Name;
						DWORD offset = 0;
						if (p_SymGetSymFromAddr(hProcess,address,&offset,&symbol))
						{
							buffer[tabsize(buffer)-1]=0;
							if (symbol.Name[0])
							{
								sprintf(temp,"Symbol: \"%s\" (+%08Xh)\n",symbol.Name,offset);
								WriteFileString(hFile,temp);
							}
						}
					}

					WriteFileString(hFile,"\nLoaded modules:\n");
					p_SymEnumerateModules(hProcess,EnumModulesCallback,hFile);

					
					call_stack_parse(param->ContextRecord->Esp,hFile,temp,hProcess);

					p_SymCleanup(hProcess);
					imagehelp_succeeded = true;
				}
			}
			if (!imagehelp_succeeded)
			{
				WriteFileString(hFile,"Failed to get module/symbol info.\n");
			}

			WriteFileString(hFile,"\nVersion info: \n");
			WriteFileString(hFile,version_string);
			WriteFileString(hFile,"\n");
			WriteFileString(hFile,
	#ifdef UNICODE
				"UNICODE"
	#else
				"ANSI"
	#endif
				);

			CloseHandle(hFile);
			ShellExecute(0,0,log_path,0,0,SW_SHOW);
		}
	}
}

//not really being utf8 here, all chars should be in 128 ascii range
UTF8API_EXPORT UINT uPrintCrashInfo(LPEXCEPTION_POINTERS param,const char * extra_info,char * outbase)
{
	uDumpCrashInfo(param);
	if (extra_info==0) extra_info = g_thread_call_stack;
	char * out = outbase;
	long number = InterlockedIncrement(&crash_no);

	unsigned address = (unsigned)param->ExceptionRecord->ExceptionAddress;
	out += sprintf(out,"Illegal operation:\nCode: %08Xh, flags: %08Xh, address: %08Xh",param->ExceptionRecord->ExceptionCode,param->ExceptionRecord->ExceptionFlags,address);

	if (param->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION && param->ExceptionRecord->NumberParameters>=2)
	{
		out += sprintf(out,"\nAccess violation, operation: %s, address: %08Xh",param->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",param->ExceptionRecord->ExceptionInformation[1]);
	}

	{
		bool imagehelp_succeeded = false;
		insync(g_imagehelp_sync);
		if (load_imagehelp())
		{
			HANDLE hProcess = GetCurrentProcess();
			char exepath[MAX_PATH];
			exepath[0]=0;
			GetModuleFileNameA(0,exepath,tabsize(exepath));
			exepath[tabsize(exepath)-1]=0;
			{
				char * ptr = exepath + strlen(exepath);
				while(ptr>exepath && *ptr!='\\') ptr--;
				*ptr=0;
			}
			if (p_SymInitialize(hProcess,exepath,TRUE))
			{
				{
					IMAGEHLP_MODULE mod;
					memset(&mod,0,sizeof(mod));
					mod.SizeOfStruct = sizeof(mod);
					if (p_SymGetModuleInfo(hProcess,address,&mod))
					{
						out += sprintf(out,"\nCrash location: \"%s\", loaded at %08Xh - %08Xh",mod.ModuleName,mod.BaseOfImage,mod.BaseOfImage+mod.ImageSize);
					}
					else
					{ 
						out += sprintf(out,"\nUnable to identify crash location");
					}
				}

				{
					union
					{
						char buffer[128];
						IMAGEHLP_SYMBOL symbol;
					};
					memset(buffer,0,sizeof(buffer));
					symbol.SizeOfStruct = sizeof(symbol);
					symbol.MaxNameLength = buffer + sizeof(buffer) - symbol.Name;
					DWORD offset = 0;
					if (p_SymGetSymFromAddr(hProcess,address,&offset,&symbol))
					{
						buffer[tabsize(buffer)-1]=0;
						if (symbol.Name[0])
						{
							out += sprintf(out,"\nSymbol: \"%s\" (+%08Xh)",symbol.Name,offset);
						}
					}
				}

				p_SymCleanup(hProcess);
				imagehelp_succeeded = true;
			}
		}
		if (!imagehelp_succeeded)
		{
			out += sprintf(out,"\n(failed to get module/symbol info)");
		}
		if (extra_info && *extra_info)
		{
			unsigned len = strlen_max(extra_info,256);
			out += sprintf(out,"\nAdditional info: ");
			memcpy(out,extra_info,len);
			out+=len;
			*out=0;
		}
		if (number==1)
		{
			out += sprintf(out,"\nThis is the first crash logged by this instance.");
		}
		else
		{
			const char * meh;
			switch(number%10)
			{
			case 1:
				meh = "st";
				break;
			case 2:
				meh = "nd";
				break;
			case 3:
				meh = "rd";
				break;
			default:
				meh = "th";
				break;
			}
			out += sprintf(out,"\nThis is your %u-%s crash. When reporting the problem to a developer, please try to post info about the first crash instead.",number,meh);
		}
	}

	out += sprintf(out,"\n");
	out = hexdump8(out,address,"Code bytes",-2,+2);

	return strlen(outbase);
}

static LPTOP_LEVEL_EXCEPTION_FILTER filterproc;

static LONG WINAPI exceptfilter(LPEXCEPTION_POINTERS param)
{
	uDumpCrashInfo(param);
	ExitProcess(0);
	return filterproc(param);//never called
}

UTF8API_EXPORT void uPrintCrashInfo_Init(const char * name)//called only by exe on startup
{
	version_string = name;
	filterproc = SetUnhandledExceptionFilter(exceptfilter);
}

static void callstack_add(const char * param)
{
	enum { MAX = tabsize(g_thread_call_stack) - 1} ;
	unsigned len = strlen(param);
	if (g_thread_call_stack_length + len > MAX) len = MAX - g_thread_call_stack_length;
	if (len>0)
	{
		memcpy(g_thread_call_stack+g_thread_call_stack_length,param,len);
		g_thread_call_stack_length += len;
		g_thread_call_stack[g_thread_call_stack_length]=0;
	}
}

uCallStackTracker::uCallStackTracker(const char * name)
{
	param = g_thread_call_stack_length;
	if (g_thread_call_stack_length>0) callstack_add("=>");
	callstack_add(name);
}

uCallStackTracker::~uCallStackTracker()
{
	g_thread_call_stack_length = param;
	g_thread_call_stack[param]=0;

}

extern "C" {UTF8API_EXPORT const char * uGetCallStackPath() {return g_thread_call_stack;} }