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

#include <argp.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <vote.h>

#include "parson.h"


/**
 * A sequence of explanations.
 **/
typedef struct expl_seq {
  int             *indices;
  size_t           nb_indices;
  struct expl_seq *next;
} expl_seq_t;


/**
 * Global variables.
 **/
vote_dataset_t        *g_dataset   = NULL;
size_t                 g_instance  = 0;
vote_ensemble_t       *g_ensemble  = NULL;
FILE*                  g_output    = 0;
size_t                 g_timeout   = UINT_MAX;
bool                   g_aborted   = true;
vote_explain_stats_t   g_stats     = {0};
expl_seq_t            *g_head      = 0;


/**
 * Encode an array of integers into a JSON number array.
 **/
static struct json_value_t*
json_encode_ints(const int* values, size_t length) {
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
json_encode_reals(const real_t* values, size_t length) {
  struct json_value_t* root = json_value_init_array();
  struct json_array_t* array = json_value_get_array(root);

  for(size_t i=0; i<length; i++) {
    json_array_append_number(array, values[i]);
  }

  return root;
}


/**
 * Encode an explanation in a JSON format.
 **/
static struct json_value_t*
json_encode_expl_seq(expl_seq_t *seq, struct json_array_t* array) {
  if(!array) {
    array = json_value_get_array(json_value_init_array());
  }

  if(!seq) {
    return json_array_get_wrapping_value(array);
  }

  json_array_append_value(array, json_encode_ints(seq->indices,
						  seq->nb_indices));

  return json_encode_expl_seq(seq->next, array);
}


/**
 * Encode results in a JSON format.
 **/
static struct json_value_t*
json_encode_results(bool aborted, real_t runtime) {
  struct json_value_t* root = json_value_init_object();
  struct json_object_t* obj = json_value_get_object(root);
  const real_t *sample;

  sample = vote_dataset_row(g_dataset, g_instance);

  json_object_set_value(obj, "sample",
			json_encode_reals(sample, g_ensemble->nb_inputs));
  json_object_set_value(obj, "explanations",
			json_encode_expl_seq(g_head, NULL));
  json_object_set_number(obj, "runtime", runtime);
  json_object_set_number(obj, "querytime", g_stats.tm_queries);
  json_object_set_number(obj, "queries", g_stats.nb_queries);
  json_object_set_boolean(obj, "aborted", aborted);

  return root;
}


/**
 * Callback function used by synthesize_explanations().
 **/
static bool
explanation_cb(void* ctx, const bool *evec, size_t length) {
  expl_seq_t *entry;

  entry = calloc(1, sizeof(expl_seq_t));
  entry->indices = malloc(length * sizeof(int));
  entry->nb_indices = 0;

  for(size_t i=0; i<length; i++) {
    if(evec[i]) {
      entry->indices[entry->nb_indices++] = i;
    }
  }

  entry->next = g_head;
  g_head      = entry;

  return true;
}


/**
 * Synthesize explanations for a predictions.
 **/
static void
synthesize_explanations(void) {
  struct json_value_t* json_root;
  struct timespec ts1, ts2;
  const real_t *sample;
  size_t nb_expl = 0;
  real_t runtime;
  char *buf;

  sample = vote_dataset_row(g_dataset, g_instance);

  vote_timespec_thread_clock(&ts1);
  vote_explain_forall_marco(g_ensemble, sample, &g_stats, explanation_cb, &g_head);
  vote_timespec_thread_clock(&ts2);

  runtime = vote_timespec_diff(&ts1, &ts2);
  
  json_root = json_encode_results(false, runtime);
  while(g_head) {
    expl_seq_t *next = g_head->next;
    free(g_head->indices);
    free(g_head);
    g_head = next;
    nb_expl++;
  }

  printf("explain:outcome:    %ld explanations\n", nb_expl);
  printf("explain:runtime:    %2.2fs\n", runtime);
  printf("explain:querytime:  %2.2fs\n", g_stats.tm_queries);
  printf("explain:queries:    %ld\n", g_stats.nb_queries);

  if(g_output) {
    buf = json_serialize_to_string(json_root);
    fputs(buf, g_output);
    json_free_serialized_string(buf);
  }
  json_value_free(json_root);
}


/**
 * Parse command line arguments.
 **/
static error_t
parse_cb(int key, char *arg, struct argp_state *state) {
  switch(key) {
  case 'm': //model
    if(!(g_ensemble=vote_ensemble_load_file(arg))) {
      fprintf(stderr, "Unable to load model from '%s'\n", arg);
      return ARGP_KEY_ERROR;
    }
    break;

  case 'i': //instance
    g_instance = atoi(arg);
    break;
    
  case 'o': //output
    if(!(g_output=fopen(arg, "w"))) {
      fprintf(stderr, "Unable to open '%s'\n", arg);
      return ARGP_KEY_ERROR;
    }
    break;

  case 't': //timeout
    g_timeout = atof(arg);
    break;

  case ARGP_KEY_ARG: //CSV_FILE
    if(!(g_dataset=vote_csv_load(arg))) {
      fprintf(stderr, "Unable to load data from '%s'\n", arg);
      return ARGP_KEY_ERROR;
    }
    break;

  case ARGP_KEY_END:
    if(state->arg_num < 1) {
      argp_failure(state, 1, 0, "No CSV file specified, see --help for more information");
    }
    
    if(!g_ensemble) {
      argp_failure(state, 1, 0, "No model specified, see --help for more information");
    }
    break;

  default:
    return ARGP_ERR_UNKNOWN;
  }

  return 0;
}


static void
on_timeout(int sig) {
  struct json_value_t* json_root;
  size_t nb_expl = 0;
  char *buf;

  json_root = json_encode_results(true, g_timeout);
  while(g_head) {
    expl_seq_t *next = g_head->next;
    free(g_head->indices);
    free(g_head);
    g_head = next;
    nb_expl++;
  }

  printf("explain:outcome:    %ld explanations (timeout)\n", nb_expl);
  printf("explain:runtime:    %lds\n", g_timeout);
  printf("explain:querytime:  %2.2fs\n", g_stats.tm_queries);
  printf("explain:queries:    %ld\n", g_stats.nb_queries);

  if(g_output) {
    buf = json_serialize_to_string(json_root);
    fputs(buf, g_output);
    json_free_serialized_string(buf);
  }
  json_value_free(json_root);
  
  exit(1);
}


/**
 * Parse command line arguments and launch the syntheses.
 **/
int
main(int argc, char** argv) {
  struct argp_option opts[] = {
    {.name="model", .key='m', .arg="PATH", .flags=0,
     .doc="Path to a serialized tree-based classifier (required)"},

    {.name="instance", .key='i', .arg="INDEX",
     .doc="Compute explanations for the i-th instance in CSV_FILE (defaults to 0)"},
    
    {.name="timeout", .key='t', .arg="NUMBER",
     .doc="Timeout the synthesis after NUMBER seconds"},

    {.name="output", .key='o', .arg="PATH",
     .doc="Save results to PATH (defaults to stderr)"},

    {0}
  };

  struct argp argp = {
    .options  = opts,
    .parser   = parse_cb,
    .args_doc = "-m PATH CSV_FILE",
    .doc      = "Enumerate all minimal explanations for a prediction made by"
                " a tree-based classifier for the i-th sample"
                " stored in CSV_FILE",
  };

  if(argp_parse(&argp, argc, argv, 0, 0, 0)) {
    exit(1);
  }

  printf("explain:dataset:    %s\n", g_dataset->filename);
  printf("explain:instance:   %ld\n", g_instance);
  printf("explain:timeout:    %lds\n", g_timeout);
  printf("explain:type:       enumeration (MARCO)\n");
  printf("explain:nb_inputs:  %ld\n", g_ensemble->nb_inputs);
  printf("explain:nb_outputs: %ld\n", g_ensemble->nb_outputs);
  printf("explain:nb_trees:   %ld\n", g_ensemble->nb_trees);
  printf("explain:nb_nodes:   %ld\n", g_ensemble->nb_nodes);

  signal(SIGALRM, on_timeout);
  alarm(g_timeout);

  synthesize_explanations();

  if(g_ensemble) {
    vote_ensemble_del(g_ensemble);
  }

  if(g_dataset) {
    vote_dataset_del(g_dataset);
  }

  if(g_output) {
    fclose(g_output);
  }

  return 0;
}


/**
 * Accessed by argp_parse().
 **/
const char *argp_program_version = VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

