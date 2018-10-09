///////////////////////////////////////////////////////////////////////////////
// \author (c) Marco Paland (info@paland.com)
//             2014-2018, PALANDesign Hannover, Germany
//
// \license The MIT License (MIT)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// \brief Tiny printf, sprintf and snprintf implementation, optimized for speed on
//        embedded systems with a very limited resources.
//        Use this instead of bloated standard/newlib printf.
//        These routines are thread safe and reentrant!
//
///////////////////////////////////////////////////////////////////////////////

#ifndef _PRINTF_H_
#define _PRINTF_H_

#include <stdarg.h>
#include <stdint.h>


// internal flag definitions
#define PF_FLAGS_ZEROPAD   (1U << 0U)
#define PF_FLAGS_LEFT      (1U << 1U)
#define PF_FLAGS_PLUS      (1U << 2U)
#define PF_FLAGS_SPACE     (1U << 3U)
#define PF_FLAGS_HASH      (1U << 4U)
#define PF_FLAGS_UPPERCASE (1U << 5U)
#define PF_FLAGS_LONG      (1U << 6U)
#define PF_FLAGS_LONG_LONG (1U << 7U)
#define PF_FLAGS_PRECISION (1U << 8U)
#define PF_FLAGS_WIDTH     (1U << 9U)
#define PF_FLAGS_TYP_STR   (1U << 10U)
#define PF_FLAGS_TYP_NUM   (1U << 11U)
#define PF_FLAGS_TYP_PTR   (1U << 12U)
#define PF_FLAGS_TYP_CHR   (1U << 13U)
#define PF_FLAGS_TYP_FLT   (1U << 14U)
#define PF_FLAGS_TYP_NUL   (1U << 15U)
// below must be last
#define PF_FLAGS_LAST      (1U << 16U)

// has to be in same order && same number of members as above
static const char *pf_flag_strs[] = {
	"ZEROPAD",
	"LEFT",
	"PLUS",
	"SPACE",
	"HASH",
	"UPPERCASE",
	"LONG",
	"LONG_LONG",
	"PRECISION",
	"WIDTH",
	"TYP_STR",
	"TYP_NUM",
	"TYP_PTR",
	"TYP_CHR",
	"TYP_FLT",
	"TYP_NUL",
	"End_of_flags",
};
  

#ifdef __cplusplus
extern "C" {
#endif

#if 0
struct lst_ft_per_fld_str {
	uint32_t flags;
	std::string prefix, fld_nm, typ;
};
#endif


/**
 * Output a character to a custom device like UART, used by the printf() function
 * This function is declared here only. You have to write your custom implementation somewhere
 * \param character Character to output
 */
void _putchar(char character);


/**
 * Tiny printf implementation
 * You have to implement _putchar if you use printf()
 * \param format A string that specifies the format of the output
 * \return The number of characters that are written into the array, not counting the terminating null character
 */
int my_printf(const char* format, ...);


/**
 * Tiny sprintf implementation
 * Due to security reasons (buffer overflow) YOU SHOULD CONSIDER USING SNPRINTF INSTEAD!
 * \param buffer A pointer to the buffer where to store the formatted string. MUST be big enough to store the output!
 * \param format A string that specifies the format of the output
 * \return The number of characters that are WRITTEN into the buffer, not counting the terminating null character
 */
int my_sprintf(char* buffer, const char* format, ...);


/**
 * Tiny snprintf/vsnprintf implementation
 * \param buffer A pointer to the buffer where to store the formatted string
 * \param count The maximum number of characters to store in the buffer, including a terminating null character
 * \param format A string that specifies the format of the output
 * \return The number of characters that are WRITTEN into the buffer, not counting the terminating null character
 *         If the formatted string is truncated the buffer size (count) is returned
 */
int  my_snprintf(char* buffer, size_t count, const char* format, ...);
int my_vsnprintf(char* buffer, size_t count, const char* format, va_list va);
int my_vsnprintf_dbg(char* buffer, size_t count, const char* format, std::vector <lst_ft_per_fld_str> &per_fld);
int flags_decode(uint32_t flags, std::string &decoded_flags);


#ifdef __cplusplus
}
#endif


#endif  // _PRINTF_H_
