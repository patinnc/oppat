/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

// use win_send_signal.exe instead of below
// cl /EHsc win_terminate_process.cpp
// based on https://support.microsoft.com/en-us/help/178893/how-to-terminate-an-application-cleanly-in-win32
//******************
//Header
//******************

#include <windows.h>
#include <stdio.h>
#include <iostream>

#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "User32.lib")


#define TA_FAILED 0
#define TA_SUCCESS_CLEAN 1
#define TA_SUCCESS_KILL 2
#define TA_SUCCESS_16 3

DWORD WINAPI TerminateApp( DWORD dwPID, DWORD dwTimeout ) ;
DWORD WINAPI Terminate16App( DWORD dwPID, DWORD dwThread,
					WORD w16Task, DWORD dwTimeout );

//******************
//Source
//******************

//#include "TermApp.h"
#include <vdmdbg.h>

typedef struct
{
  DWORD   dwID ;
  DWORD   dwThread ;
} TERMINFO ;

// Declare Callback Enum Functions.
BOOL CALLBACK TerminateAppEnum( HWND hwnd, LPARAM lParam ) ;

BOOL CALLBACK Terminate16AppEnum( HWND hwnd, LPARAM lParam ) ;

/*----------------------------------------------------------------
DWORD WINAPI TerminateApp( DWORD dwPID, DWORD dwTimeout )

Purpose:
  Shut down a 32-Bit Process (or 16-bit process under Windows 95)

Parameters:
  dwPID
	 Process ID of the process to shut down.

  dwTimeout
	 Wait time in milliseconds before shutting down the process.

Return Value:
  TA_FAILED - If the shutdown failed.
  TA_SUCCESS_CLEAN - If the process was shutdown using WM_CLOSE.
  TA_SUCCESS_KILL - if the process was shut down with
	 TerminateProcess().
  NOTE:  See header for these defines.
----------------------------------------------------------------*/ 
DWORD WINAPI TerminateApp( DWORD dwPID, DWORD dwTimeout )
{
  HANDLE   hProc ;
  DWORD   dwRet ;

  // If we can't open the process with PROCESS_TERMINATE rights,
  // then we give up immediately.
  hProc = OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE, FALSE,
	 dwPID);

  if(hProc == NULL)
  {
	 return TA_FAILED ;
  }

  // TerminateAppEnum() posts WM_CLOSE to all windows whose PID
  // matches your process's.
  EnumWindows((WNDENUMPROC)TerminateAppEnum, (LPARAM) dwPID) ;

  // Wait on the handle. If it signals, great. If it times out,
  // then you kill it.
  if(WaitForSingleObject(hProc, dwTimeout)!=WAIT_OBJECT_0)
	 dwRet=(TerminateProcess(hProc,0)?TA_SUCCESS_KILL:TA_FAILED);
  else
	 dwRet = TA_SUCCESS_CLEAN ;

  CloseHandle(hProc) ;

  return dwRet ;
}

/*----------------------------------------------------------------
DWORD WINAPI Terminate16App( DWORD dwPID, DWORD dwThread,
					WORD w16Task, DWORD dwTimeout )

Purpose:
  Shut down a Win16 APP.

Parameters:
  dwPID
	 Process ID of the NTVDM in which the 16-bit application is
	 running.

  dwThread
	 Thread ID of the thread of execution for the 16-bit
	 application.

  w16Task
	 16-bit task handle for the application.

  dwTimeout
	 Wait time in milliseconds before shutting down the task.

Return Value:
  If successful, returns TA_SUCCESS_16
  If unsuccessful, returns TA_FAILED.
  NOTE:  These values are defined in the header for this
  function.

NOTE:
  You can get the Win16 task and thread ID through the
  VDMEnumTaskWOW() or the VDMEnumTaskWOWEx() functions.
----------------------------------------------------------------*/ 
DWORD WINAPI Terminate16App( DWORD dwPID, DWORD dwThread,
					WORD w16Task, DWORD dwTimeout )
{
  HINSTANCE      hInstLib ;
  TERMINFO      info ;

  // You will be calling the functions through explicit linking
  // so that this code will be binary compatible across
  // Win32 platforms.
  BOOL (WINAPI *lpfVDMTerminateTaskWOW)(DWORD dwProcessId,
	 WORD htask) ;

  hInstLib = LoadLibraryA( "VDMDBG.DLL" ) ;
  if( hInstLib == NULL )
	 return TA_FAILED ;

  // Get procedure addresses.
  lpfVDMTerminateTaskWOW = (BOOL (WINAPI *)(DWORD, WORD ))
	 GetProcAddress( hInstLib, "VDMTerminateTaskWOW" ) ;

  if( lpfVDMTerminateTaskWOW == NULL )
  {
	 FreeLibrary( hInstLib ) ;
	 return TA_FAILED ;
  }

  // Post a WM_CLOSE to all windows that match the ID and the
  // thread.
  info.dwID = dwPID ;
  info.dwThread = dwThread ;
  EnumWindows((WNDENUMPROC)Terminate16AppEnum, (LPARAM) &info) ;

  // Wait.
  Sleep( dwTimeout ) ;

  // Then terminate.
  lpfVDMTerminateTaskWOW(dwPID, w16Task) ;

  FreeLibrary( hInstLib ) ;
  return TA_SUCCESS_16 ;
}

