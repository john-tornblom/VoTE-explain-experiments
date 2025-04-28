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


#ifndef VOTE_H
#define VOTE_H

#include <stddef.h>
#include <stdbool.h>
#include <math.h>


#define VOTE_UNUSED(x) (void)(x)

#define USE_DOUBLE 1
typedef double real_t;

#if USE_DOUBLE
#define VOTE_INFINITY (real_t)INFINITY
#define VOTE_NAN      (real_t)NAN
#else
#define VOTE_INFINITY INFINITY
#define VOTE_NAN      NAN
#endif


/**
 * The bound of a variable, i.e. its range.
 **/
typedef struct vote_bound {
  real_t lower;
  real_t upper;
} vote_bound_t;


/**
 * A mapping from an input region to an output range.
 **/
typedef struct vote_mapping {
  vote_bound_t* inputs;
  vote_bound_t* outputs;
  size_t        nb_inputs;
  size_t        nb_outputs;
} vote_mapping_t;


/**
 * Forward declaration of a tree, only used internally by VoTE.
 **/
struct vote_tree;
typedef struct vote_tree vote_tree_t;


/**
 * Post process an ensemble with an algorithm that differentiates e.g.
 * the random forest models from gradient boosting models during prediction.
 **/
typedef enum vote_post_process {
  VOTE_POST_PROCESS_NONE     = 0,
  VOTE_POST_PROCESS_DIVISOR  = 1,
  VOTE_POST_PROCESS_SOFTMAX  = 2,
  VOTE_POST_PROCESS_SIGMOID  = 3
} vote_post_process_t;


/**
 * An ensemble is a collection of trees.
 **/
typedef struct vote_ensemble {
  vote_tree_t          **trees;
  size_t                 nb_trees;
  size_t                 nb_inputs;
  size_t                 nb_outputs;
  size_t                 nb_nodes;
  vote_post_process_t    post_process;
  struct vote_ensemble **onevsall;
  size_t                *feature_usage;
} vote_ensemble_t;


/**
 * The outcome of a property checker can be inconclusive when approximations
 * are too conservative.
 **/
typedef enum vote_outcome {
  VOTE_UNSURE = -1,
  VOTE_FAIL   = 0,
  VOTE_PASS   = 1
} vote_outcome_t;


/**
 * A dataset in the form of a matrix of reals.
 **/
typedef struct vote_dataset {
  char   *filename;
  size_t  nb_rows;
  size_t  nb_cols;
  real_t *data;
} vote_dataset_t;


typedef struct vote_explain_stats {
  size_t nb_queries;
  double tm_queries;
} vote_explain_stats_t;


/**
 * Callback function prototype used to iterate input/output mappings.
 * Returning a conclusive outcome (pass/fail) stops the iterations.
 **/
typedef vote_outcome_t (vote_mapping_cb_t)(void *ctx, vote_mapping_t *mapping);


/**
 *
 **/
typedef bool (vote_explain_cb_t)(void *ctx, const bool *evec, size_t evec_size);


/**
 * Obtain the version number of VoTE.
 **/
const char* vote_version(void);


/**
 * Compute the argmax of a vector with reals.
 **/
size_t vote_argmax(const real_t *fvec, size_t length);


/**
 * Compute the argmin of a vector with reals.
 **/
size_t vote_argmin(const real_t *fvec, size_t length);


/**
 * Normalize a vector.
 **/
void vote_normalize(real_t *fvec, size_t length);


/**
 * Get the smallest value in a vector of reals
 **/
real_t vote_vector_min(const real_t* fvec, size_t length);


/**
 * Get the largest value in a vector of reals
 **/
real_t vote_vector_max(const real_t* fvec, size_t length);


/**
 * Get the average (mean) value of a vector of reals
 **/
real_t vote_vector_avg(const real_t* fvec, size_t length);


/**
 * Forward declaration for timespec.
 **/
struct timespec;


/**
 * Compare two timespecs.
 **/
int vote_timespec_cmp(struct timespec *ts1, struct timespec *ts2);


/**
 * Add seconds to a timespec.
 **/
void vote_timespec_add(struct timespec *ts, double seconds);


/**
 * Get the number of seconds that differ between *start* and *stop*.
 **/
double vote_timespec_diff(struct timespec *start, struct timespec *stop);


/**
 * Get number of seconds in floating-point form
 **/
double vote_timespec_seconds(const struct timespec *ts);


/**
 * Get the CPU time consumed by the calling thread.
 **/
void vote_timespec_thread_clock(struct timespec *ts);


/**
 * Get the CPU time consumed by the calling process.
 **/
void vote_timespec_proc_clock(struct timespec *ts);


/**
 * Get the system-wide clock that measures realtime.
 **/
void vote_timespec_realtime_clock(struct timespec *ts);


/**
 * Load a CSV file into memory. 
 *
 * A correctly formatted CSV file (using the comma delimiter) is assumed.
 **/
vote_dataset_t* vote_csv_load(const char *filename);


/**
 * Delete a dataset and free associated resources.
 **/
void vote_dataset_del(vote_dataset_t *ds);


/**
 * Get the row at a particular index in a dataset.
 **/
real_t* vote_dataset_row(vote_dataset_t *ds, size_t index);


/**
 * Create a new mapping with the given input/output dimensions. Input bounds
 * are initialized to [-∞, ∞], and output bounds are initialized to [0, 0].
 **/
vote_mapping_t* vote_mapping_new(size_t input_dim, size_t output_dim);


/**
 * Create a deep copy of a mapping.
 **/
vote_mapping_t* vote_mapping_copy(const vote_mapping_t *m);


/**
 * Check if the argmax of a mapping is as expected.
 **/
