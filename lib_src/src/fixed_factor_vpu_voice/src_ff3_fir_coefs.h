// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XCORE VocalFusion Licence.

/*********************************/
/* AUTOGENERATED. DO NOT MODIFY! */
/*********************************/

// Use src_ff3_fir_gen.py script to regenare this file
// python src_ff3_fir_gen.py -gc True -ntp 32 -np 3

#ifndef _SRC_FF3_COEFS_H_
#define _SRC_FF3_COEFS_H_

#include <stdint.h>

#ifndef ALIGNMENT
#  ifdef __xcore__
#    define ALIGNMENT(N)  __attribute__((aligned (N)))
#  else
#    define ALIGNMENT(N)
#  endif
#endif

#define SRC_FF3_FIR_NUM_PHASES (3)
#define SRC_FF3_FIR_TAPS_PER_PHASE (32)

/** q31 coefficients to use for debugging ff3 sample rate conversion */
extern const int32_t src_ff3_fir_coefs_debug[SRC_FF3_FIR_NUM_PHASES * SRC_FF3_FIR_TAPS_PER_PHASE];

/** q30 coefficients to use for the ff3 48 - 16 kHz polyphase FIR filtering */
extern const int32_t src_ff3_fir_coefs[SRC_FF3_FIR_NUM_PHASES][SRC_FF3_FIR_TAPS_PER_PHASE];

#endif // _SRC_FF3_COEFS_H_

