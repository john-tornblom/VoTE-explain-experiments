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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "vote.h"


size_t
vote_argmax(const real_t* fvec, size_t length) {
  size_t m = 0;

  // assume the output is probability in 0/1 classification
  if(length == 1) {
    return fvec[0] >= (real_t)0.5;
  }
  
  for(size_t i=1; i<length; i++) {
    if(fvec[i] > fvec[m]) {
      m = i;
    }
  }

  return m;
}


size_t
vote_argmin(const real_t* fvec, size_t length) {
  size_t m = 0;

  // assume the output is probability in 0/1 classification
  if(length == 1) {
    return fvec[0] < (real_t)0.5;
  }
  
  for(size_t i=1; i<length; i++) {
    if(fvec[i] < fvec[m]) {
      m = i;
    }
  }

  return m;
}


void
vote_normalize(real_t* vec, size_t length) {
  real_t sum = 0;

  for(size_t i=0; i<length; i++) {
    sum += vec[i];
  }

  assert(sum != 0);
  
  for(size_t i=0; i<length; i++) {
    vec[i] /= sum;
  }
}


real_t
vote_vector_min(const real_t* fvec, size_t length) {
  real_t val = VOTE_NAN;

  for(size_t i=0; i<length; i++) {
#if VOTE_FLT_SIZE == 32
    val = fminf(val, fvec[i]);
#else
    val = fmin(val, fvec[i]);
#endif
  }
  
  return val;
}


real_t
vote_vector_max(const real_t* fvec, size_t length) {
  real_t val = VOTE_NAN;

  for(size_t i=0; i<length; i++) {
#if VOTE_FLT_SIZE == 32
    val = fmaxf(val, fvec[i]);
#else
    val = fmax(val, fvec[i]);
#endif
  }
  
  return val;
}


real_t
vote_vector_avg(const real_t* fvec, size_t length) {
  real_t val = 0;

  for(size_t i=0; i<length; i++) {
    val += (fvec[i] / length);
  }

  if(!length) {
    return VOTE_NAN;
  } else {
    return val;
  }
}


int
vote_timespec_cmp(struct timespec *ts1, struct timespec *ts2) {
  if(ts1->tv_sec > ts2->tv_sec) {
    return 1;

  } else if(ts1->tv_sec < ts2->tv_sec) {
    return -1;

  } else if(ts1->tv_nsec > ts2->tv_nsec) {
    return 1;

  } else if(ts1->tv_nsec < ts2->tv_nsec) {
    return -1;
  }

  return 0;
}


void
vote_timespec_add(struct timespec *ts, double seconds) {
  ts->tv_sec += (long)seconds;
  ts->tv_nsec += (long)((seconds - (long)seconds) * 1000000000);
}


double
vote_timespec_seconds(const struct timespec *ts) {
  double sec = ts->tv_sec;
  double nsec = ts->tv_nsec;
  return sec + (nsec / 1e9);
}


double
vote_timespec_diff(struct timespec *start, struct timespec *stop) {
  double sec = 0;
  double nsec = 0;

  if((stop->tv_nsec - start->tv_nsec) < 0) {
    sec = (double)stop->tv_sec - start->tv_sec - 1;
    nsec = (double)stop->tv_nsec - start->tv_nsec + 1000000000;
  } else {
    sec = (double)stop->tv_sec - start->tv_sec;
    nsec = (double)stop->tv_nsec - start->tv_nsec;
  }

  return sec + (nsec / 1e9);
}


void
vote_timespec_thread_clock(struct timespec *ts) {
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, ts);
}


void
vote_timespec_proc_clock(struct timespec *ts) {
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, ts);
}


void
vote_timespec_realtime_clock(struct timespec *ts) {
  clock_gettime(CLOCK_REALTIME, ts);
}


const char*
vote_version(void) {
  return VERSION;
}