vote_outcome_t vote_mapping_check_argmax(const vote_mapping_t* m, size_t expected);


/**
 * Check if the argmin of a mapping is as expected.
 **/
vote_outcome_t vote_mapping_check_argmin(const vote_mapping_t *m, size_t expected);


/**
 * Check if a mapping is precise (lower and upper output bounds).
 **/
bool vote_mapping_precise(const vote_mapping_t *m);


/**
 * Compute the argmax of a mapping.
 *
 * Note: Negative one (-1) indicate an inconclusive mapping.
 **/
int vote_mapping_argmax(const vote_mapping_t *m);


/**
 * Compute the argmin of a mapping.
 *
 * Note: Negative one (-1) indicate an inconclusive mapping.
 **/
int vote_mapping_argmin(const vote_mapping_t *m);


/**
 * Delete a mapping, including its bounds.
 **/
void vote_mapping_del(vote_mapping_t* m);


/**
 * Load an ensemble from disk persisted in a JSON-based format.
 **/
vote_ensemble_t *vote_ensemble_load_file(const char *filename);


/**
 * Load an ensemble from a JSON-based formated string.
 **/
vote_ensemble_t *vote_ensemble_load_string(const char *filename);


/**
 * Load an ensemble from disk persisted in the (binary) xgboost format.
 **/
vote_ensemble_t *vote_xgboost_load_file(const char *filename);


/**
 * Load an ensemble from a blob in the (binary) xgboost format.
 **/
vote_ensemble_t* vote_xgboost_load_blob(void *data, size_t size);


/**
 * Save an ensemble as a JSON-based formated string.
 **/
const char* vote_ensemble_save_string(const vote_ensemble_t *e);


/**
 * Save an ensemble to disk in a JSON-based format.
 **/
bool vote_ensemble_save_file(const vote_ensemble_t *e, const char *filename);


/**
 * Delete an ensemble and all of its trees.
 **/
void vote_ensemble_del(vote_ensemble_t *e);


/**
 * Evaluate an ensemble on concrete values.
 **/
void vote_ensemble_eval(const vote_ensemble_t *e, const real_t *inputs,
			real_t *outputs);


/**
 * Iterate all feasible mappings of an ensemble for some input region.
 *
 * Returns true if all mappings were satisified, and false if any were 
 * unsatisfied.
 **/
bool vote_ensemble_forall(const vote_ensemble_t *e,
			  const vote_bound_t* input_region,
			  vote_mapping_cb_t *cb, void* ctx);


/**
 * Iterate abstract mappings of an ensemble using a abstraction-refinement
 * approach for some input region. Should the property not hold, a counter
 * example will be copied to *coex* when it is not NULL.
 **/
vote_outcome_t vote_absref_query(const vote_ensemble_t *e,
				 const vote_bound_t *input_region,
				 vote_mapping_cb_t *cb, void *ctx,
				 vote_bound_t *coex);


/**
 * Wrapper function for vote_absref_query().
 **/
vote_outcome_t vote_ensemble_absref(const vote_ensemble_t *e,
				    const vote_bound_t* input_region,
				    vote_mapping_cb_t *cb, void* ctx);


/**
 * Approximate a pessimistic and sound mapping for a given input region.
 **/
vote_mapping_t *vote_ensemble_approximate(const vote_ensemble_t *e,
					  const vote_bound_t* input_region);


/**
 * The number of times each feature splits an input region into smaller pieces.
 **/
void vote_ensemble_feature_usage(const vote_ensemble_t *e,
				 const vote_bound_t *input_region,
				 size_t *feature_usage);


/**
 * Check if an explanation explanation is valid for a given input point.
 *
 * The input parameter *present_vars* encodes which elements are included in
 * the explanation.
 **/
bool vote_explain_is_valid(const vote_ensemble_t *e,
			   const real_t *input_point,
			   const bool *present_vars,
			   real_t *coex);


/**
 * Compute a minimal explanation from the given input point.
 *
 * The output parameter *present_vars* encodes which elements are included in
 * the explanation.
 **/
void vote_explain_minimal(const vote_ensemble_t *e,
			  const real_t *input_point,
			  bool *present_vars);


/**
 * Compute a minimum explanation from the given input point.
 *
 * Equivalent to vote_explain_minimum_marco().
 **/
void vote_explain_minimum(const vote_ensemble_t *e,
			  const real_t *input_point,
			  const real_t *feature_costs,
			  bool *present_vars);

/**
 * Compute a minimum explanation from the given input point using the MARCO
 * approach, and keep track of the number of queries and elapsed time spent
 * in the valid explanation oracle.
 **/
void vote_explain_minimum_marco(const vote_ensemble_t *e,
				const real_t *input_point,
				const real_t *feature_costs, bool *present_vars,
				vote_explain_stats_t *stats);


/**
 * Compute a minimum explanation from the given input point using the B&B
 * approach, and keep track of the number of queries and elapsed time spent
 * in the valid explanation oracle.
 **/
void vote_explain_minimum_bb(const vote_ensemble_t *e,
			     const real_t *input_point,
			     const real_t *feature_costs, bool *present_vars,
			     vote_explain_stats_t *stats);



/**
 * Enumerate all minimal explanations from the given input point.
 *
 * Equivalent to:
 *   vote_explain_forall_marco(e, input_point, cb, ctx, NULL, NULL);
 **/
void vote_explain_forall(const vote_ensemble_t *e,
			 const real_t *input_point,
			 vote_explain_cb_t *cb, void* ctx);


/**
 *
 **/
void vote_explain_forall_marco(const vote_ensemble_t *e,
			       const real_t *input_point,
			       vote_explain_stats_t *stats,
			       vote_explain_cb_t *cb, void* ctx);


#endif //VOTE_H
