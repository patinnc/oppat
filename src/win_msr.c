
// cl win_msr.cpp

/*
 *  The BSD 3-clause "New" or "Revised" License
 *  Copyright (c) 2014, Intel Corp., Patrick Fay
 *  All rights reserved.
 *  
 *  Redistribution and use in source and binary forms, with or without modification,
 *  are permitted provided that the following conditions are met:
 *  
 *      Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *      Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *      Neither the name of the Intel Corp. nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *  
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 *  WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#ifndef BUILD_MAIN
#define BUILD_MAIN 0  // if 0 don't build main(), if 1 then build the main() routine. If building as lib, set to 0
#endif

#pragma warning(disable : 4996)


#ifdef _WIN32
//#include "stdafx.h"
#include <windows.h>
#include <wtypes.h>
#include <winbase.h>
#include <shlwapi.h>
#include <conio.h>
#include <intrin.h>  
#include "OlsDef.h"
#include "OlsApiInit.h"
#endif
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#pragma intrinsic(__rdtsc)
#include "utils.h"

#define WIN_MSR_C
#include "win_msr.h"

#ifdef _AMD64_
#define AFF_TYPE uint64_t
#else
#define AFF_TYPE uint32_t
#endif

#ifndef DWORD
#define DWORD uint32_t
#endif

#ifndef DWORD_PTR
#define DWORD_PTR uint32_t *
#endif

#ifdef IA64
int my_number_of_cpus = 0;
#endif

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>


#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <sys/timeb.h>
#include <time.h>
#include <stdlib.h>
#include <memory.h>
//#include "cpuid.h"
//#include "cpuid_supported_models.h"

#ifdef _M_X64
#define LNX_PTR2INT uint64_t
#define LNX_MY1CON 1LL
#else
#define LNX_PTR2INT uint32_t
#define LNX_MY1CON 1
#endif

#ifndef BOOL
#define BOOL int
#endif
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

enum {
	CHECK_SLM_NO=0,
	CHECK_SLM_YES=1
};

#ifdef _WIN32
static LNX_PTR2INT processor_mask= -LNX_MY1CON;
static HMODULE m_hOpenLibSys;  // handle for WinRing0 driver dll

//#define REG64_MAX_SIZE 512
static uint64_t Reg64[REG64_MAX_SIZE];

BOOL do_read_msr( int *onum_cpus, uint32_t numMsr, uint32_t *MsrList, uint64_t pMsr[][REG64_MAX_SIZE])
{
	int num_cpus, i;
	uint32_t j;
	static int first_time = 0;
	BOOL brc = TRUE;
	static int last_cpu_used = -1;
	uint64_t oReg64[REG64_MAX_SIZE];
	LNX_PTR2INT  dwSystemAffinity=0;
	DWORD_PTR dwptr;
	union cvt_ds
	{
		uint64_t ui64;
		struct
		{
			DWORD low;
			DWORD high;
		} ui32;
	} cvt;


	num_cpus  = 1;

	for(i=0; i < num_cpus; i++) {
		//BOOL brc;
#if 0
		if(last_cpu_used == -1 || last_cpu_used != onum_cpus[i])
		{
			// this is an optimization to avoid resetting the affinity if the old affinity==new aff.
			// It will fail if anything outside this routine changes the affinity... because this rtn won't know it got 
			// changed. Probably safer to just set the affinity for each cpu.
			dwSystemAffinity = (LNX_MY1CON << onum_cpus[i]);
			last_cpu_used = onum_cpus[i];
			SetThreadAffinityMask( GetCurrentThread(), &dwSystemAffinity);
			//memcpy(&dwptr, &dwSystemAffinity, sizeof(dwptr));
		}
#else
		dwSystemAffinity = (LNX_MY1CON << onum_cpus[i]);
		SetThreadAffinityMask( GetCurrentThread(), dwSystemAffinity);
#endif
		//Reg64[i] = 0;
		if(processor_mask == -1 || (processor_mask & (LNX_MY1CON << i)) != 0)
		{
			for(j=0; j < numMsr; j++)
			{
				pMsr[j][i] = 0;
				//brc = RdmsrNEx(numMsr, MsrList, oReg64, dwptr);
				//brc = RdmsrTx(MsrList[j], &(cvt.ui32.low), &(cvt.ui32.high), dwptr);
				brc = Rdmsr(MsrList[j], &(cvt.ui32.low), &(cvt.ui32.high));
				if(brc == TRUE)
				{
					pMsr[j][i] = cvt.ui64;
				}
			}
		}
	}
	return brc; // probably should return nothing since we aren't doing any 
}

static BOOL do_init_msr(void)
{
    BOOL brc;

		BOOL brc2 = FALSE;
		m_hOpenLibSys = NULL;
		brc2 = InitOpenLibSys(&m_hOpenLibSys);
		brc = brc2;
		if(brc2 != TRUE)
		{
			//AfxMessageBox(_T("DLL Load Error!!"));
			printf("Driver init failed. WinRing0*.dll and WinRing0*.sys files must be in same dir as id_cpu executable.\n");
			fprintf(stderr, "got an error Initializing WinRing0 driver at %s %d\n", __FILE__, __LINE__);
			fprintf(stderr, "Driver init failed. WinRing0*.dll and WinRing0*.sys files must be in same dir as id_cpu executable.\n");
			fprintf(stderr, "On win2008 & vista64 OS you need to run id_cpu from a dos box started with 'run as admin' privilege.\n");
			return FALSE;
		}
		
	return TRUE;
	
}


#define MSR_RAPL_POWER_UNIT    0x606;
#define MSR_PKG_ENERGY_STATUS  0x611
#define MSR_PP0_ENERGY_STATUS  0x639
#define MSR_PP1_ENERGY_STATUS  0x641
#define IA32_PACKAGE_THERM_STATUS 0x1b1
#define MSR_TEMPERATURE_TARGET 0x1a2

#define INIT_NOT_DONE 0
#define INIT_FAILED   1
#define INIT_WORKED   2

static int did_init_msrs = INIT_NOT_DONE;
static double rapl_power_unit, rapl_energy_status_unit, rapl_time_unit;

static double get_unit(int check_silvermont, uint64_t msr, int beg, int end)
{
	uint64_t myll;
	double dbl;
	int ck_slm = CHECK_SLM_NO;
	myll = extract_bits(msr, beg, end);
	dbl = (double)myll;
	dbl = pow(2.0, dbl);
	if(check_silvermont == CHECK_SLM_YES)
	{
#if 0
        // TBD this needs to be done but... not yet
		UINT32 this_cpu_model = cpuid_get_cpu_model();
		int i = get_supported_cpu_model(this_cpu_model);
		if(i != -1 && supported_cpu_model[i].type == MODEL_TYPE_SLM2_PLUS)
		{
			// baytrail ESU factor update coming soon
		}
#endif
	}
	if(ck_slm == CHECK_SLM_NO)
	{
		if(dbl > 0.0) { dbl = 1.0/dbl; }
	}
	return dbl;

}

// returns 1 if Process is Elevated
static int IsProcessElevated(void)
{
	int rc;
	HANDLE hToken;
	DWORD tkInfoLen;
	TOKEN_ELEVATION tkElevation;

	OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &hToken);

	GetTokenInformation( hToken, TokenElevation, &tkElevation, sizeof(tkElevation), &tkInfoLen);

	rc = ( tkElevation.TokenIsElevated != 0 );
	//fprintf(stderr, "IsProcessElevated= %d\n", rc);
	return rc;
}


int ck_init_msrs(void)
{
	if(did_init_msrs == INIT_NOT_DONE)
	{
		if(FALSE == do_init_msr())
		{
			char winring0_filenm[MAX_PATH];
			/* reasons for failure:
			 * 1) WinRing0* files not found or
			 * 2) not running 'run as admin'
			 */
			printf("msr init failed\n");
			snprintf(winring0_filenm, sizeof(winring0_filenm)-1, "%s\\%s", get_root_dir_of_exe(), "winring0.sys");
			winring0_filenm[MAX_PATH-1] = 0;

			if(ck_filename_exists(winring0_filenm, __FILE__, __LINE__, 0) != 0)
			{
				MessageBox(NULL, "Failed to load WinRing0 drivers.\n"
						"Didn't find WinRing0*.sys or WinRing0*.dll in same dir as ippet.exe.\n"
						"Need WinRing0.sys, WinRing0.dll, WinRing0_x64.sys and WinRing0_x64.\n"
						"See the readme.txt file instructions on how to get the files. Bye.", "Error", MB_OK);
			}
			else if(IsProcessElevated() != 1)
			{
					MessageBox(NULL, "Failed to load WinRing0 drivers.\n"
						"You need to start ippet.exe with 'run as admin' privileges. Bye.", "Error", MB_OK);
			}
			else
			{
					MessageBox(NULL, "Failed to load WinRing0 drivers.\n"
						"Not sure why. Driver files are present and seem have 'run as admin' privilege."
						"Maybe wrong vesion of driver files? IPPET needs ver 1.3.x. Just guessing. Bye.", "Error", MB_OK);
			}
			exit(1);
			did_init_msrs = INIT_FAILED;
		}
		else
		{
			uint32_t Msr;
			int cpus;
			printf("msr init worked\n");
			did_init_msrs = INIT_WORKED;
			Msr = 0x10;
            Reg64[0] = 0;
			do_read_msr( &cpus, 1, &Msr, &Reg64);
			printf("read msr TSC 0x%x value= 0x%I64x\n", Msr, Reg64[0]);
			Msr = MSR_RAPL_POWER_UNIT;
			do_read_msr( &cpus, 1, &Msr, &Reg64);
			//printf("read msr MSR_RAPL_POWER_UNIT 0x%x value= 0x%I64x\n", Msr, Reg64[0]);
			rapl_power_unit         = get_unit(CHECK_SLM_NO,  Reg64[0],  0,  3);
			rapl_energy_status_unit = get_unit(CHECK_SLM_YES, Reg64[0],  8, 12);
			rapl_time_unit          = get_unit(CHECK_SLM_NO,  Reg64[0], 16, 19);
			//printf("rapl_power_unit= %f, rapl_energy_status_unit= %f, rapl_time_unit= %f\n",
			//	rapl_power_unit, rapl_energy_status_unit, rapl_time_unit);
		}
	}
	return did_init_msrs;
}

