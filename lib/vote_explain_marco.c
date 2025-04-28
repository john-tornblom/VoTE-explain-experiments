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

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include <ipasir.h>

#include "vote.h"
#include "vote_explain.h"


struct ipasir_solver;
typedef struct ipasir_solver ipasir_solver_t;


/**
 * Data structure used for ordering weights while keeping track of indices.
 **/
typedef struct vote_marco_weight {
  real_t value;
  size_t index;
} vote_marco_weight_t;


/**
 * Data structure that captures user-defined parameters.
 **/
typedef struct marco_ctx {
  const vote_ensemble_t *ensemble;
  const real_t          *xvec;
  vote_marco_weight_t   *weight;
  bool                  *mcs_singleton;
  size_t                 label;
  size_t                 nb_usedvars;
  vote_explain_stats_t  *stats;
} marco_ctx_t;


/**
 * qsort() callback function for ordering weights.
 **/
static int
weight_cmp(const void *a, const void *b) {
  vote_marco_weight_t *w1 = (vote_marco_weight_t*)a;
  vote_marco_weight_t *w2 = (vote_marco_weight_t*)b;

  if((*w1).value < (*w2).value) {
    return 1;
  }

  if((*w1).value > (*w2).value) {
    return -1;
  }

  return 0;
}


/**
 * Query the oracle.
 **/
static bool
marco_query(marco_ctx_t *ctx, const bool *svec) {
  const vote_ensemble_t *e = ctx->ensemble;
  vote_bound_t inputs[e->nb_inputs];
  struct timespec ts1, ts2;
  size_t neg_cnt = 0;
  size_t neg_ind = 0;
  bool is_valid;

  for(size_t dim=0, i=0; dim<e->nb_inputs; dim++) {
    if(!e->feature_usage[dim]) {
      inputs[dim].lower = -VOTE_INFINITY;
      inputs[dim].upper = VOTE_INFINITY;
      continue;
    }

    if(svec[i]) {
      inputs[dim].lower = -VOTE_INFINITY;
      inputs[dim].upper = VOTE_INFINITY;
    } else {
      inputs[dim].lower = ctx->xvec[dim];
      inputs[dim].upper = ctx->xvec[dim];
      neg_cnt++;
      neg_ind = i;
    }
    i++;
  }

  vote_timespec_thread_clock(&ts1);
  is_valid = vote_explain_check_label(ctx->ensemble, inputs, ctx->label, NULL);
  vote_timespec_thread_clock(&ts2);

  ctx->stats->nb_queries++;
  ctx->stats->tm_queries += vote_timespec_diff(&ts1, &ts2);

  // found a MCS singleton, use that to speed up MSS enumeration
  if(is_valid && neg_cnt == 1) {
    ctx->mcs_singleton[neg_ind] = true;
  }

  return is_valid;
}


/**
 * Explore the power set lattice of the constraint system, and extract
 * a set of constraints.
 **/
static bool
marco_explore(ipasir_solver_t *s, bool *svec, int nvars) {
  if(ipasir_solve(s) != 10) { // #define SAT 10
    return false;
  }

  for(int i=0; i<nvars; i++) {
    svec[i] = ipasir_val(s, i + 1) > 0;
  }

  return true;
}


/**
 * Block further lattice explorations upwards with respect to a set of 
 * constraints.
 **/
static void
marco_block_up(ipasir_solver_t *s, const bool *svec, int nvars) {
  for(int i=0; i<nvars; i++) {
    if(svec[i]) {
      ipasir_add(s, -(i + 1));
    }
  }
  ipasir_add(s, 0);
}


/**
 * Block further lattice explorations downwards with respect to a set of 
 * constraints.
 **/
static void
marco_block_down(ipasir_solver_t *s, const bool *svec, int nvars) {
  for(int i=0; i<nvars; i++) {
    if(!svec[i]) {
      ipasir_add(s, i + 1);
    }
  }
  ipasir_add(s, 0);
}


/**
 *  Compute the cost of a subset.
 **/
static real_t
marco_cost(const marco_ctx_t *ctx, bool *svec) {
  real_t cost = 0;

  for(size_t i=0; i<ctx->nb_usedvars; i++) {
    size_t ind = ctx->weight[i].index;
    real_t val = ctx->weight[i].value;

    if(svec[ind]) {
      cost += val;
    }
  }

  return cost;
}


/**
 * Add a cardinality constraint to the seed generator so that generated
 * seed exceedes a given cost.
 **/
static void
marco_atleast(ipasir_solver_t *s, const vote_marco_weight_t *w, size_t nvars,
	      real_t cost) {
  real_t c = 0;
  int k = nvars;

  // The weights are sorted in decending order, so we can compute a lower bound
  // on the number of variables that should be assigned the value false by the
  // seed generator in linear time. We then encode this as an 'atmost constraint'
  // in the SAT solver.
  for(size_t i=0; i<nvars; i++) {
    c += w[i].value;
    k--;
    if(c > cost) {
      break;
    }
  }

  for(size_t i=0; i<nvars; i++) {
    ipasir_add(s, -(i + 1));
  }

  ipasir_atmost(s, k);
}


/**
 * Comute a minimal unsatisfiable subset (MUS).
 **/
static void
marco_shrink(marco_ctx_t *ctx, bool *svec) {
  for(size_t i=0; i<ctx->nb_usedvars; i++) {
    if(!svec[i]) {
      continue;
    }

    // MARCO+ optimisation
    if(ctx->mcs_singleton[i]) {
      continue;
    }

    svec[i] = false;
    if(marco_query(ctx, svec)) {
      svec[i] = true;
    }
  }
}


