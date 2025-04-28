/* Copyright (C) 2023 John Törnblom

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

#include <ipamir.h>

#include "vote.h"
#include "vote_explain.h"


struct ipamir_solver;
typedef struct ipamir_solver ipamir_solver_t;


/**
 * Data structure that captures user-defined parameters.
 **/
typedef struct mhs_ctx {
  const vote_ensemble_t *ensemble;
  const real_t          *xvec;
  const size_t          *wvec;
  size_t                 label;
  size_t                 nb_usedvars;
  vote_explain_stats_t  *stats;
} mhs_ctx_t;


/**
 * Add a hitting set to the MHS problem.
 **/
static void
mhs_hit(ipamir_solver_t* s, const bool *vec, size_t size) {
  for(size_t i=0; i<size; i++) {
    if(vec[i]) {
      ipamir_add_hard(s, i + 1);
    }
  }
  ipamir_add_hard(s, 0);
}


/**
 * Solve the MHS problem.
 **/
static bool
mhs_solve(ipamir_solver_t* s, bool *vec, size_t size) {
  if(ipamir_solve(s) != 30) { // #define OPTIMAL 30
    return false;
  }

  for(size_t i=0; i<size; i++) {
    vec[i] = ipamir_val_lit(s, i + 1) > 0;
  }
  return true;
}


/**
 * Query the oracle.
 **/
static bool
mhs_oracle_query(mhs_ctx_t *ctx, const bool *svec) {
  const vote_ensemble_t *e = ctx->ensemble;
  vote_bound_t inputs[e->nb_inputs];
  struct timespec ts1, ts2;
  bool is_valid;

  for(size_t dim=0, i=0; dim<e->nb_inputs; dim++) {
    if(!e->feature_usage[dim]) {
      inputs[dim].lower = -VOTE_INFINITY;
      inputs[dim].upper = VOTE_INFINITY;
      
    } else if(svec[i++]) {
      inputs[dim].lower = ctx->xvec[dim];
      inputs[dim].upper = ctx->xvec[dim];

    } else {
      inputs[dim].lower = -VOTE_INFINITY;
      inputs[dim].upper = VOTE_INFINITY;
    }
  }

  vote_timespec_thread_clock(&ts1);
  is_valid = vote_explain_check_label(ctx->ensemble, inputs, ctx->label, NULL);
  vote_timespec_thread_clock(&ts2);

  ctx->stats->nb_queries++;
  ctx->stats->tm_queries += vote_timespec_diff(&ts1, &ts2);

  return is_valid;
}


/**
 * Compute a minimal unsatisfiable subset that in minimum with respect to the
 * weighted sum of costs.
 **/
static void
mhs_minimum(mhs_ctx_t *ctx, bool *svec) {
  void* s = ipamir_init();
  bool set[ctx->nb_usedvars];
  bool mhs[ctx->nb_usedvars];
  bool hit[ctx->nb_usedvars];

  assert(s);

  for(size_t dim=0, i=1; dim<ctx->ensemble->nb_inputs; dim++) {
    if(!ctx->ensemble->feature_usage[dim]) {
      continue;
    }
    ipamir_add_soft_lit(s, i++, ctx->wvec[dim]);
  }

  while(1) {
    if(!mhs_solve(s, mhs, ctx->nb_usedvars)) {
      abort();
    }
    if(mhs_oracle_query(ctx, mhs)) {
      memcpy(svec, mhs, sizeof(mhs));
      break;
    }

    memset(hit, 0, sizeof(hit));
    memcpy(set, mhs, sizeof(set));
    for(size_t i=0; i<ctx->nb_usedvars; i++) {
      if(set[i]) {
	continue;
      }

      set[i] = true;
      if(mhs_oracle_query(ctx, set)) {
	set[i] = false;
	hit[i] = true;
      }
    }
    mhs_hit(s, hit, ctx->nb_usedvars);
  }

  ipamir_release(s);
}


void
vote_explain_minimum_mhs(const vote_ensemble_t *e, const real_t *xvec,
			 const size_t *wvec, bool *evec,
			 vote_explain_stats_t *stats) {
  real_t yvec[e->nb_outputs];
  bool svec[e->nb_inputs];
  mhs_ctx_t ctx = {
    .ensemble    = e,
    .xvec        = xvec,
    .wvec        = wvec,
    .label       = 0,
    .nb_usedvars = 0,
    .stats       = stats
  };

  vote_ensemble_eval(e, xvec, yvec);
  ctx.label = vote_argmax(yvec, e->nb_outputs);

  for(size_t dim=0; dim<e->nb_inputs; dim++) {
    if(e->feature_usage[dim]) {
      ctx.nb_usedvars++;
    }
  }

  stats->nb_queries = 0;
  stats->tm_queries = 0;

  memset(svec, 0, sizeof(svec));
  mhs_minimum(&ctx, svec);

  for(size_t dim=0, i=0; dim<e->nb_inputs; dim++) {
    if(e->feature_usage[dim]) {
      evec[dim] = svec[i++];
    } else {
      evec[dim] = false;
    }
  }

#ifndef NDEBUG
  assert(vote_explain_is_valid(e, xvec, evec, NULL));
#endif
}
