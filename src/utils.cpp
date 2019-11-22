/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#ifdef _WIN32
#include <windows.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <sys/time.h>
#include <limits.h>
#define _strdup strdup
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <string.h>

#include <time.h>
#include <stdio.h>
#include <chrono>
#include <iostream>
#include <chrono>
#include <vector>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#include <stdint.h>
#define UTILS_CPP
#include "utils.h"

volatile int got_signal = 0;

void set_signal(void)
{
	got_signal = 1;
}

int get_signal(void)
{
	return got_signal;
}

static char *root_dir_of_exe = NULL;

char *get_root_dir_of_exe(void)
{
	return root_dir_of_exe;
}

#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif

static char *get_root_dir(char *in_path)
{
	int i, len;
	static char new_name[MAX_PATH];
	// could use sz = GetModuleFileName(NULL, out_path, out_path_size);
	strncpy(new_name, in_path, sizeof(new_name));
	printf("old_name= %s at %s %d\n", new_name, __FILE__, __LINE__);
	new_name[MAX_PATH-1] = 0;
	len = (int)strlen(new_name);
	for(i=len-1; i >= 0; i--)
	{
		if(new_name[i] == '/' || new_name[i] == '\\')
		{
			if(i < len)
			{
				new_name[i+1] = 0;
			}
			break;
		}
	}
	if(i < 0)
	{
		strncpy(new_name, "." DIR_SEP, sizeof(new_name));
	}
    printf("new_name= %s at %s %d\n", new_name, __FILE__, __LINE__);
	return new_name;
}

char *set_root_dir_of_exe(char *pth)
{
#ifdef _WIN32
    char szPath[MAX_PATH];

    if( !GetModuleFileNameA( NULL, szPath, MAX_PATH ) )
    {
        printf("Cannot get path for %s at %s %d\n", pth, __FILE__, __LINE__);
        return NULL;
    }
	root_dir_of_exe = _strdup(get_root_dir(szPath));

#else
	if(root_dir_of_exe != NULL)
	{
		free(root_dir_of_exe);
	}
	root_dir_of_exe = _strdup(get_root_dir(pth));
#endif
	return root_dir_of_exe;
}

#ifdef _WIN32
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif
 
