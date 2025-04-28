/* Copyright (C) 2020 John Törnblom

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

#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#include "workqueue.h"


typedef struct task {
  workqueue_cb_t  *cb;
  void            *ctx;
  time_t           timeout;
  struct task     *next;
} task_t;


struct workqueue {
  task_t         *queue;
  pthread_mutex_t lock;
  size_t          length;
};


static task_t*
workqueue_pop_task(workqueue_t *wq) {
  pthread_mutex_lock(&wq->lock);

  task_t *task = wq->queue;
  if(task) {
    wq->queue = task->next;
  }

  pthread_mutex_unlock(&wq->lock);

  return task;
}


static void*
workqueue_thread(void *ctx) {
  task_t *task = (task_t*)ctx;

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  task->cb(task->ctx);
  pthread_exit(NULL);
  
  return NULL;
}


static void*
workqueue_dispatch(void *ctx) {
  workqueue_t* wq = (workqueue_t*)ctx;
  struct timespec ts;
  pthread_t trd;
  task_t *task;

  while((task = workqueue_pop_task(wq))) {
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += task->timeout;
    
    pthread_create(&trd, NULL, workqueue_thread, task);
    if(ETIMEDOUT == pthread_timedjoin_np(trd, NULL, &ts)) {
      pthread_cancel(trd);
    }

    free(task);
    
    pthread_mutex_lock(&wq->lock);
    wq->length--;
    pthread_mutex_unlock(&wq->lock);
  }
  
  pthread_exit(NULL);
  return NULL;
}


workqueue_t*
workqueue_new(void) {
  workqueue_t* wq = calloc(1, sizeof(workqueue_t));
  assert(wq);

  pthread_mutex_init(&wq->lock, NULL);
  wq->length = 0;
  
  return wq;
}


void
workqueue_del(workqueue_t* wq) {
  task_t* next;
  
  while(wq->queue) {
    next = wq->queue->next;
    free(wq->queue);
    wq->queue = next;
  }
  
  free(wq);
}


void
workqueue_launch_nb(workqueue_t* wq, unsigned short nb_threads) {
  pthread_t threads[nb_threads];

  for(size_t i=0; i<nb_threads; i++) {
    pthread_create(&threads[i], NULL, workqueue_dispatch, wq);
  }
}


void
workqueue_launch(workqueue_t* wq, unsigned short nb_threads) {
  pthread_t threads[nb_threads];

  for(size_t i=0; i<nb_threads; i++) {
    pthread_create(&threads[i], NULL, workqueue_dispatch, wq);
  }
  
  for(size_t i=0; i<nb_threads; i++) {
    pthread_join(threads[i], NULL);
  }
}


void
workqueue_schedule(workqueue_t *wq, workqueue_cb_t *cb, void *ctx, time_t timeout) {
  task_t* task = calloc(1, sizeof(task_t));
  assert(task);

  task->cb      = cb;
  task->ctx     = ctx;
  task->timeout = timeout;
  
  pthread_mutex_lock(&wq->lock);
  task->next = wq->queue;
  wq->queue = task;
  wq->length++;
  pthread_mutex_unlock(&wq->lock);
}


size_t
workqueue_length(workqueue_t *wq) {
  size_t length;
  
  pthread_mutex_lock(&wq->lock);
  length = wq->length;
  pthread_mutex_unlock(&wq->lock);

  return length;
}

