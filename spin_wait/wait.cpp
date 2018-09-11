/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#ifdef _WIN32
#include <windows.h>
#include <Setupapi.h>
#define INITGUID
#include<batclass.h>
#include<setupapi.h>
#include<devguid.h>
#include <process.h>
#endif

#include <iostream>
#include <time.h>
#include <stdio.h>
//#include <arm_neon.h>
#include <ctime>
#include <cstdio>
#include <stdint.h>
#include <stdlib.h>
#include <thread>
#include <vector>
#include <string>
#include <time.h>
#ifdef __linux__
#include <unistd.h>
#endif
#include <signal.h>
#include <sys/types.h>

#include "utils.h"
#ifdef _WIN32
#if 0
	// don't need this yet. Not doing MSRs right now.
#include "win_msr.h"
#endif
#endif

enum {
	SYS_BAT,
};


struct sysfs_data_str {
	double ts;
	double val;
	int typ;
	sysfs_data_str(): ts(-1.0), val(-1.0), typ(-1) {}
};

#ifdef _WIN32
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   switch(message)
   {
   
      case WM_CLOSE:
			HRESULT msg;
		
			// !!!!!!!!!!! HERE
			fprintf(stderr, "got WM_CLOSE at %s %d\n", __FILE__, __LINE__);
			set_signal();
#if 0
			msg = MessageBox(hWnd, "Quit?", "yes/no", MB_YESNO);
//                     This checks if its been clicked yes or no
			if(msg == IDYES)
//             Destroy window sends WM_DESTROY message(see below)
         	DestroyWindow(hWnd);
#endif
         
      break;
      
      case WM_DESTROY:
			set_signal();
      
         	//PostQuitMessage(0);
      break;
      default:
         return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}
BOOL CtrlHandler( DWORD fdwCtrlType ) 
{ 
  switch( fdwCtrlType ) 
  { 
    // Handle the CTRL-C signal. 
    case CTRL_C_EVENT: 
      fprintf(stderr,  "Ctrl-C event\n\n" );
		set_signal();
      return( FALSE );
 
    // CTRL-CLOSE: confirm that the user wants to exit. 
    case CTRL_CLOSE_EVENT: 
      fprintf(stderr, "Ctrl-Close event\n\n" );
		set_signal();
      return( TRUE );
 
    // Pass other signals to the next handler. 
    case CTRL_BREAK_EVENT: 
      fprintf(stderr, "Ctrl-Break event\n\n" );
		set_signal();
      return( FALSE );
 
    case CTRL_LOGOFF_EVENT: 
		set_signal();
      printf( "Ctrl-Logoff event\n\n" );
      return FALSE; 
 
    case CTRL_SHUTDOWN_EVENT: 
		set_signal();
      printf( "Ctrl-Shutdown event\n\n" );
      return FALSE; 
 
    default: 
      return FALSE; 
  } 
} 
 
static std::vector <HANDLE> bat_vec;
static std::vector <PSP_DEVICE_INTERFACE_DETAIL_DATA> pdidd_vec;
static std::vector <BATTERY_QUERY_INFORMATION> bq_vec;
static std::vector <HDEVINFO> hdev_vec;
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "User32.lib")

