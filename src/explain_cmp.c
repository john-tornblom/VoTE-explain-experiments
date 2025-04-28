/* Copyright (C) 2023 John Törnblom

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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include <argp.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>
#include <sys/wait.h>

#include <vote.h>


/**
 * Keep track of command line options for explanations.
 **/
typedef struct explanation_args {
  vote_ensemble_t *ensemble;
  time_t           timeout;
  vote_dataset_t  *dataset;
  uint8_t          algo;
  FILE            *output;
  size_t           threads;
} explanation_args_t;



/**
 * When child procs time out, SIGUSR1 is signaled by the parent.
 **/
static vote_explain_stats_t g_stats  = {0};
static int                  g_cost   = -1;
static int                  g_pipefd = -1;


static void
pipe_results(void) {
  if(write(g_pipefd, &g_cost, sizeof(g_cost)) != sizeof(g_cost)) {
    perror("write");
  }

  if(write(g_pipefd, &g_stats, sizeof(g_stats)) != sizeof(g_stats)) {
    perror("write");
  }
}

static void
on_SIGALRM(int sig) {
  pipe_results();
  _exit(1);
}


/**
 *
 **/
static size_t
exec_explain_minimum(const vote_ensemble_t *e, const real_t *sample,
		     uint8_t algo, vote_explain_stats_t *stats) {
  size_t wvec[e->nb_inputs];
  bool evec[e->nb_inputs];
  size_t cost = 0;
  
  // Assume all features costs the same.
  for(size_t i=0; i<e->nb_inputs; i++) {
    wvec[i] = 1;
  }

  switch(algo) {
  default:
  case 0: // marco
    vote_explain_minimum_marco(e, sample, wvec, evec, stats);
    break;
    
  case 1: // minimum hitting set
    vote_explain_minimum_mhs(e, sample, wvec, evec, stats);
    break;
    
  case 2: // branch & bound
    vote_explain_minimum_bb(e, sample, wvec, evec, stats);
    break;
  }

  for(size_t i=0; i<e->nb_inputs; i++) {
    if(evec[i]) {
      cost += wvec[i];
    }
  }

  return cost;
}


/**
 *
 **/
static pid_t
spawn_explain_minimum(const vote_ensemble_t *e, const real_t *sample,
		      uint8_t algo, int* pipefd, time_t timeout) {
  int fds[2];
  pid_t pid;

  if(pipe(fds) < 0) {
    perror("pipe");
    abort();
  }
  
  pid = fork();
  if(pid < 0) {
    perror("fork");
    abort();
  }

  if(pid == 0) {
    signal(SIGALRM, on_SIGALRM);
    close(fds[0]);
    g_pipefd = fds[1];

    alarm(timeout);
    g_cost = exec_explain_minimum(e, sample, algo, &g_stats);
    pipe_results();
    _exit(0);
  } else {
    close(fds[1]);
    *pipefd = fds[0];
  }

  return pid;
}


/**
 * Synthesize explanations for a set of predictions.
 **/
