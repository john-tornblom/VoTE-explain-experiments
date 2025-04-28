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
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "vote.h"
#include "vote_abstract.h"
#include "vote_math.h"
#include "vote_postproc.h"
#include "vote_tree.h"


/**
 *
 **/
typedef struct vote_absref_query {
  const vote_ensemble_t *ensemble;
  vote_tree_t          **trees;
  vote_mapping_cb_t     *prop_cb;
  void                  *prop_ctx;
  vote_bound_t          *coex;
} vote_absref_query_t;


/**
 *
 **/
static vote_outcome_t vote_absref_decend(vote_absref_query_t *q, size_t tree_id,
					 size_t node_id, vote_mapping_t *m);


/**
 *
 **/
static vote_outcome_t
vote_absref_check_property(vote_absref_query_t *q, vote_mapping_t *m) {
  vote_outcome_t o = q->prop_cb(q->prop_ctx, m);
  if(o == VOTE_FAIL && q->coex) {
    memcpy(q->coex, m->inputs, sizeof(vote_bound_t) * m->nb_inputs);
  }
  return o;
}

//#define ABSREF_DUMP_STATE
//#include <stdio.h>


/**
 *
 **/
static vote_outcome_t
vote_absref_abstract(vote_absref_query_t *q, size_t tree_id, vote_mapping_t *m) {
  vote_bound_t join_outputs[m->nb_outputs];
  vote_bound_t tree_outputs[m->nb_outputs];
  const vote_ensemble_t *e = q->ensemble;
  vote_outcome_t outcome;
  vote_mapping_t join = {
    .inputs = m->inputs,
    .outputs = join_outputs,
    .nb_inputs = m->nb_inputs,
    .nb_outputs = m->nb_outputs
  };

  if(tree_id == e->nb_trees) {
    vote_ensemble_postproc(e, m->outputs);
    return vote_absref_check_property(q, m);
  }

#ifdef ABSREF_DUMP_STATE
  printf("%ld | ", tree_id);

  for(size_t i=0; i<m->nb_inputs; i++) {
    printf("[%.2f,%.2f] ", m->inputs[i].lower, m->inputs[i].upper);
  }
  printf("| ");
#endif

  memcpy(join_outputs, m->outputs, sizeof(join_outputs));
  for(size_t i=tree_id; i<e->nb_trees; i++) {
    vote_abstract_join_tree(q->trees[i], join.inputs, join.nb_inputs,
			    tree_outputs, join.nb_outputs);

    real_t tree_score = 0;
    for(size_t dim=0; dim<e->nb_outputs; dim++) {
      join_outputs[dim].lower = vote_add(q->trees[i]->flt_ops,
					 join_outputs[dim].lower,
					 tree_outputs[dim].lower);
      join_outputs[dim].upper = vote_add(q->trees[i]->flt_ops,
					 join_outputs[dim].upper,
					 tree_outputs[dim].upper);

#ifdef ABSREF_DUMP_STATE
      printf("[%.2f,%.2f] ", tree_outputs[dim].lower, tree_outputs[dim].upper);
#endif
    }

#ifdef ABSREF_DUMP_STATE
  printf("| ");
#endif
  }

#ifdef ABSREF_DUMP_STATE
  for(size_t i=0; i<m->nb_outputs; i++) {
    printf("[%.2f,%.2f] ", join_outputs[i].lower, join_outputs[i].upper);
  }
  printf("\n");
#endif

  vote_ensemble_postproc(e, join_outputs);
  if((outcome=vote_absref_check_property(q, &join)) == VOTE_UNSURE) {
    return vote_absref_decend(q, tree_id, 0, m);
  }

  return outcome;
}


/**
 *
 **/
static vote_outcome_t
vote_absref_ldecend(vote_absref_query_t *q, size_t tree_id, size_t node_id,
		    vote_mapping_t *m) {
  const vote_tree_t *t = q->trees[tree_id];
  int left_id = t->left[node_id];
  int right_id = t->right[node_id];
  real_t threshold = t->threshold[node_id];
  int dim = t->feature[node_id];
  real_t lower = m->inputs[dim].lower;
  real_t upper = m->inputs[dim].upper;
  vote_outcome_t outcome = VOTE_PASS;

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

    outcome = vote_absref_decend(q, tree_id, left_id, &msplit);
    m->inputs[dim].upper = upper;

    if(outcome != VOTE_PASS) {
      return outcome;
    }
  }

  // refine right split: (threshold, upper]
  if(vote_gt(t->flt_ops, upper, threshold)) {
    if(vote_le(t->flt_ops, lower, threshold)) {
      m->inputs[dim].lower = vote_nextafter(t->flt_ops, threshold, VOTE_INFINITY);
    }
    outcome = vote_absref_decend(q, tree_id, right_id, m);
    m->inputs[dim].lower = lower;
  }

  return outcome;
}


/**
 * Decend into children of a node, starting with the right child.
 **/
static vote_outcome_t
vote_absref_rdecend(vote_absref_query_t *q, size_t tree_id, size_t node_id,
		    vote_mapping_t *m) {
  const vote_tree_t *t = q->trees[tree_id];
  int left_id = t->left[node_id];
  int right_id = t->right[node_id];
  real_t threshold = t->threshold[node_id];
  int dim = t->feature[node_id];
  real_t lower = m->inputs[dim].lower;
  real_t upper = m->inputs[dim].upper;
  vote_outcome_t outcome = VOTE_PASS;

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

    outcome = vote_absref_decend(q, tree_id, right_id, &msplit);
    m->inputs[dim].lower = lower;

    if(outcome != VOTE_PASS) {
      return outcome;
    }
  }

  // refine left split: [lower, threshold]
  if(vote_le(t->flt_ops, lower, threshold)) {
    if(vote_gt(t->flt_ops, upper, threshold)) {
      m->inputs[dim].upper = threshold;
    }

    outcome = vote_absref_decend(q, tree_id, left_id, m);
    m->inputs[dim].upper = upper;
  }

  return outcome;
}


/**
 * Decend into children of a node, starting with the child with the least input
 * space.
 **/
static vote_outcome_t
vote_absref_decend(vote_absref_query_t *q, size_t tree_id, size_t node_id,
		   vote_mapping_t *m) {
  vote_tree_t *t = q->trees[tree_id];
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

    return vote_absref_abstract(q, tree_id + 1, m);
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
    return vote_absref_ldecend(q, tree_id, node_id, m);
  } else {
    return vote_absref_rdecend(q, tree_id, node_id, m);
  }
}


vote_outcome_t
vote_absref_query(const vote_ensemble_t *e, const vote_bound_t *input_region,
		  vote_mapping_cb_t *prop_cb, void *prop_ctx,
		  vote_bound_t *coex) {
  vote_bound_t outputs[e->nb_outputs];
  vote_bound_t inputs[e->nb_inputs];
  vote_tree_t* trees[e->nb_trees];
  vote_mapping_t m = {
    .inputs = inputs,
    .outputs = outputs,
    .nb_inputs = e->nb_inputs,
    .nb_outputs = e->nb_outputs
  };
  vote_absref_query_t q = {
    .ensemble = e,
    .trees = trees,
    .prop_cb = prop_cb,
    .prop_ctx = prop_ctx,
    .coex = coex
  };

  memcpy(trees, e->trees, sizeof(trees));
  memcpy(inputs, input_region, sizeof(inputs));
  memset(outputs, 0, sizeof(outputs));

  return vote_absref_abstract(&q, 0, &m);
}