// below from https://docs.microsoft.com/en-us/windows/desktop/Power/enumerating-battery-devices
DWORD GetBatteryState_init()
{
#define GBS_HASBATTERY 0x1
#define GBS_ONBATTERY  0x2
  // Returned value includes GBS_HASBATTERY if the system has a 
  // non-UPS battery, and GBS_ONBATTERY if the system is running on 
  // a battery.
  //
  // dwResult & GBS_ONBATTERY means we have not yet found AC power.
  // dwResult & GBS_HASBATTERY means we have found a non-UPS battery.

  DWORD dwResult = GBS_ONBATTERY;

  // IOCTL_BATTERY_QUERY_INFORMATION,
  // enumerate the batteries and ask each one for information.

  HDEVINFO hdev =
            SetupDiGetClassDevs(&GUID_DEVCLASS_BATTERY, 
                                0, 
                                0, 
                                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (INVALID_HANDLE_VALUE != hdev)
   {
    // Limit search to 100 batteries max
    hdev_vec.push_back(hdev);
    for (int idev = 0; idev < 100; idev++)
     {
      SP_DEVICE_INTERFACE_DATA did = {0};
      did.cbSize = sizeof(did);

      if (SetupDiEnumDeviceInterfaces(hdev,
                                      0,
                                      &GUID_DEVCLASS_BATTERY,
                                      idev,
                                      &did))
       {
        DWORD cbRequired = 0;

        SetupDiGetDeviceInterfaceDetail(hdev,
                                        &did,
                                        0,
                                        0,
                                        &cbRequired,
                                        0);
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
         {
          PSP_DEVICE_INTERFACE_DETAIL_DATA pdidd =
            (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR,
                                                         cbRequired);
          if (pdidd)
           {
               pdidd_vec.push_back(pdidd);
            pdidd->cbSize = sizeof(*pdidd);
            if (SetupDiGetDeviceInterfaceDetail(hdev,
                                                &did,
                                                pdidd,
                                                cbRequired,
                                                &cbRequired,
                                                0))
             {
              // Enumerated a battery.  Ask it for information.
              HANDLE hBattery = 
                      CreateFile(pdidd->DevicePath,
                                 GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL);
              if (INVALID_HANDLE_VALUE != hBattery)
              {
                // Ask the battery for its tag.
                BATTERY_QUERY_INFORMATION bqi = {0};
                bat_vec.push_back(hBattery);

                DWORD dwWait = 0;
                DWORD dwOut;

                if (DeviceIoControl(hBattery,
                                    IOCTL_BATTERY_QUERY_TAG,
                                    &dwWait,
                                    sizeof(dwWait),
                                    &bqi.BatteryTag,
                                    sizeof(bqi.BatteryTag),
                                    &dwOut,
                                    NULL)
                    && bqi.BatteryTag)
                 {
                  // With the tag, you can query the battery info.
                  BATTERY_INFORMATION bi = {0};
                  bqi.InformationLevel = BatteryInformation;

                  if (DeviceIoControl(hBattery,
                                      IOCTL_BATTERY_QUERY_INFORMATION,
                                      &bqi,
                                      sizeof(bqi),
                                      &bi,
                                      sizeof(bi),
                                      &dwOut,
                                      NULL))
                   {
                    // Only non-UPS system batteries count
                    if (bi.Capabilities & BATTERY_SYSTEM_BATTERY)
                     {
                      if (!(bi.Capabilities & BATTERY_IS_SHORT_TERM))
                       {
                        dwResult |= GBS_HASBATTERY;
                       }

                      // Query the battery status.
                      BATTERY_WAIT_STATUS bws = {0};
                      bws.BatteryTag = bqi.BatteryTag;
                      bq_vec.push_back(bqi);

                      BATTERY_STATUS bs;
                      if (DeviceIoControl(hBattery,
                                          IOCTL_BATTERY_QUERY_STATUS,
                                          &bws,
                                          sizeof(bws),
                                          &bs,
                                          sizeof(bs),
                                          &dwOut,
                                          NULL))
                       {
                        if (bs.PowerState & BATTERY_CHARGING) {
                            // online means not running from the battery?
                             printf("charging bat idev= %d, rate= %d\n", idev, bs.Rate);
                          //dwResult &= ~GBS_ONBATTERY;
                         } else {
                             printf("using bat idev= %d, rate= %d\n", idev, bs.Rate);
                         }
                       }
                     }
                   }
                 }
                //CloseHandle(hBattery);
               }
             }
            //LocalFree(pdidd);
           }
         }
       }
        else  if (ERROR_NO_MORE_ITEMS == GetLastError())
         {
          break;  // Enumeration failed - perhaps we're out of items
         }
     }
    //SetupDiDestroyDeviceInfoList(hdev);
   }

  //  Final cleanup:  If we didn't find a battery, then presume that we
  //  are on AC power.

  if (!(dwResult & GBS_HASBATTERY))
    dwResult &= ~GBS_ONBATTERY;

  return dwResult;
}
double GetBatteryState(std::vector <sysfs_data_str> &sysfs_vec)
{
    double tot=0.0;
    for (uint32_t i=0; i < bat_vec.size(); i++) {
        // Query the battery status.
        DWORD dwOut;
        BATTERY_WAIT_STATUS bws = {0};
        bws.BatteryTag = bq_vec[i].BatteryTag;

        BATTERY_STATUS bs;
        if (DeviceIoControl(bat_vec[i],
                IOCTL_BATTERY_QUERY_STATUS,
                &bws,
                sizeof(bws),
                &bs,
                sizeof(bs),
                &dwOut,
                NULL)) {
                double x = -1.0e3 * (double)bs.Rate; //Rate in mWatts
                sysfs_data_str sds;
                sds.ts = dclock();
                sds.val = x;
                sds.typ = SYS_BAT;
                sysfs_vec.push_back(sds);
                tot += x;
        }
    }
    return tot;
}

#endif
/*
 /sys/class/power_supply/BAT1/power_now
*/
#define BATTERY_FILE "/sys/class/power_supply/BAT1/power_now"

FILE *bat_open(std::string bat_flnm)
{
	FILE *fp;
	fp = fopen(bat_flnm.c_str(), "r");
	if (!fp) {
		printf("failed to open sysfs file for battery '%s' at %s %d\n", bat_flnm.c_str(), __FILE__, __LINE__);
	}
	return fp;
}


void sighandler(int sig)
{
	set_signal();
}

#if 0
// get from utils.cpp now
double dclock(void)
{
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (double)(tp.tv_sec) + 1.0e-9 * (double)(tp.tv_nsec);
}
#endif

int bat_read(FILE *fp_bat, std::vector <sysfs_data_str> &sysfs_vec, int verbose)
{
	char buf[256];
	if (!fp_bat) {
		return 1;
	}
	double tm = dclock();
	rewind(fp_bat);
	size_t bytes_read = fread(buf, sizeof(char), sizeof(buf), fp_bat);
	double x = atof(buf);
	if (verbose > 0)
		printf("tm= %f bat pwr= %f\n", tm, x);
	sysfs_data_str sds;
	sds.ts = tm;
	sds.val = x;
	sds.typ = SYS_BAT;
	sysfs_vec.push_back(sds);
	return 1;
}

double set_sleep_str(double secs, double fraction_of_sec, struct timespec &tm_slp)
{
	tm_slp.tv_sec = secs;
	tm_slp.tv_nsec = 1e9 * fraction_of_sec;
	return secs + fraction_of_sec;
}

int main(int argc, char **argv) {

	struct timespec tm_slp;
	double t_first = dclock();
	int pid = -1, verbose = 0;
	std::string flnm="prf_energy2.txt", stdo;
	FILE *fp=NULL, *fp_bat=NULL;
	std::vector <sysfs_data_str> sysfs_vec;
	const double slp_secs = 0, slp_msecs = 500;
	double slp_req_intrvl = set_sleep_str(slp_secs, 0.001 * slp_msecs, tm_slp);
	FILE *newstdo = stdout;

	if (argc > 2) {
		stdo = std::string(argv[2]);
		newstdo = freopen(stdo.c_str(),"w",stdout);
	}
	printf("t_first= %.9f\n", t_first);
	set_root_dir_of_exe(argv[0]);
#ifdef _WIN32
	int islp_msecs = (int)slp_msecs;
	//pid2 = (int)GetCurrentThreadId();
	//pid2 = (int)GetCurrentProcessId();
	//pid = (int)GetCurrentThreadId();
	pid = (int)_getpid();
	GetBatteryState_init();
#if 0
	// don't need this yet. Not doing MSRs right now.
	ck_init_msrs();
#endif
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE) CtrlHandler, TRUE );
    
