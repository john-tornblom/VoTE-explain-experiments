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
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <vote.h>

#include "parson.h"


/**
 * Global variables.
 **/
vote_dataset_t      *g_dataset   = NULL;
size_t               g_instance  = 0;
vote_ensemble_t     *g_ensemble  = NULL;
int                  g_algo      = 0;
FILE*                g_output    = NULL;
size_t               g_timeout   = UINT_MAX;


/**
 * Encode an array of ints into a JSON number array.
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
json_encode_expl(const bool *evec, size_t size) {
  struct json_array_t* array;
  size_t len = 0;
  int expl[size];

  array = json_value_get_array(json_value_init_array());
  for(int i=0; i<size; i++) {
    if(evec && evec[i]) {
      expl[len++] = i;
    }
  }
  if(evec) {
    json_array_append_value(array, json_encode_ints(expl, len));
  }

  return json_array_get_wrapping_value(array);
}


/**
 * Encode results in a JSON format.
 **/
static struct json_value_t*
json_encode_results(const bool *evec, real_t runtime) {
  struct json_value_t* root = json_value_init_object();
  struct json_object_t* obj = json_value_get_object(root);
  const real_t *sample;

  sample = vote_dataset_row(g_dataset, g_instance);

  json_object_set_value(obj, "sample",
			json_encode_reals(sample, g_ensemble->nb_inputs));
  json_object_set_value(obj, "explanations",
			json_encode_expl(evec, g_ensemble->nb_inputs));
  json_object_set_number(obj, "runtime", runtime);
  json_object_set_boolean(obj, "aborted", evec == NULL);

  return root;
}


/**
 * Synthesize a minimum explanation for a prediction.
 **/
static void
synthesize_explanation(void) {
  size_t wvec[g_ensemble->nb_inputs];
  bool evec[g_ensemble->nb_inputs];
  struct json_value_t* json_root;
  struct timespec ts1, ts2;
  const real_t *sample;
  real_t runtime;
  char *buf;

  // Assume all features costs the same.
  for(size_t i=0; i<g_ensemble->nb_inputs; i++) {
    wvec[i] = 1;
  }

  sample = vote_dataset_row(g_dataset, g_instance);

  vote_timespec_thread_clock(&ts1);
  vote_explain_minimal(g_ensemble, sample, evec);
  vote_timespec_thread_clock(&ts2);

  runtime = vote_timespec_diff(&ts1, &ts2);

  printf("explain:outcome:    {");
  const char *prefix = "";
  for(size_t i=0; i<g_ensemble->nb_inputs; i++) {
    if(evec[i]) {
      printf("%s%ld", prefix, i);
      prefix = ", ";
    }
  }
  printf("}\n");
  printf("explain:runtime:    %2.2fs\n", runtime);

  json_root = json_encode_results(evec, runtime);
  buf = json_serialize_to_string(json_root);
  fputs(buf, g_output);
  json_free_serialized_string(buf);
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
  char *buf;

  printf("explain:outcome:    timeout\n");
  printf("explain:runtime:    %lds\n", g_timeout);

  json_root = json_encode_results(NULL, g_timeout);
  buf = json_serialize_to_string(json_root);
  fputs(buf, g_output);
  json_free_serialized_string(buf);
  json_value_free(json_root);
  
  exit(1);
}


/**
 * Parse command line arguments and launch the syntheses.
 **/
int
main(int argc, char** argv) {
  const real_t *sample;
  const char *algo;

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
    .doc      = "Compute a minimal explanation for a prediction made by"
                " a tree-based classifier for the i-th sample"
                " stored in CSV_FILE",
  };

  g_output = stderr;
  if(argp_parse(&argp, argc, argv, 0, 0, 0)) {
    exit(1);
  }

  printf("explain:dataset:    %s\n", g_dataset->filename);
  printf("explain:instance:   %ld\n", g_instance);
  printf("explain:timeout:    %lds\n", g_timeout);
  printf("explain:type:       minimal (deletion filter)\n");
  printf("explain:nb_inputs:  %ld\n", g_ensemble->nb_inputs);
  printf("explain:nb_outputs: %ld\n", g_ensemble->nb_outputs);
  printf("explain:nb_trees:   %ld\n", g_ensemble->nb_trees);
  printf("explain:nb_nodes:   %ld\n", g_ensemble->nb_nodes);

  signal(SIGALRM, on_timeout);
  alarm(g_timeout);

  synthesize_explanation();

  if(g_ensemble) {
    vote_ensemble_del(g_ensemble);
  }

  if(g_dataset) {
    vote_dataset_del(g_dataset);
  }

  if(g_output != stderr) {
    fclose(stderr);
  }
}


/**
 * Accessed by argp_parse().
 **/
const char *argp_program_version = VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