/**
 * Compute a maximal satisfiable subset (MSS).
 **/
static void
marco_grow(marco_ctx_t *ctx, bool *svec) {
  for(size_t i=0; i<ctx->nb_usedvars; i++) {
    if(svec[i]) {
      continue;
    }

    svec[i] = true;
    if(!marco_query(ctx, svec)) {
      svec[i] = false;
    }
  }
}



/**
 * Compute a maximal satisfiable subset that is maximum with respect to the
 * weighted sum of costs.
 **/
static void 
marco_mss_maximum(marco_ctx_t *ctx, bool *svec) {
  ipasir_solver_t *s = ipasir_init();
  bool seed[ctx->nb_usedvars];
  real_t max_cost = -1;
  real_t cost = 0;

  while(marco_explore(s, seed, ctx->nb_usedvars)) {
    if((cost=marco_cost(ctx, seed)) <= max_cost) {
      // Not reachable when weights are equal for all variables
      marco_block_down(s, seed, ctx->nb_usedvars);

    } else if(marco_query(ctx, seed)) {
      marco_grow(ctx, seed);
      marco_block_down(s, seed, ctx->nb_usedvars);
      cost = marco_cost(ctx, seed);
      marco_atleast(s, ctx->weight, ctx->nb_usedvars, cost);
      memcpy(svec, seed, sizeof(seed));
      max_cost = cost;

    } else {
      marco_shrink(ctx, seed);
      marco_block_up(s, seed, ctx->nb_usedvars);
    }
  }

  assert(max_cost >= 0);

  ipasir_release(s);
}


/**
 *
 **/
static void
marco_mss_forall(marco_ctx_t *ctx, vote_explain_cb_t *cb, void* cb_ctx) {
  const vote_ensemble_t *e = ctx->ensemble;
  ipasir_solver_t *s = ipasir_init();
  bool seed[ctx->nb_usedvars];
  bool evec[e->nb_inputs];

  while(marco_explore(s, seed, ctx->nb_usedvars)) {
    if(marco_query(ctx, seed)) {
      marco_grow(ctx, seed);
      marco_block_down(s, seed, ctx->nb_usedvars);

      for(size_t dim=0, i=0; dim<e->nb_inputs; dim++) {
	if(!e->feature_usage[dim]) {
	  evec[dim] = false;
	  continue;
	}
	evec[dim] = !seed[i++];
      }

#ifndef NDEBUG
      assert(vote_explain_is_valid(e, ctx->xvec, evec, NULL));
#endif

      if(!cb(cb_ctx, evec, e->nb_inputs)) {
	break;
      }
    } else {
      marco_shrink(ctx, seed);
      marco_block_up(s, seed, ctx->nb_usedvars);
    }
  }

  ipasir_release(s);
}


void
vote_explain_minimum_marco(const vote_ensemble_t *e, const real_t *xvec,
			   const real_t *wvec, bool *evec,
			   vote_explain_stats_t *stats) {
  vote_marco_weight_t weight[e->nb_inputs];
  bool mcs_singleton[e->nb_inputs];
  real_t yvec[e->nb_outputs];
  bool svec[e->nb_inputs];
  marco_ctx_t ctx = {
    .ensemble      = e,
    .xvec          = xvec,
    .weight        = weight,
    .mcs_singleton = mcs_singleton,
    .label         = 0,
    .nb_usedvars   = 0,
    .stats         = stats
  };

  vote_ensemble_eval(e, xvec, yvec);
  ctx.label = vote_argmax(yvec, e->nb_outputs);

  stats->nb_queries = 0;
  stats->tm_queries = 0;
  memset(mcs_singleton, 0, sizeof(mcs_singleton));

  // Compute number of referenced variables, and sort weights in ascending order
  for(size_t dim=0; dim<e->nb_inputs; dim++) {
    if(e->feature_usage[dim]) {
      size_t i = ctx.nb_usedvars;
      weight[i].value = wvec[dim];
      weight[i].index = i;
      ctx.nb_usedvars++;
    }
  }
  qsort(weight, ctx.nb_usedvars, sizeof(weight[0]), weight_cmp);

  marco_mss_maximum(&ctx, svec);

  for(size_t dim=0, i=0; dim<e->nb_inputs; dim++) {
    if(e->feature_usage[dim]) {
      evec[dim] = !svec[i++];
    } else {
      evec[dim] = false;
    }
  }

#ifndef NDEBUG
  assert(vote_explain_is_valid(e, xvec, evec, NULL));
#endif
}


void
vote_explain_forall_marco(const vote_ensemble_t *e, const real_t *xvec,
			  vote_explain_stats_t *stats,
			  vote_explain_cb_t *cb, void* cb_ctx) {
  bool mcs_singleton[e->nb_inputs];
  real_t yvec[e->nb_outputs];
  bool svec[e->nb_inputs];
  marco_ctx_t ctx = {
    .ensemble      = e,
    .xvec          = xvec,
    .weight        = NULL,
    .mcs_singleton = mcs_singleton,
    .label         = 0,
    .nb_usedvars   = 0,
    .stats         = stats
  };

  vote_ensemble_eval(e, xvec, yvec);
  ctx.label = vote_argmax(yvec, e->nb_outputs);

  stats->nb_queries = 0;
  stats->tm_queries = 0;
  memset(mcs_singleton, 0, sizeof(mcs_singleton));
  
  // Compute number of referenced variables
  for(size_t dim=0; dim<e->nb_inputs; dim++) {
    if(e->feature_usage[dim]) {
      ctx.nb_usedvars++;
    }
  }

  marco_mss_forall(&ctx, cb, cb_ctx);
}