#endif
#ifdef __linux__
	pid = getpid();
#endif

	signal(SIGABRT, &sighandler);
	signal(SIGTERM, &sighandler);
	signal(SIGINT, &sighandler);

#ifdef __linux__
	fp_bat = bat_open(std::string(BATTERY_FILE));
#endif

	fp = fopen("wait.pid.txt", "w");
	fprintf(fp, "%d\n", pid);
	fclose(fp);


	if (argc > 1) {
		flnm = std::string(argv[1]);
	}

	// spin till get killed
	double tm_beg = dclock();
	while(get_signal() == 0) {
#ifdef __linux__
		nanosleep(&tm_slp, NULL);
		bat_read(fp_bat, sysfs_vec, verbose);
#endif
#ifdef _WIN32
		Sleep(islp_msecs);
		GetBatteryState(sysfs_vec);
#endif
	}
	double tm_end = dclock();
	if (fp_bat) {
		fclose(fp_bat);
	}
	//     0.020185569	0.13	Joules	power/energy-pkg/	20134748	100.00				
	fp = fopen(flnm.c_str(), "w");
	for (uint32_t i=0; i < sysfs_vec.size(); i++) {
		double tm_off, tm_dura;
		tm_off = sysfs_vec[i].ts - t_first;
		if (i == 0) {
			tm_dura = sysfs_vec[i].ts - t_first;
		} else {
			tm_dura = sysfs_vec[i].ts - sysfs_vec[i-1].ts;
		}
		tm_dura *= 1.0e9;
		fprintf(fp, "     %.9f\t%.3f\twatts\tpower/battery/\t%.0f\t100.00\n", tm_off, 1.0e-6*sysfs_vec[i].val, tm_dura);
	}
	fclose(fp);
    printf("ran for %f seconds at %s %d\n", tm_end-tm_beg, __FILE__, __LINE__);

#ifdef _WIN32
    for (uint32_t i=0; i < bat_vec.size(); i++) {
        CloseHandle(bat_vec[i]);
    }
    for (uint32_t i=0; i < pdidd_vec.size(); i++) {
        LocalFree(pdidd_vec[i]);
    }
    for (uint32_t i=0; i < hdev_vec.size(); i++) {
        SetupDiDestroyDeviceInfoList(hdev_vec[i]);
    }
#endif
	
	return 0;
}

