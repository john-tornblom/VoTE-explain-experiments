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

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <vote.h>

#include "vote_explain.h"
#include "vote_math.h"


/**
 * 
 **/
typedef struct check_argmax_args {
  const vote_ensemble_t *ensemble;
  size_t                 label;
  vote_bound_t          *coex;
} check_argmax_args_t;


/**
 * 
 **/
typedef struct check_onevsall_args {
  const vote_mapping_t *m;
  vote_bound_t         *coex;
} check_onevsall_args_t;


/**
 * Callback function used by explain_check_argmax_onevsall() to check if:
 *     m1.output > m2.output.
 **/
static vote_outcome_t
explain_check_mapping_gt(void *ctx, vote_mapping_t *m) {
  check_onevsall_args_t *args = (check_onevsall_args_t*)ctx;

  if(args->m->outputs[0].lower > m->outputs[0].upper) {
    return VOTE_PASS;
  }

  if(m->outputs[0].lower > args->m->outputs[0].upper) {
    if(args->coex) {
      for(size_t dim=0; dim<m->nb_inputs; dim++) {
	args->coex[dim].lower = m->inputs[dim].lower;
	args->coex[dim].upper = m->inputs[dim].upper;
      }
    }
    return VOTE_FAIL;
  }

  return VOTE_UNSURE;
}


/**
 * Callback function used by explain_check_label() in the case of one vs. all
 * multiclass classification.
 **/
static vote_outcome_t
explain_check_argmax_onevsall(void *ctx, vote_mapping_t *m) {
  check_argmax_args_t *args = (check_argmax_args_t*)ctx;
  check_onevsall_args_t other_args = {m, args->coex};
  const vote_ensemble_t *e = args->ensemble;
  vote_outcome_t o = VOTE_PASS;
  size_t label = args->label;

  for(size_t i=0; i<e->nb_outputs; i++) {
    if(i == label) {
      continue;
    }
    o = vote_ensemble_absref(e->onevsall[i], m->inputs,
			     explain_check_mapping_gt, &other_args);
    if(VOTE_PASS != o) {
      break;
    }
  }

  return o;
}


/**
 * Callback function used by explain_check_label().
 **/
static vote_outcome_t
explain_check_argmax(void *ctx, vote_mapping_t *m) {
  check_argmax_args_t *args = (check_argmax_args_t*)ctx;
  vote_outcome_t o = vote_mapping_check_argmax(m, args->label);

  if(args->coex && o == VOTE_FAIL) {
    for(size_t dim=0; dim<m->nb_inputs; dim++) {
      args->coex[dim].lower = m->inputs[dim].lower;
      args->coex[dim].upper = m->inputs[dim].upper;
    }
  }

  return o;
}


bool
vote_explain_check_label(const vote_ensemble_t *e, const vote_bound_t *inputs,
			 size_t label, vote_bound_t *coex) {
  check_argmax_args_t args = {e, label, coex};
  vote_outcome_t o;

  if(e->onevsall) {
    o = vote_ensemble_absref(e->onevsall[label], inputs,
			     explain_check_argmax_onevsall, &args);
  } else {
    o = vote_ensemble_absref(e, inputs, explain_check_argmax, &args);
  }

  assert(o != VOTE_UNSURE);

  return o == VOTE_PASS;
}


bool
vote_explain_is_valid(const vote_ensemble_t *e, const real_t *xvec,
		      const bool *evec, real_t *counter_example) {
  vote_bound_t inputs[e->nb_inputs];
  vote_bound_t coex[e->nb_inputs];
  real_t yvec[e->nb_outputs];
  size_t label;

  memset(coex, 0, sizeof(coex));
  memset(yvec, 0, sizeof(yvec));
  vote_ensemble_eval(e, xvec, yvec);
  label = vote_argmax(yvec, e->nb_outputs);

  for(size_t dim=0; dim<e->nb_inputs; dim++) {
    if(evec[dim]) {
      inputs[dim].lower = xvec[dim];
      inputs[dim].upper = xvec[dim];
    } else {
      inputs[dim].lower = -VOTE_INFINITY;
      inputs[dim].upper = VOTE_INFINITY;
    }
  }

  if(vote_explain_check_label(e, inputs, label, coex)) {
    return true;
  }

  if(counter_example) {
    for(size_t dim=0; dim<e->nb_inputs; dim++) {
      if(isinf(coex[dim].upper)) {
	counter_example[dim] = coex[dim].upper;

      } else if(isinf(coex[dim].lower)) {
	counter_example[dim] = coex[dim].lower;

      } else {
	real_t delta = (coex[dim].upper - coex[dim].lower) / 2;
	counter_example[dim] = coex[dim].lower + delta;
      }
    }
  }

  return false;
}


void
vote_explain_minimal(const vote_ensemble_t *e, const real_t *xvec, bool *evec) {
  vote_bound_t inputs[e->nb_inputs];
  real_t yvec[e->nb_outputs];
  size_t label;

  // compute predicted label
  memset(yvec, 0, sizeof(yvec));
  vote_ensemble_eval(e, xvec, yvec);
  label = vote_argmax(yvec, e->nb_outputs);

  // init explanation to the most costly one
  for(size_t dim=0; dim<e->nb_inputs; dim++) {
    inputs[dim].lower = xvec[dim];
    inputs[dim].upper = xvec[dim];
    evec[dim] = true;
  }

  // relax one variable at a time
  for(size_t dim=0; dim<e->nb_inputs; dim++) {
    inputs[dim].lower = -VOTE_INFINITY;
    inputs[dim].upper = VOTE_INFINITY;
    evec[dim] = false;
    
    if(e->feature_usage[dim] &&
       !vote_explain_check_label(e, inputs, label, NULL)) {
      inputs[dim].lower = xvec[dim];
      inputs[dim].upper = xvec[dim];
      evec[dim] = true;
    }
  }
}


void
vote_explain_minimum(const vote_ensemble_t *e, const real_t *xvec,
		     const real_t *wvec, bool *evec) {
  vote_explain_stats_t stats;
  vote_explain_minimum_marco(e, xvec, wvec, evec, &stats);
}


void
vote_explain_forall(const vote_ensemble_t *e, const real_t *xvec,
		    vote_explain_cb_t *cb, void* ctx) {
  vote_explain_stats_t stats;
  vote_explain_forall_marco(e, xvec, &stats, cb, ctx);
}

