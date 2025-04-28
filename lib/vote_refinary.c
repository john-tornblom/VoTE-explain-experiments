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
#include <assert.h>
#include <string.h>

#include "vote.h"
#include "vote_pipeline.h"
#include "vote_refinary.h"
#include "vote_math.h"


typedef struct vote_refinery {
  const vote_tree_t     *tree;
  const vote_pipeline_t *pipeline;
} vote_refinery_t;


static vote_outcome_t vote_refinery_decend(const vote_refinery_t *r,
					   size_t node_id, vote_mapping_t *m);


/**
 * Decend into children of a node, starting with the left child.
 **/
static vote_outcome_t
vote_refinery_decend_left(const vote_refinery_t *r, size_t node_id,
			  vote_mapping_t *m) {
  const vote_tree_t *t = r->tree;
  int left_id = t->left[node_id];
  int right_id = t->right[node_id];
  real_t threshold = t->threshold[node_id];
  int dim = t->feature[node_id];
  real_t lower = m->inputs[dim].lower;
  real_t upper = m->inputs[dim].upper;
  vote_outcome_t o = VOTE_PASS;

  // refine left split: [lower, threshold]
  if(vote_le(t->flt_ops, lower, threshold)) {
    vote_bound_t outputs[m->nb_outputs];
    vote_mapping_t msplit = {
      .inputs = m->inputs,
      .outputs = outputs,
      .nb_inputs = m->nb_inputs,
      .nb_outputs = m->nb_outputs
    };
    memcpy(msplit.outputs, m->outputs, m->nb_outputs * sizeof(vote_bound_t));

    if(vote_gt(t->flt_ops, upper, threshold)) {
      msplit.inputs[dim].upper = threshold;
    }
    if((o=vote_refinery_decend(r, left_id, &msplit)) != VOTE_PASS) {
      m->inputs[dim].upper = upper;
      return o;
    }

    m->inputs[dim].upper = upper;
  }

  // refine right split: (threshold, upper]
  if(vote_gt(t->flt_ops, upper, threshold)) {
    if(vote_le(t->flt_ops, lower, threshold)) {
      m->inputs[dim].lower = vote_nextafter(t->flt_ops, threshold, VOTE_INFINITY);
    }
    if((o=vote_refinery_decend(r, right_id, m)) != VOTE_PASS) {
      m->inputs[dim].lower = lower;
      return o;
    }

    m->inputs[dim].lower = lower;
  }

  return o;
}


/**
 * Decend into children of a node, starting with the right child.
 **/
static vote_outcome_t
vote_refinery_decend_right(const vote_refinery_t *r, size_t node_id,
			   vote_mapping_t *m) {
  const vote_tree_t *t = r->tree;
  int left_id = t->left[node_id];
  int right_id = t->right[node_id];
  real_t threshold = t->threshold[node_id];
  int dim = t->feature[node_id];
  real_t lower = m->inputs[dim].lower;
  real_t upper = m->inputs[dim].upper;
  vote_outcome_t o = VOTE_PASS;

  // refine right split: (threshold, upper]
  if(vote_gt(t->flt_ops, upper, threshold)) {
    vote_bound_t outputs[m->nb_outputs];
    vote_mapping_t msplit = {
      .inputs = m->inputs,
      .outputs = outputs,
      .nb_inputs = m->nb_inputs,
      .nb_outputs = m->nb_outputs
    };

    memcpy(msplit.outputs, m->outputs, m->nb_outputs * sizeof(vote_bound_t));
 
    if(vote_le(t->flt_ops, lower, threshold)) {
      msplit.inputs[dim].lower = vote_nextafter(t->flt_ops, threshold, VOTE_INFINITY);
    }
    if((o=vote_refinery_decend(r, right_id, &msplit)) != VOTE_PASS) {
      m->inputs[dim].lower = lower;
      return o;
    }

    m->inputs[dim].lower = lower;
  }

  // refine left split: [lower, threshold]
  if(vote_le(t->flt_ops, lower, threshold)) {
    if(vote_gt(t->flt_ops, upper, threshold)) {
      m->inputs[dim].upper = threshold;
    }

    if((o=vote_refinery_decend(r, left_id, m)) != VOTE_PASS) {
      m->inputs[dim].upper = upper;
      return o;
    }

    m->inputs[dim].upper = upper;
  }

  return o;
}


/**
 * Decend into children of a node, starting with the child with the least input
 * space.
 **/
static vote_outcome_t
vote_refinery_decend(const vote_refinery_t *r, size_t node_id,
		     vote_mapping_t *m) {
  const vote_tree_t *t = r->tree;
  int left_id = t->left[node_id];
  int right_id = t->right[node_id];
  real_t value[m->nb_outputs];

  // leaf node encountered, emit mapping
  if(left_id < 0 || right_id < 0) {
    assert(left_id < 0 && right_id < 0);

    memcpy(value, t->value[node_id], m->nb_outputs * sizeof(real_t));
    if(t->normalize) {
      vote_normalize(value, m->nb_outputs);
    }

    for(size_t i=0; i<m->nb_outputs; i++) {
      m->outputs[i].lower = vote_add(t->flt_ops, m->outputs[i].lower, value[i]);
      m->outputs[i].upper = vote_add(t->flt_ops, m->outputs[i].upper, value[i]);
    }

    return vote_pipeline_output(r->pipeline, m);
  }

  //
  //        left       right
  //   |-----------|-----------|
  // lower     threshold     upper

  real_t threshold = t->threshold[node_id];
  int dim = t->feature[node_id];

  real_t right_width = m->inputs[dim].upper - threshold;
  real_t left_width = threshold - m->inputs[dim].lower;

  if(left_width < right_width) {
    return vote_refinery_decend_left(r, node_id, m);
  } else {
    return vote_refinery_decend_right(r, node_id, m);
  }
}


/**
 * Apply the refining algorithm on a mapping.
 **/
static vote_outcome_t
vote_refinery_input(void *ctx, vote_mapping_t *m) {
  vote_refinery_t *r = (vote_refinery_t*)ctx;

  return vote_refinery_decend(r, 0, m);
}


vote_pipeline_t*
vote_refinary_pipeline(const vote_tree_t *t) {
  vote_refinery_t *r = calloc(1, sizeof(vote_refinery_t));
  vote_pipeline_t *p = vote_pipeline_new(r, vote_refinery_input, free);

  assert(r);

  r->tree     = t;
  r->pipeline = p;

  return p;
}
