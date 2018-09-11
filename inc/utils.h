/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */


#ifdef _WIN32
#define DIR_SEP "\\"
#endif
#ifdef __linux__
#define DIR_SEP "/"
#endif

#ifdef _WIN32
#if defined __cplusplus
extern "C" { /* Begin "C" */
/* Intrinsics use C name-mangling.  */
#endif /* __cplusplus */
#endif


#define UINT32_M1 ~0U

#pragma once

#ifdef EXTERN2
#undef EXTERN2
#endif
#ifdef UTILS_CPP
#define EXTERN2
#else
#define EXTERN2 extern
#endif

#ifdef _WIN32
struct timezone 
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};
EXTERN2 int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

EXTERN2 double dclock(void);
EXTERN2 char *get_root_dir_of_exe(void);
EXTERN2 char *set_root_dir_of_exe(char *pth);
EXTERN2 int ck_filename_exists(const char *filename, const char *file, int line, int verbose);
EXTERN2 uint64_t get_file_size(const char *filename, const char *file, int line, int verbose);
EXTERN2 uint32_t build_bit_ui(int beg, int end);
EXTERN2 uint64_t extract_bits(uint64_t myin, int beg, int end);
EXTERN2 uint64_t build_bit(int beg, int end);
EXTERN2 uint32_t extract_bits_ui(uint32_t myin, int beg, int end);
EXTERN2 int get_signal(void);
EXTERN2 void set_signal(void);

#ifdef _WIN32
#if defined __cplusplus
}
/* Intrinsics use C name-mangling.  */
#endif /* __cplusplus */
#endif