static uint64_t get_energy_val(uint32_t Msr)
{
	int cpus = 0;
	uint64_t MsrVal;
	double dbl;

	if(ck_init_msrs() != INIT_WORKED)
	{
		return 0.0;
	}
	do_read_msr( &cpus, 1, &Msr, &Reg64);
	Reg64[0] = extract_bits(Reg64[0], 0, 31);
	return Reg64[0];
}

static double get_pkg_temperature(uint32_t Msr)
{
	int cpus = 0;
	uint64_t MsrVal;
	double dbl;

	if(ck_init_msrs() != INIT_WORKED)
	{
		return 0.0;
	}
	do_read_msr( &cpus, 1, &Msr, &Reg64);
	dbl = (double)extract_bits(Reg64[0], 16, 22);
	return dbl;
}

static double get_pkg_tcc_activation(uint32_t Msr)
{
	int cpus = 0;
	uint64_t MsrVal;
	double dbl;
	if(ck_init_msrs() != INIT_WORKED)
	{
		return 0.0;
	}
	do_read_msr( &cpus, 1, &Msr, &Reg64);
	dbl = (double)extract_bits(Reg64[0], 16, 23);
	return dbl;
}


static uint64_t get_pkg_energy(void)
{
	return get_energy_val(MSR_PKG_ENERGY_STATUS);
}