static void
run_experiments(explanation_args_t *args) {
  size_t nb_samples = args->dataset->nb_rows;
  //
  vote_explain_stats_t stats[nb_samples];
  real_t runtimes[nb_samples];
  int costs[nb_samples];
  //
  size_t rowmap[args->threads];
  pid_t pidmap[args->threads];
  int pipemap[args->threads];
  //
  size_t nb_forks = 0;
  size_t next_row = 0;

  memset(rowmap, 0, sizeof(rowmap));
  memset(pidmap, 0, sizeof(pidmap));
  memset(pipemap, 0, sizeof(pipemap));
  
  memset(stats, 0, sizeof(stats));
  memset(costs, 0, sizeof(costs));
  memset(runtimes, 0, sizeof(runtimes));
    
  printf("explain:dataset:    %s\n", args->dataset->filename);
  printf("explain:timeout:    %lds\n", args->timeout);
  printf("explain:nb_inputs:  %ld\n", args->ensemble->nb_inputs);
  printf("explain:nb_outputs: %ld\n", args->ensemble->nb_outputs);
  printf("explain:nb_trees:   %ld\n", args->ensemble->nb_trees);
  printf("explain:nb_nodes:   %ld\n", args->ensemble->nb_nodes);

  if(args->algo == 0) {
    printf("explain:algo:       MARCO\n");
  }
  if(args->algo == 1) {
    printf("explain:algo:       MHS\n");
  }
  if(args->algo == 2) {
    printf("explain:algo:       BB\n");
  }
  
  do {
    // read from pipes
    for(size_t i=0; i<args->threads; i++) {
      if(!pipemap[i]) {
	continue;
      }

      struct pollfd pfd = {
	.fd = pipemap[i],
	.events = POLLIN,
      };
      
      if(poll(&pfd, 1, 1) < 0) {
	perror("poll");
	abort();
      }

      if(pfd.revents & POLLIN) {
	size_t row = rowmap[i];
	struct timespec ts;
	clockid_t cid;
	
	if(read(pfd.fd, &costs[row], sizeof(costs[row])) != sizeof(costs[row])) {
	  perror("read");
	  abort();
	}
	if(read(pfd.fd, &stats[row], sizeof(stats[row])) != sizeof(stats[row])) {
	  perror("read");
	  abort();
	}
	if(clock_getcpuclockid(pidmap[i], &cid) < 0) {
	  perror("clock_getcpuclockid");
	  abort();
	}
	if(clock_gettime(cid, &ts) < 0) {
	  perror("clock_gettime");
	  abort();
	}
	runtimes[row] = ts.tv_sec + (ts.tv_nsec / 1e9);

	if(waitpid(pidmap[i], NULL, 0) < 0) {
	  perror("waitpid");
	  abort();
	}
	close(pipemap[i]);
	pidmap[i] = 0;
	nb_forks--;	
      }
    }

    // create child proc
    if(nb_forks < args->threads && next_row < nb_samples) {
      real_t *sample = vote_dataset_row(args->dataset, next_row);
      for(size_t i=0; i<args->threads; i++) {
	if(pidmap[i]) {
	  continue; // slot taken
	}
	rowmap[i] = next_row++;
	pidmap[i] = spawn_explain_minimum(args->ensemble, sample, args->algo,
					  &pipemap[i], args->timeout);
	nb_forks++;
	break;
      }
    }
    if(isatty(STDOUT_FILENO)) {
      printf("explain:progress:   %ld/%ld\r", next_row-nb_forks, nb_samples);
      fflush(stdout);
    }
    
    usleep(10000);

  } while(nb_forks);

  real_t runtime = 0;
  size_t timeouts = 0;
  size_t nb_queries_pass = 0;
  size_t nb_queries_fail = 0;
  real_t tm_queries_pass = 0;
  real_t tm_queries_fail = 0;
  
  for(size_t i=0; i<nb_samples; i++) {
    real_t *sample = vote_dataset_row(args->dataset, i);
    for(size_t j=0; j<args->ensemble->nb_inputs; j++) {
      fprintf(args->output, "%f,", sample[j]);
    }

    fprintf(args->output, "%d,%f,%f,%f,%ld,%ld\n",
	    costs[i], runtimes[i],
	    stats[i].tm_queries_pass, stats[i].tm_queries_fail,
	    stats[i].nb_queries_pass, stats[i].nb_queries_fail);

    runtime += runtimes[i];
    timeouts += (costs[i] < 0);

    tm_queries_pass += stats[i].tm_queries_pass;
    tm_queries_fail += stats[i].tm_queries_fail;
    
    nb_queries_pass += stats[i].nb_queries_pass;
    nb_queries_fail += stats[i].nb_queries_fail;
  }

    
  printf("explain:runtime:    %2.2f\n", runtime);
  printf("explain:aborted:    %ld\n", timeouts);
  printf("explain:querytime:  %2.2f\n", tm_queries_pass + tm_queries_fail);
  printf("explain:nb_queries: %ld\n", nb_queries_pass + nb_queries_fail);
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

  case 'A': //algo
    if(!strcasecmp(arg, "BB")) {
      args->algo = 2;
    } else if(!strcasecmp(arg, "MHS")) {
      args->algo = 1;
    } else if(!strcasecmp(arg, "MARCO")) {
      args->algo = 0;
    } else {
      fprintf(stderr, "Unknown algorithm '%s'\n", arg);
      return ARGP_KEY_ERROR;
    }
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

    {.name="algo", .key='A', .arg="STRING",
     .doc="Compute a minimum-sized explanation using a specific algorithm: "
          "MARCO (default), MHS, or BB"},
    
    {.name="output", .key='o', .arg="PATH",
     .doc="Save results to PATH (encoded in the CSV format)"},

    {0}
  };

  struct argp argp = {
    .parser   = parse_cb,
    .doc      = "Synthesize minimum explanations made by a tree-based classifier"
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

  run_experiments(&args);

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

