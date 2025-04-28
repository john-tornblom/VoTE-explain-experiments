/* Copyright (C) 2022 John Törnblom

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

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "vote.h"
#include "vote_explain.h"


#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)


/**
 * 
 **/
static void
bb_find_minimum(const vote_ensemble_t *e, const real_t *xvec, size_t label,
		size_t *path, int depth, vote_bound_t *inputs,
		const real_t *wvec, real_t *min_cost, bool *evec,
		vote_explain_stats_t *stats) {
  struct timespec ts1, ts2;
  bool is_valid;

  // check if candidate is minimum
  if(likely(depth >= 0)) {
    size_t dim = path[depth];
    inputs[dim].lower = xvec[dim];
    inputs[dim].upper = xvec[dim];

    vote_timespec_thread_clock(&ts1);
    is_valid = vote_explain_check_label(e, inputs, label, NULL);
    vote_timespec_thread_clock(&ts2);

    stats->nb_queries++;
    stats->tm_queries += vote_timespec_diff(&ts1, &ts2);

    if(is_valid) {
      memset(evec, 0, sizeof(bool) * e->nb_inputs);
      *min_cost = 0;

      for(int i=0; i<=depth; i++) {
	size_t j = path[i];
	*min_cost = *min_cost + wvec[j];
	evec[j] = true;
      }

      inputs[dim].lower = -VOTE_INFINITY;
      inputs[dim].upper = VOTE_INFINITY;
      return;
    }
  }

  for(size_t dim=0; dim<e->nb_inputs; dim++) {
    bool dim_in_path = false;
    real_t cur_cost = 0;

    if(!e->feature_usage[dim]) {
      continue;
    }

    for(int j=0; j<=depth; j++) {
      if(path[j] == dim) {
	dim_in_path = true;
	break;
      }
      cur_cost += wvec[path[j]];
    }

    if(dim_in_path || *min_cost <= cur_cost + wvec[dim]) {
      continue;
    }
 
    path[depth+1] = dim;
    bb_find_minimum(e, xvec, label, path, depth+1, inputs, wvec,
		    min_cost, evec, stats);
  }

  if(likely(depth >= 0)) {
    size_t dim = path[depth];
    inputs[dim].lower = -VOTE_INFINITY;
    inputs[dim].upper = VOTE_INFINITY;
  }
}


void
vote_explain_minimum_bb(const vote_ensemble_t *e, const real_t *xvec,
			const real_t *wvec, bool *evec,
			vote_explain_stats_t *stats) {
  vote_bound_t inputs[e->nb_inputs];
  real_t yvec[e->nb_outputs];
  size_t path[e->nb_inputs];
  real_t min_cost = 1;
  size_t label;

  // compute output values
  memset(yvec, 0, sizeof(yvec));
  vote_ensemble_eval(e, xvec, yvec);
  label = vote_argmax(yvec, e->nb_outputs);
  
  // relax all variables
  for(size_t dim=0; dim<e->nb_inputs; dim++) {
    inputs[dim].lower = -VOTE_INFINITY;
    inputs[dim].upper = VOTE_INFINITY;
    min_cost += wvec[dim];
  }

  stats->nb_queries = 0;
  stats->tm_queries = 0;

  // permutate ways to add a variable
  bb_find_minimum(e, xvec, label, path, -1, inputs, wvec,
		  &min_cost, evec, stats);

#ifndef NDEBUG
  assert(vote_explain_is_valid(e, xvec, evec, NULL));
#endif
}
