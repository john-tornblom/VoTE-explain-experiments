/* Copyright (C) 2022 John Törnblom

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include <argp.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <vote.h>

#include "workqueue.h"
#include "parson.h"


/**
 * 
 **/
typedef struct expl_seq {
  int             *indices;
  size_t           nb_indices;
  struct expl_seq *next;
} expl_seq_t;


/**
 * 
 **/
typedef struct expl_query {
  vote_ensemble_t     *ensemble;
  real_t              *sample;
  real_t               runtime;
  expl_seq_t          *expl_seq;
  bool                 timedout;
  vote_explain_stats_t stats;
  pthread_mutex_t      lock;
} expl_query_t;


/**
 * Keep track of command line options for explanations.
 **/
typedef struct explanation_args {
  vote_ensemble_t *ensemble;
  time_t           timeout;
  size_t           threads;
  vote_dataset_t  *dataset;
  FILE            *output;
  bool             minimum;
  bool             forall;
} explanation_args_t;


/**
 * Callback function used by forall_explanation_thread().
 **/
static bool
forall_explanation_cb(void* ctx, const bool *evec, size_t length) {
  expl_query_t *eq = (expl_query_t*)ctx;
  expl_seq_t *seq;
  int oldstate;

  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

  seq = calloc(1, sizeof(expl_seq_t));
  seq->indices = malloc(length * sizeof(int));
  seq->nb_indices = 0;

  for(size_t i=0; i<length; i++) {
    if(evec[i]) {
      seq->indices[seq->nb_indices++] = i;
    }
  }

  pthread_mutex_lock(&eq->lock);
  seq->next = eq->expl_seq;
  eq->expl_seq = seq;
  pthread_mutex_unlock(&eq->lock);

  pthread_setcancelstate(oldstate, &oldstate);

  return true;
}


/**
 * Invoke vote_explain_forall() from a seperate thread.
 **/
static void
forall_explanation_thread(void* ctx) {
  expl_query_t *eq = (expl_query_t*)ctx;
  vote_ensemble_t *e = eq->ensemble;
  struct timespec ts1;
  struct timespec ts2;

  pthread_mutex_init(&eq->lock, NULL);
  vote_timespec_thread_clock(&ts1);
  vote_explain_forall_marco(e, eq->sample, &eq->stats,
			    forall_explanation_cb, eq);
  vote_timespec_thread_clock(&ts2);

  eq->runtime = vote_timespec_diff(&ts1, &ts2);
  eq->timedout = false;
}



/**
 * Invoke vote_explain_minimum() from a seperate thread.
 **/
static void
minimum_explanation_thread(void* ctx) {
  expl_query_t *eq = (expl_query_t*)ctx;
  vote_ensemble_t *e = eq->ensemble;
  size_t wvec[e->nb_inputs];
  bool evec[e->nb_inputs];
  struct timespec ts1;
  struct timespec ts2;
  expl_seq_t *seq;
  int oldstate;

  // Assume all features costs the same.
  for(size_t i=0; i<e->nb_inputs; i++) {
    wvec[i] = 1;
  }

  vote_timespec_thread_clock(&ts1);
  vote_explain_minimum_marco(e, eq->sample, wvec, evec, &eq->stats);
  vote_timespec_thread_clock(&ts2);

  eq->runtime = vote_timespec_diff(&ts1, &ts2);
  eq->timedout = false;
  
  // Add a copy of the explanation to linked list.
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
  seq = calloc(1, sizeof(expl_seq_t));
  seq->next = eq->expl_seq;
  eq->expl_seq = seq;
  
  seq->indices = calloc(e->nb_inputs, sizeof(size_t));
  for(size_t i=0; i<e->nb_inputs; i++) {
    if(evec[i]) {
      seq->indices[seq->nb_indices++] = i;
    }
  }
  pthread_setcancelstate(oldstate, &oldstate);
}


/**
 * Invoke vote_explain_minimal() from a seperate thread.
 **/
static void
minimal_explanation_thread(void* ctx) {
  expl_query_t *eq = (expl_query_t*)ctx;
  vote_ensemble_t *e = eq->ensemble;
  bool evec[e->nb_inputs];
  struct timespec ts1;
  struct timespec ts2;
  expl_seq_t *seq;
  int oldstate;

  // Compute explanation, and meassure elapsed (thread) time.
  vote_timespec_thread_clock(&ts1);
  vote_explain_minimal(e, eq->sample, evec);
  vote_timespec_thread_clock(&ts2);

  eq->runtime = vote_timespec_diff(&ts1, &ts2);
  eq->timedout = false;
  
  // Add a copy of the explanation to linked list.
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
  seq = calloc(1, sizeof(expl_seq_t));
  seq->next = eq->expl_seq;
  eq->expl_seq = seq;
  seq->indices = calloc(e->nb_inputs, sizeof(size_t));
  for(size_t i=0; i<e->nb_inputs; i++) {
    if(evec[i]) {
      seq->indices[seq->nb_indices++] = i;
    }
  }
  pthread_setcancelstate(oldstate, &oldstate);
}


