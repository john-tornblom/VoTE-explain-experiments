/* Copyright (C) 2019 John Törnblom

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


#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "vote_postproc.h"
#include "vote_math.h"
#include "vote_abstract.h"


typedef struct vote_postproc {
  const vote_ensemble_t *ensemble;
  vote_mapping_cb_t     *user_cb;
  void                  *user_ctx;
} vote_postproc_t;


/**
 * Post-processing used by random forests.
 **/
static void
vote_bound_division(bool flt, vote_bound_t *values, size_t nb_values, size_t divisor) {
  for(size_t i=0; i<nb_values; i++) {
    values[i].lower = vote_div(flt, values[i].lower, divisor);
    values[i].upper = vote_div(flt, values[i].upper, divisor);
  }
}


/**
 * Post-processing algorithm used by some gradient boosting machines.
 **/
static void
vote_bound_softmax(bool flt, vote_bound_t *values, size_t nb_values) {
  vote_bound_t sum = {0, 0};
  vote_bound_t off = {0, 0};
  real_t max = -VOTE_INFINITY;

  // This is used for numerical stabillity, see
  // http://www.deeplearningbook.org/contents/numerical.html
  for(size_t i=0; i<nb_values; i++) {
    max = vote_max(flt, max, values[i].upper);
  }

  for(size_t i=0; i<nb_values; i++) {
    real_t exp_lo = vote_exp(flt, vote_sub(flt, values[i].lower, max));
    real_t exp_hi = vote_exp(flt, vote_sub(flt, values[i].upper, max));
    sum.lower = vote_add(flt, sum.lower, exp_lo);
    sum.upper = vote_add(flt, sum.upper, exp_hi);
  }

  // log(0) results in undefined behaviour
  assert(sum.lower != 0);
  assert(sum.upper != 0);

  off.lower = vote_add(flt, vote_log(flt, sum.lower), max);
  off.upper = vote_add(flt, vote_log(flt, sum.upper), max);

  //negate and swap bounds in the box that captures the offset
  real_t tmp = off.lower;
  off.lower = -off.upper;
  off.upper = -tmp;
    
  for(size_t i=0; i<nb_values; i++) {
    real_t add_lo = vote_add(flt, off.lower, values[i].lower);
    real_t add_hi = vote_add(flt, off.upper, values[i].upper);
    values[i].lower = vote_exp(flt, add_lo);
    values[i].upper = vote_exp(flt, add_hi);
  }
}


/**
 * Post-processing algorithm used by some gradient boosting machines.
 *
 * For floats, we use the algorithm from xgboost that is more robust
 * against overflows and division by zero.
 **/
static void
vote_bound_sigmoid(bool flt, vote_bound_t *values, size_t nb_values) {
  for(size_t i=0; i<nb_values; i++) {
    real_t one = 1.0;
    
    if(flt) {
      // xgboost: avoid 0 div
      one = vote_add(flt, one, 1e-16);

      // xgboost: // avoid exp overflow
      values[i].lower = vote_max(flt, values[i].lower, -88.7);
      values[i].upper = vote_max(flt, values[i].upper, -88.7);
    }

    real_t exp_lo = vote_exp(flt, -values[i].lower);
    real_t exp_hi = vote_exp(flt, -values[i].upper);
    real_t add_lo = vote_add(flt, exp_lo, one);
    real_t add_hi = vote_add(flt, exp_hi, one);
    values[i].lower = vote_div(flt, 1.0, add_lo);
    values[i].upper = vote_div(flt, 1.0, add_hi);
  }
}


void
vote_ensemble_postproc(const vote_ensemble_t *e, vote_bound_t *outputs) {
  bool flt = e->trees[0]->flt_ops;

  switch(e->post_process) {
  case VOTE_POST_PROCESS_DIVISOR:
    vote_bound_division(flt, outputs, e->nb_outputs, e->nb_trees);
    break;
    
  case VOTE_POST_PROCESS_SOFTMAX:
    vote_bound_softmax(flt, outputs, e->nb_outputs);
    break;
    
  case VOTE_POST_PROCESS_SIGMOID:
    vote_bound_sigmoid(flt, outputs, e->nb_outputs);
    break;
    
  default:
  case VOTE_POST_PROCESS_NONE:
    break;
  }
}


/**
 * Apply a post processing algorithm on a mapping.
 **/
static vote_outcome_t
vote_postproc_input(void *ctx, vote_mapping_t *m) {
  vote_postproc_t *pp = (vote_postproc_t*)ctx;

  vote_ensemble_postproc(pp->ensemble, m->outputs);

#ifndef NDEBUG
  const vote_ensemble_t *e = pp->ensemble;
  vote_bound_t outputs[m->nb_outputs];
  memset(outputs, 0, sizeof(outputs));
  vote_abstract_join_trees(e->trees, e->nb_trees,
			   m->inputs, m->nb_inputs,
			   outputs, m->nb_outputs);
  vote_ensemble_postproc(e, outputs);
  for(size_t dim=0; dim<m->nb_outputs; dim++) {
    assert(outputs[dim].lower <= m->outputs[dim].upper);
    assert(outputs[dim].upper >= m->outputs[dim].lower);
  }
#endif

  return pp->user_cb(pp->user_ctx, m);
}


vote_pipeline_t*
vote_postproc_pipeline(const vote_ensemble_t *e, void *user_ctx,
		       vote_mapping_cb_t *user_cb) {
  vote_postproc_t *pp = calloc(1, sizeof(vote_postproc_t));
  vote_pipeline_t *p = vote_pipeline_new(pp, vote_postproc_input, free);
  
  assert(pp);

  pp->ensemble = e;
  pp->user_ctx = user_ctx;
  pp->user_cb  = user_cb;

  return p;
}