static uint64_t get_core_energy(void)
{
	return get_energy_val(MSR_PP0_ENERGY_STATUS);
}

static uint64_t get_gfx_energy(void)
{
	return get_energy_val(MSR_PP1_ENERGY_STATUS);
}

// return tcc_activation
double get_tcc_activation(void)
{
	return get_pkg_tcc_activation(MSR_TEMPERATURE_TARGET);
}

int get_power_data(int power_sz, double *power, int tm_elap_sz, double *tm_elap, double cur_time, double *pkg_temp_headroom)
{
	static uint64_t energy[3][3];
	static int first_time = 0;
	int i;
	if(power_sz < POWER_SZ || tm_elap_sz < TM_ELAP_SZ)
	{
		return 0;
	}
	tm_elap[VAL_CUR] = cur_time;
	if(first_time == 0)
	{
		tm_elap[VAL_PRV] = tm_elap[VAL_CUR];
	}
	tm_elap[VAL_DIF] = tm_elap[VAL_CUR] - tm_elap[VAL_PRV];
	tm_elap[VAL_PRV] = tm_elap[VAL_CUR];

	energy[ENERGY_PKG][VAL_CUR] = get_pkg_energy();
	energy[ENERGY_COR][VAL_CUR] = get_core_energy();
	energy[ENERGY_GFX][VAL_CUR] = get_gfx_energy();
	*pkg_temp_headroom = get_pkg_temperature(IA32_PACKAGE_THERM_STATUS);
	if(first_time == 0)
	{
		for(i=0; i <= ENERGY_GFX; i++)
		{
			energy[i][VAL_PRV] = energy[i][VAL_CUR];
		}
	}
	for(i=0; i <= ENERGY_GFX; i++)
	{
		if(energy[i][VAL_CUR] >= energy[i][VAL_PRV])
		{
			energy[i][VAL_DIF] = energy[i][VAL_CUR] - energy[i][VAL_PRV];
		}
		else
		{
			// cntr is only 32 bits. will wrap eventually. if cur < prev then wrapped.
			uint64_t myll = (uint64_t)-1;
			myll = extract_bits(myll, 0, 31);
			energy[i][VAL_DIF] = myll - energy[i][VAL_PRV] + energy[i][VAL_CUR];
		}
		energy[i][VAL_PRV] = energy[i][VAL_CUR];
	}
	if(first_time > 0 && tm_elap[VAL_DIF] > 0.0)
	{
		double efactor;
		efactor = rapl_energy_status_unit/tm_elap[VAL_DIF];
		for(i=0; i <= ENERGY_GFX; i++)
		{
			power[i] = efactor * (double)(energy[i][VAL_DIF]);
		}
	}
	first_time = 1;
	return 1;
}