BOOL CALLBACK TerminateAppEnum( HWND hwnd, LPARAM lParam )
{
  DWORD dwID ;

  GetWindowThreadProcessId(hwnd, &dwID) ;

  if(dwID == (DWORD)lParam)
  {
	  printf("got win for dwID= %d at %s %d\n", dwID, __FILE__, __LINE__);
	 PostMessage(hwnd, WM_CLOSE, 0, 0) ;
  }

  return TRUE ;
}

BOOL CALLBACK Terminate16AppEnum( HWND hwnd, LPARAM lParam )
{
  DWORD      dwID ;
  DWORD      dwThread ;
  TERMINFO   *termInfo ;

  termInfo = (TERMINFO *)lParam ;

  dwThread = GetWindowThreadProcessId(hwnd, &dwID) ;

  if(dwID == termInfo->dwID && termInfo->dwThread == dwThread )
  {
	 PostMessage(hwnd, WM_CLOSE, 0, 0) ;
  }

  return TRUE ;
}

#include <utility>

HWND FindTopWindow(DWORD pid)
{
    std::pair<HWND, DWORD> params = { 0, pid };

    // Enumerate the windows using a lambda to process each window
    BOOL bResult = EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL 
    {
        auto pParams = (std::pair<HWND, DWORD>*)(lParam);

        DWORD processId;
        if (GetWindowThreadProcessId(hwnd, &processId) && processId == pParams->second)
        {
            // Stop enumerating
            SetLastError(-1);
            pParams->first = hwnd;
            return FALSE;
        }

        // Continue enumerating
        return TRUE;
    }, (LPARAM)&params);

    if (!bResult && GetLastError() == -1 && params.first)
    {
        return params.first;
    }

    return 0;
}



int main(int argc, char **argv)
{

	if (argc < 2) {
		printf("need pid to which to send ctrl+c. bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	DWORD dwPID = atoi(argv[1]);
	DWORD tmout = 1000, rc;
	if (argc > 2) {
		tmout = atoi(argv[2]);
	}

	printf("got arg pid= %d tmout= %d ms at %s %d\n", dwPID, tmout, __FILE__, __LINE__);
	HWND hwin = FindTopWindow(dwPID);
	std::cout << "got hwin= " << hwin << std::endl;

	rc = TerminateApp( dwPID, tmout );
#define TA_FAILED 0
#define TA_SUCCESS_CLEAN 1
#define TA_SUCCESS_KILL 2
#define TA_SUCCESS_16 3
	switch(rc) {
		case TA_FAILED:
			printf("rc= TA_FAILED\n");
			break;
		case TA_SUCCESS_CLEAN:
			printf("rc= TA_SUCCESS_CLEAN\n");
			break;
		case TA_SUCCESS_KILL:
			printf("rc= TA_SUCCESS_KILL\n");
			break;
		case TA_SUCCESS_16:
			printf("rc= TA_SUCCESS_16\n");
			break;
		default:
			printf("rc= %d default...\n", rc);
			break;
	}
	return 0;
}
