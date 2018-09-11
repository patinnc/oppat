/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

// cl /EHsc win_send_signal.cpp
// usage:
// start wait.exe fl1 fl2
// type wait.pid.txt  # to see pid of wait.exe
// win_send_signal.exe pid # where pid is from wait.pid.txt
#include <windows.h>
#include <stdio.h>

#include <iostream>
#include <cstdio>

// Inspired from http://stackoverflow.com/a/15281070/1529139
// and http://stackoverflow.com/q/40059902/1529139
bool signalCtrl(DWORD dwProcessId, DWORD dwCtrlEvent)
{
    bool success = false;
    DWORD thisConsoleId = GetCurrentProcessId();
    // Leave current console if it exists
    // (otherwise AttachConsole will return ERROR_ACCESS_DENIED)
    bool consoleDetached = (FreeConsole() != FALSE);

    if (AttachConsole(dwProcessId) != FALSE)
    {
        // Add a fake Ctrl-C handler for avoid instant kill is this console
        // WARNING: do not revert it or current program will be also killed
        SetConsoleCtrlHandler(nullptr, true);
        success = (GenerateConsoleCtrlEvent(dwCtrlEvent, 0) != FALSE);
		std::cout << "GenConsole event returned: " << success << std::endl;
        FreeConsole();
    }

    if (consoleDetached)
    {
        // Create a new console if previous was deleted by OS
        if (AttachConsole(thisConsoleId) == FALSE)
        {
            int errorCode = GetLastError();
            if (errorCode == 31) // 31=ERROR_GEN_FAILURE
            {
                AllocConsole();
            }
        }
    }
    return success;
}

int main(int argc, char **argv)
{

	if (argc < 2) {
		printf("need pid to which to send ctrl+c. bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	DWORD dwProcessId = atoi(argv[1]);
	printf("got arg pid= %d at %s %d\n", dwProcessId, __FILE__, __LINE__);
	if (signalCtrl(dwProcessId, CTRL_C_EVENT))
	{
		std::cout << "Signal sent" << std::endl;
	}
}