#if 0
#if defined __cplusplus
extern "C" { /* Begin "C" */
/* Intrinsics use C name-mangling.  */
#endif /* __cplusplus */
#endif

static double QP_freq_inv = 0.0;
static int64_t QP_initial = 0;


/* return 0 if file name exists
 */

uint32_t build_bit_ui(int beg, int end)
{
	uint32_t myll=0;
	if(end == 31) 
	{
		myll = (uint32_t)(-1);
	}
	else
	{
		myll = (1<<(end+1)) - 1;
	}
	myll = myll >> beg;
	return myll;
}

uint32_t extract_bits_ui(uint32_t myin, int beg, int end)
{
	uint32_t myll=0;
	int beg1, end1;

	// Let the user reverse the order of beg & end.  
	if(beg <= end)
	{
		beg1 = beg;
		end1 = end;
	}
	else
	{
		beg1 = end;
		end1 = beg;
	}
	myll = myin >> beg1;
	myll = myll & build_bit_ui(beg1, end1);
	return myll;
}

uint64_t build_bit(int beg, int end)
{
	uint64_t myll=0;
	if(end == 63) 
	{
		myll = (uint64_t)(-1);
	}
	else
	{
		myll = ( 1LL << (end+1)) - 1;
	}
	myll = myll >> beg;
	return myll;
}


uint64_t extract_bits(uint64_t myin, int beg, int end)
{
	uint64_t myll=0;
	int beg1, end1;

	// Let the user reverse the order of beg & end.  
	if(beg <= end)
	{
		beg1 = beg;
		end1 = end;
	}
	else
	{
		beg1 = end;
		end1 = beg;
	}
	myll = myin >> beg1;
	myll = myll & build_bit(beg1, end1);
	return myll;
}


static void QP_init(void)
{
    LARGE_INTEGER li;
    if(!QueryPerformanceFrequency(&li)) {
        printf("QueryPerformanceFrequency issues. Bye at %s %d\n", __FILE__, __LINE__);
        exit(1);
    }

    double freq = (double)li.QuadPart;
    QP_freq_inv = 1.0/freq;

    QueryPerformanceCounter(&li);
    QP_initial = li.QuadPart;
}
static double QP_diff(void)
{
    static bool first_time=true;
    if (first_time) {
        QP_init();
        first_time = false;
    }
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return QP_freq_inv * (double)(li.QuadPart-QP_initial);
}


#if 0
   using elapsed_resolution = std::chrono::milliseconds;

    Stopwatch()
    {
        Reset();
    }

    void Reset()
    {
        reset_time = clock.now();
    }

    elapsed_resolution Elapsed()
    {
        return std::chrono::duration_cast<elapsed_resolution>(clock.now() - reset_time);
	}
#endif


double getfiletime(void)
{
    FILETIME    file_time;
    uint64_t    time;
	double x;

#if 1
    GetSystemTimePreciseAsFileTime(&file_time); // for win 8+... gets link err on win7
    //GetSystemTimeAsFileTime(&file_time); // for win7
#else
    SYSTEMTIME  system_time;
    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
#endif
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;
	x = (double)time;
	return 1.0e-7 * x;
}

#endif

double dclock(void)
{
#ifdef _WIN32
    static double gtod_initial=0.0;
    if (gtod_initial == 0.0) {
        //struct timeval tv;
        //gtod_initial = (double)tv.tv_sec + 1.0e-6 * (double)(tv.tv_usec);
		gtod_initial = getfiletime();
    }
    return gtod_initial + QP_diff();
#else
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (double)(tp.tv_sec) + 1.0e-9 * (double)(tp.tv_nsec);
#endif
}

#ifndef _WIN32
#define _snprintf snprintf
#endif

int ck_filename_exists(const char *filename, const char *file, int line, int verbose)
{
	char flnm[MAX_PATH];
	size_t len;
	int rc;
#if defined(__linux__) || defined(__APPLE__)
	struct stat stat_buffer;
#else
	struct _stat64 stat_buffer;
#endif
	if(filename == NULL || *filename == 0)
	{
		return -1;
	}
	len = strlen(filename)+1;
	len = (len > MAX_PATH  ? MAX_PATH : len);
	memset(flnm, 0, len);
	_snprintf(flnm, len, "%s", filename);
	//printf("filenm= %s at %s %d\n", flnm, __FILE__, __LINE__);
#if defined(__linux__) || defined(__APPLE__)
	rc = stat( flnm, &stat_buffer );
#else
	rc = _stat64( flnm, &stat_buffer );
#endif
	if(verbose > 0)
	{
		if(rc != 0)
		{
			fprintf(stderr, "File %s not found. Called by file= %s line= %d. Error at %s %d\n", 
					flnm, file, line, (char *)__FILE__, __LINE__);
		}
	}
	return rc;
}

uint64_t get_file_size(const char *filename, const char *file, int line, int verbose)
{
	char flnm[MAX_PATH];
	size_t len;
	int rc;
#if defined(__linux__) || defined(__APPLE__)
	struct stat stat_buffer;
#else
	struct _stat64 stat_buffer;
#endif
	if(filename == NULL || *filename == 0)
	{
		return -1;
	}
	len = strlen(filename)+1;
	len = (len > MAX_PATH  ? MAX_PATH : len);
	memset(flnm, 0, len);
	_snprintf(flnm, len, "%s", filename);
	//printf("filenm= %s at %s %d\n", flnm, __FILE__, __LINE__);
#if defined(__linux__) || defined(__APPLE__)
	rc = stat( flnm, &stat_buffer );
#else
	rc = _stat64( flnm, &stat_buffer );
#endif
	if(verbose > 0)
	{
		if(rc != 0)
		{
			fprintf(stderr, "File %s not found. Called by file= %s line= %d. Error at %s %d\n", 
					flnm, file, line, (char *)__FILE__, __LINE__);
			return -1;
		}
	}
	if(rc != 0)
	{
		return -1;
	}
	return stat_buffer.st_size;
}

int run_heapchk(const char *prefx, const char *file, int line, int verbose)
{
   int rc = 0;
#ifdef _WIN32
	int heapstatus;
	// Check heap status
	heapstatus = _heapchk();
	switch( heapstatus )
	{
	case _HEAPOK:
		if (verbose)
			printf("heapchk: %s: OK - heap is fine: called from %s %d. at %s %d\n", prefx, file, line, __FILE__, __LINE__);
		rc = 0;
		break;
	case _HEAPEMPTY:
		if (verbose)
			printf("heapchk: %s: OK - heap is empty: called from %s %d. at %s %d\n", prefx, file, line, __FILE__, __LINE__);
		rc = 0;
		break;
	case _HEAPBADBEGIN:
		if (verbose)
			printf("heapchk: %s: ERROR - bad start of heap: called from %s %d. at %s %d\n", prefx, file, line, __FILE__, __LINE__);
		rc = -1;
		break;
	case _HEAPBADNODE:
		if (verbose)
			printf("heapchk: %s: ERROR - bad node in heap: called from %s %d. at %s %d\n", prefx, file, line, __FILE__, __LINE__);
		rc = -1;
		break;
	}
#endif
	return rc;
}

#if 0
#if defined __cplusplus
}
/* Intrinsics use C name-mangling.  */
#endif /* __cplusplus */
#endif