#if BUILD_MAIN == 1
int main(int argc, char **argv) {

    BOOL brc;
	uint64_t myll, Reg64[REG64_MAX_SIZE];
	uint32_t Msr;
	int cpus;
	double dbl, dprv, dff, dadd, dmxx;
	uint64_t myllx, mylla;

	if(ck_init_msrs() != INIT_WORKED)
	{
		printf("bye\n");
		return 1;
	}
	printf("msr init worked\n");
	Reg64[0] = 0;
	Msr = 0x10; // read the TSC msr
	do_read_msr( &cpus, 1, &Msr, &Reg64);
	printf("read msr 0x%x returned %I64x\n", Msr, Reg64[0]);

	printf("read MSR_PKG_ENERGY_STATUS  pkg_energy(J)= %f\n", get_pkg_energy());
	printf("read MSR_PP0_ENERGY_STATUS core_energy(J)= %f\n", get_core_energy());
	printf("read MSR_PP1_ENERGY_STATUS  gfx_energy(J)= %f\n", get_gfx_energy());

	return 0;
}
#endif
#else  // if not def _WIN32
int    get_power_data(int power_sz, double *power, int tm_elap_sz, double *tm_elap, double cur_time, double *pkg_temp_headroom) {
	return 0;
}
double get_tcc_activation(void) {
	return 0.0;
}
BOOL   do_read_msr( int *onum_cpus, uint32_t numMsr, uint32_t *MsrList, uint64_t pMsr[][REG64_MAX_SIZE]) {
	return 1;
}
int    ck_init_msrs(void) {
	return 1;
}
#endif
