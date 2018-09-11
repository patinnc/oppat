
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

#if defined __cplusplus
extern "C" { /* Begin "C" */
/* Intrinsics use C name-mangling.  */
#endif /* __cplusplus */

#define ENERGY_PKG 0
#define ENERGY_COR 1
#define ENERGY_GFX 2

#define VAL_CUR 0
#define VAL_PRV 1
#define VAL_DIF 2

#define POWER_SZ ENERGY_GFX+1
#define TM_ELAP_SZ VAL_DIF+1

#define REG64_MAX_SIZE 512

#ifndef BOOL
#define BOOL int
#endif

#pragma once

int    get_power_data(int power_sz, double *power, int tm_elap_sz, double *tm_elap, double cur_time, double *pkg_temp_headroom);
double get_tcc_activation(void);
BOOL   do_read_msr( int *onum_cpus, uint32_t numMsr, uint32_t *MsrList, uint64_t pMsr[][REG64_MAX_SIZE]);
int    ck_init_msrs(void);



#if defined __cplusplus
}
/* Intrinsics use C name-mangling.  */
#endif /* __cplusplus */