/**
 * Encode an array of integers into a JSON number array.
 **/
static struct json_value_t*
vote_encode_ints(int* values, size_t length) {
  struct json_value_t* root = json_value_init_array();
  struct json_array_t* array = json_value_get_array(root);

  for(size_t i=0; i<length; i++) {
    json_array_append_number(array, values[i]);
  }

  return root;
}


/**
 * Encode an array of reals into a JSON number array.
 **/
static struct json_value_t*
vote_encode_reals(real_t* values, size_t length) {
  struct json_value_t* root = json_value_init_array();
  struct json_array_t* array = json_value_get_array(root);

  for(size_t i=0; i<length; i++) {
    json_array_append_number(array, values[i]);
  }

  return root;
}


/**
 * Serialize an explanation.
 **/
static struct json_value_t*
serialize_expl_seq(expl_seq_t *seq, struct json_array_t* array) {
  if(!array) {
    array = json_value_get_array(json_value_init_array());
  }

  if(!seq) {
    return json_array_get_wrapping_value(array);
  }

  if(seq->nb_indices) {
    json_array_append_value(array, vote_encode_ints(seq->indices,
						    seq->nb_indices));
  }

  return serialize_expl_seq(seq->next, array);
}


/**
 * Serialize explanations in a JSON format.
 **/
static struct json_value_t*
serialize_query(expl_query_t *eq) {
  struct json_value_t* root = json_value_init_object();
  struct json_object_t* obj = json_value_get_object(root);

  json_object_set_value(obj, "sample", vote_encode_reals(eq->sample, eq->ensemble->nb_inputs));
  json_object_set_number(obj, "runtime", eq->runtime);
  json_object_set_boolean(obj, "timedout", eq->timedout);
  json_object_set_number(obj, "nb_queries_pass", eq->stats.nb_queries_pass);
  json_object_set_number(obj, "nb_queries_fail", eq->stats.nb_queries_fail);
  json_object_set_number(obj, "tm_queries_pass", eq->stats.tm_queries_pass);
  json_object_set_number(obj, "tm_queries_fail", eq->stats.tm_queries_fail);
  json_object_set_value(obj, "explanations", serialize_expl_seq(eq->expl_seq, NULL));

  return root;
}


/**
 * Synthesize explanations for a set of predictions.
 **/
static void
synthesize_explanations(explanation_args_t *args) {
  size_t nb_samples = args->dataset->nb_rows;
  workqueue_t *wq = workqueue_new();
  expl_query_t queries[nb_samples];
  struct json_array_t* json_array;
  struct json_value_t* json_value;
  struct json_value_t* json_root;
  size_t unused = 0;
  char *buf;

  for(size_t row=0; row<nb_samples; row++) {
    queries[row].ensemble   = args->ensemble;
    queries[row].sample     = vote_dataset_row(args->dataset, row);
    queries[row].expl_seq   = NULL;
    queries[row].runtime    = args->timeout;
    queries[row].timedout   = true;
    memset(&queries[row].stats, 0, sizeof(queries[row].stats));
    

    if(args->forall) {
      workqueue_schedule(wq, forall_explanation_thread, &queries[row],
			 args->timeout);
    } else if(args->minimum) {
      workqueue_schedule(wq, minimum_explanation_thread, &queries[row],
			 args->timeout);

    } else {
      workqueue_schedule(wq, minimal_explanation_thread, &queries[row],
			 args->timeout);
    }
  }

  for(size_t i=0; i<args->ensemble->nb_inputs; i++) {
    unused += (!args->ensemble->feature_usage[i]);
  }

  printf("explain:dataset:         %s\n", args->dataset->filename);
  printf("explain:timeout:         %lds\n", args->timeout);
  printf("explain:nb_inputs:       %ld (%ld unused)\n", args->ensemble->nb_inputs, unused);
  printf("explain:nb_outputs:      %ld\n", args->ensemble->nb_outputs);
  printf("explain:nb_trees:        %ld\n", args->ensemble->nb_trees);
  printf("explain:nb_nodes:        %ld\n", args->ensemble->nb_nodes);

  if(args->minimum) {
    printf("explain:mode:            minimum\n");
  } else if(args->forall) {
    printf("explain:mode:            forall\n");
  } else {
    printf("explain:mode:            minimal\n");
  }
  workqueue_launch_nb(wq, args->threads);
  while(workqueue_length(wq)) {
    if(isatty(STDOUT_FILENO)) {
      printf("explain:progress:        %ld/%ld\r",
	     nb_samples - workqueue_length(wq), nb_samples);
      fflush(stdout);
    }
    usleep(10000);
  }

  json_root = json_value_init_array();
  json_array = json_value_get_array(json_root);
  size_t timeouts = 0;
  size_t nb_queries_pass = 0;
  size_t nb_queries_fail = 0;
  real_t tm_queries_pass = 0;
  real_t tm_queries_fail = 0;
  real_t runtime = 0;
  
  for(size_t row=0; row<nb_samples; row++) {
    if(queries[row].timedout) {
      queries[row].runtime = args->timeout;
    }
    json_value = serialize_query(&queries[row]);
    json_array_append_value(json_array, json_value);

    while(queries[row].expl_seq) {
      expl_seq_t *next = queries[row].expl_seq->next;
      free(queries[row].expl_seq->indices);
      free(queries[row].expl_seq);

      queries[row].expl_seq = next;
    }

    timeouts += queries[row].timedout;
    runtime += queries[row].runtime;

    tm_queries_pass += queries[row].stats.tm_queries_pass;
    tm_queries_fail += queries[row].stats.tm_queries_fail;
    
    nb_queries_pass += queries[row].stats.nb_queries_pass;
    nb_queries_fail += queries[row].stats.nb_queries_fail;
  }

  printf("explain:runtime:         %2.2f\n", runtime);
  printf("explain:aborted:         %ld\n", timeouts);
  printf("explain:nb_queries_pass: %ld\n", nb_queries_pass);
  printf("explain:nb_queries_fail: %ld\n", nb_queries_fail);
  printf("explain:tm_queries_pass: %2.2f\n", tm_queries_pass);
  printf("explain:tm_queries_fail: %2.2f\n", tm_queries_fail);
  
  buf = json_serialize_to_string(json_root);
  fputs(buf, args->output);
  json_free_serialized_string(buf);
  json_array_clear(json_array);
  json_value_free(json_root);

  workqueue_del(wq);
}


