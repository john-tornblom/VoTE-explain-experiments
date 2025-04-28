/* Copyright (C) 2018 John Törnblom

   This file is part of VoTE (Verifier of Tree Ensembles).

VoTE is free software: you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

VoTE is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
for more details.

You should have received a copy of the GNU Lesser General Public
License along with VoTE; see the files COPYING and COPYING.LESSER. If not,
see <http://www.gnu.org/licenses/>.  */


#ifndef VOTE_MATH_H
#define VOTE_MATH_H

#include <math.h>

#include "vote.h"

#if VOTE_FLT_SIZE == 32
#define vote_nextafter(flt, x, y) (nextafterf((real_t)x, (real_t)y))
#define vote_exp(flt, x)          (expf((real_t)x))
#define vote_add(flt, x, y)       ((real_t)x + (real_t)y)
#define vote_sub(flt, x, y)       ((real_t)x - (real_t)y)
#define vote_le(flt, x, y)        ((real_t)x <= (real_t)y)
#define vote_gt(flt, x, y)        ((real_t)x > (real_t)y)
#define vote_div(flt, x, y)       ((real_t)x / (real_t)y)
#define vote_log(flt, x)          (logf((real_t)x))
#define vote_max(flt, x, y)       (fmaxf((real_t)x, (real_t)y))
#define vote_min(flt, x, y)       (fminf((real_t)x, (real_t)y))
#elif VOTE_FLT_SIZE == 64
#define vote_nextafter(flt, x, y) (nextafter(x, y))
#define vote_exp(flt, x)          (exp(x))
#define vote_add(flt, x, y)       (x + y)
#define vote_sub(flt, x, y)       (x - y)
#define vote_le(flt, x, y)        (x <= y)
#define vote_gt(flt, x, y)        (x > y)
#define vote_div(flt, x, y)       (x / y)
#define vote_log(flt, x)          (log(x))
#define vote_max(flt, x, y)       (fmax(x, y))
#define vote_min(flt, x, y)       (fmin(x, y))
#else
#define vote_nextafter(flt, x, y) (flt ? (double)nextafterf((float)x, (float)y) : nextafter(x, y))
#define vote_exp(flt, x)          (flt ? (double)expf((float)x) : exp(x))
#define vote_add(flt, x, y)       (flt ? (double)((float)x + (float)y) : x + y)
#define vote_sub(flt, x, y)       (flt ? (double)((float)x - (float)y) : x - y)
#define vote_le(flt, x, y)        (flt ? (float)x <= (float)y : x <= y)
#define vote_gt(flt, x, y)        (flt ? (float)x > (float)y : x > y)
#define vote_div(flt, x, y)       (flt ? (double)((float)x / (float)y) : x / y)
#define vote_log(flt, x)          (flt ? (double)logf((float)x) : log(x))
#define vote_max(flt, x, y)       (flt ? (double)fmaxf((float)x, (float)y) : fmax(x, y))
#define vote_min(flt, x, y)       (flt ? (double)fminf((float)x, (float)y) : fmin(x, y))
#endif


#endif //VOTE_MATH_H
