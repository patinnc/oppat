
#include <stdio.h>
#include <string.h>
#include <string>

#include "trace_marker.h"

#ifdef _WIN32
#include <windows.h> 
//Define the function prototype
typedef int (CALLBACK* EtwSetMarkType)(unsigned long sessionHandle, const char *, int len);


#if defined __cplusplus
extern "C" { /* Begin "C" */
/* Intrinsics use C name-mangling.  */
#endif /* __cplusplus */
	extern int ETWSetMark(unsigned long sessionHandle, const char *, int len);
#if defined __cplusplus
}
/* Intrinsics use C name-mangling.  */
#endif /* __cplusplus */
#define  STR_SZ 2048
struct {
	int len;
	char str[STR_SZ];
} etw;

int trace_marker_write(std::string str)
{
	enum {TRY_OPEN=0, OPEN_WORKED, OPEN_FAILED};
	static int open_state = TRY_OPEN;
	static EtwSetMarkType EtwSetMarkPtr = NULL;
	static HINSTANCE dllHandle = NULL;              

	if (open_state == TRY_OPEN) {
		BOOL freeResult;
		dllHandle = LoadLibrary("ntdll.dll");
		// If the handle is valid, try to get the function address. 
		if (NULL == dllHandle) {
			open_state = OPEN_FAILED;
			fprintf(stderr, "LoadLibrary(ntdll.dll) failed at %s %d\n", __FILE__, __LINE__);
		} else {
			//Get pointer to our function using GetProcAddress:
			EtwSetMarkPtr = (EtwSetMarkType)GetProcAddress(dllHandle, "EtwSetMark");
			if (NULL != EtwSetMarkPtr) {
				printf("found EtwSetMark routine in ntdll.dll at %s %d\n", __FILE__, __LINE__);
				open_state = OPEN_WORKED;
			} else {
				open_state = OPEN_FAILED;
			}
		}
	}
	if (open_state == OPEN_WORKED) {
		if (str.size() < STR_SZ) {
			strcpy(etw.str, str.c_str());
		} else {
			strcpy(etw.str, str.substr(0, STR_SZ-2).c_str());
		}
		etw.len = strlen(etw.str);
		int ulen = etw.len+sizeof(etw.len)+1;
		int rc = EtwSetMarkPtr(0, (const char *)&etw, ulen);
		//printf("rc= %d, ulen= %d at %s %d\n", rc, ulen, __FILE__, __LINE__);
	}
	//Free the library:
	//freeResult = FreeLibrary(dllHandle);       
	return 0;
}

#if 0
   HINSTANCE dllHandle = NULL;              
   FindArtistType FindArtistPtr = NULL;

   //Load the dll and keep the handle to it
   dllHandle = LoadLibrary("art.dll");

   // If the handle is valid, try to get the function address. 
   if (NULL != dllHandle) 
   { 
      //Get pointer to our function using GetProcAddress:
      FindArtistPtr = (FindArtistType)GetProcAddress(dllHandle,
         "FindArtist");

      // If the function address is valid, call the function. 
      if (runTimeLinkSuccess = (NULL != FindArtistPtr))
      {
         LPCTSTR myArtist = "Duchamp";
         short retVal = FindArtistPtr(myArtist);
      }

      //Free the library:
      freeResult = FreeLibrary(dllHandle);       
   }
#endif
/*
 * from http://geekswithblogs.net/akraus1/Default.aspx
 <summary>
 Undocumented method of the kernel to write an ETW marker which does show
 up in the WPA viewer under the Marks tab. This makes navigation in trace files
 much easier to find important time marks.
 This is the same as xperf -m "string" which writes only to the "NT Kernel Logger" session.
 </summary>
 <param name="sessionHandle">if 0 the NT Kernel Logger Session is written to. Otherwise you need to use supply a handle to a kernel ETW session.</param>
 <param name="pStr">Ascii encoded string with a DWORD prefix</param>
 <param name="length">Length of complete string including the prefix and terminating \0 char</param>
 <returns>0 on success, otherwise a WIN32 error code</returns>
 [DllImport("ntdll.dll")]
 extern unsafe static int EtwSetMark(ulong sessionHandle, ETWString* pStr, int length);
*/
#endif
#ifdef __linux__
int trace_marker_write(std::string str)
{
	enum {TRY_OPEN=0, OPEN_WORKED, OPEN_FAILED};
	static int open_state = TRY_OPEN;
	static std::ofstream ofile2;
	const std::string mrkr = "/sys/kernel/debug/tracing/trace_marker";
	if (open_state == TRY_OPEN) {
		ofile2.open (mrkr.c_str(), std::ios::out);
		if (!ofile2.is_open()) {
			fprintf(stderr, "messed up open of trace marker file flnm= %s at %s %d\n", mrkr.c_str(), __FILE__, __LINE__);
			fprintf(stdout, "messed up open of trace marker file flnm= %s at %s %d\n", mrkr.c_str(), __FILE__, __LINE__);
			open_state = OPEN_FAILED;
			return 0;
			//exit(1);
		} else {
			open_state = OPEN_WORKED;
		}
	}
	if (open_state == OPEN_WORKED) {
		ofile2.write((const char *)str.c_str(), str.size());
	}
	//ofile2.close();
}

#endif