/**
 * Parse command line arguments.
 **/
static error_t
parse_cb(int key, char *arg, struct argp_state *state) {
  explanation_args_t *args = state->input;

  switch(key) {
  case 'm': //model
    if(!(args->ensemble = vote_ensemble_load_file(arg))) {
      fprintf(stderr, "Unable to load model from '%s'\n", arg);
      return ARGP_KEY_ERROR;
    }
    break;

  case 'M': //minimum
    args->minimum = true;
    break;

  case 'a': //all
    args->forall = true;
    break;
    
  case 'o': //output
    if(!(args->output=fopen(arg, "w"))) {
      fprintf(stderr, "Unable to open '%s'\n", arg);
      return ARGP_KEY_ERROR;
    }
    break;

  case 'T': //timeout
    args->timeout = atof(arg);
    break;

  case 't': //threads
    args->threads = atoi(arg);
    break;

  case ARGP_KEY_ARG: //CSV_FILE
    if(!(args->dataset = vote_csv_load(arg))) {
      fprintf(stderr, "Unable to load data from '%s'\n", arg);
      return ARGP_KEY_ERROR;
    }
    break;

  case ARGP_KEY_END:
    if(state->arg_num < 1) {
      argp_usage(state);
    }
    break;

  default:
    return ARGP_ERR_UNKNOWN;
  }

  return 0;
}


/**
 * Parse command line arguments and launch the syntheses.
 **/
int
main(int argc, char** argv) {
  struct argp_option opts[] = {
    {.name="model", .key='m', .arg="PATH",
     .doc="Path to a serialized tree-based classifier"},

    {.name="threads", .key='t', .arg="NUMBER",
     .doc="Perform synthesis concurrently on a given NUMBER of threads"},

    {.name="timeout", .key='T', .arg="NUMBER",
     .doc="Timeout the synthesis of an explanation after NUMBER seconds"},

    {.name="minimum", .key='M',
     .doc="Compute a minimum-sized explanation"},

    {.name="all", .key='a',
     .doc="Compute all minimal explanations"},

    {.name="output", .key='o', .arg="PATH",
     .doc="Save results to PATH (encoded in the JSON format)"},

    {0}
  };

  struct argp argp = {
    .parser   = parse_cb,
    .doc      = "Synthesize explanations made by a tree-based classifier"
                " from samples stored in CSV_FILE",
    .args_doc = "CSV_FILE",
    .options  = opts
  };

  struct explanation_args args = {
    .timeout = UINT_MAX,
    .threads = sysconf(_SC_NPROCESSORS_ONLN),
    .output = stderr,
    .ensemble = NULL,
    .dataset = NULL
  };

  if(argp_parse(&argp, argc, argv, 0, 0, &args)) {
    exit(1);
  }

  synthesize_explanations(&args);

  if(args.ensemble) {
    vote_ensemble_del(args.ensemble);
  }

  if(args.dataset) {
    vote_dataset_del(args.dataset);
  }

  if(args.output != stderr) {
    fclose(stderr);
  }
}


/**
 * Accessed by argp_parse().
 **/
const char *argp_program_version = VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

