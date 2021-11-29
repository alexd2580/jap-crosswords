#include <alloca.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algorithms.h"
#include "field.h"
#include "graphics.h"
#include "macros.h"

struct thread_data {
  pthread_mutex_t *mutex;
  pthread_cond_t *waitForJobs;
  pthread_cond_t *waitForResults;

  field_t *field;

  volatile int num_threads;
  volatile int num_threads_idle;
  volatile int next_task_id;
  volatile int num_iterations;

  // Solving function.
  void (*method)(algorithm_data_t *);

  // Skip combinatorical solving if more than this many possibilities.
  volatile int skip_threshold;
  volatile bool is_row;

  volatile bool field_updated;
  volatile bool rows_skipped;
};
typedef struct thread_data thread_data_t;

__attribute__((const)) char is_terminal_task_id(int task_id) {
  return task_id == -1;
}

char has_invalid_task_id(thread_data_t const *thread_data) {
  field_t *field = thread_data->field;
  int field_size = thread_data->is_row ? field->height : field->width;
  return thread_data->next_task_id >= field_size;
}

void *solve_single(void *arg) {
  thread_data_t *thread_data = (thread_data_t *)arg;
  field_t *field = thread_data->field;

  size_t buffer_size = (size_t)MAX(field->width, field->height);

  algorithm_data_t algorithm_data = {
      .field = (char *)alloca(buffer_size * sizeof(char)),
      // These two are used for the bruteforce method.
      .prepare = (char *)alloca(buffer_size * sizeof(char)),
      .suggest = (char *)alloca(buffer_size * sizeof(char)),
      .updated = false,
      .skipped = false,
  };

  printf("[%s] Synchronizing\n", __func__);

  // Wait for start signal.
  pthread_mutex_lock(thread_data->mutex);
  printf("[%s] Reporting idle, waiting for jobs\n", __func__);
  thread_data->num_threads_idle++;
  pthread_cond_signal(thread_data->waitForResults);
  pthread_cond_wait(thread_data->waitForJobs, thread_data->mutex);
  printf("[%s] Job received\n", __func__);
  thread_data->num_threads_idle--;

  while (true) {

    // Wait for jobs when the current batch is done.
    while (has_invalid_task_id(thread_data)) {
      thread_data->num_threads_idle++;
      pthread_cond_signal(thread_data->waitForResults);
      pthread_cond_wait(thread_data->waitForJobs, thread_data->mutex);
      thread_data->num_threads_idle--;
    }

    // When `next_task_id` is valid...
    int task_id = thread_data->next_task_id;
    thread_data->next_task_id += thread_data->next_task_id >= 0;

    pthread_mutex_unlock(thread_data->mutex);

    if (is_terminal_task_id(task_id)) {
      break;
    }

    // Initialize task-specific values.
    bool is_row = thread_data->is_row;
    algorithm_data.field_length = is_row ? field->width : field->height;
    algorithm_data.blocks = (is_row ? field->rows : field->columns) + task_id;

    algorithm_data.skip_threshold = thread_data->skip_threshold;

    algorithm_data.updated = false;
    algorithm_data.skipped = false;

    if (!algorithm_data.blocks->solved) {
      // These define how to navigate `raw_ptr`.
      int grid_offset = is_row ? 1 : field->height;
      int grid_step = is_row ? field->height : 1;

      // Get the initial offset.
      char *grid = field->flat_grid + task_id * grid_offset;

      for (int i = 0; i < algorithm_data.field_length; i++) {
        algorithm_data.field[i] = grid[i * grid_step];
      }

      // Is it faster to run `method` without copying the original data?
      // Maybe a little, but it's annoying, so "no".
      thread_data->method(&algorithm_data);

      for (int i = 0; i < algorithm_data.field_length; i++) {
        grid[i * grid_step] = algorithm_data.field[i];
      }
    }

    pthread_mutex_lock(thread_data->mutex);

    thread_data->field_updated =
        thread_data->field_updated || algorithm_data.updated;
    thread_data->rows_skipped =
        thread_data->rows_skipped || algorithm_data.skipped;
    // TODO copy updated/skipped over.
  }

  pthread_mutex_unlock(thread_data->mutex);
  return NULL;
}

void run_threads(thread_data_t *thread_data) {
  pthread_cond_broadcast(thread_data->waitForJobs);
  do {
    pthread_cond_wait(thread_data->waitForResults, thread_data->mutex);
  } while (thread_data->num_threads_idle != thread_data->num_threads);
}

void run_threads_once(thread_data_t *thread_data) {
  // Run horizontally.
  thread_data->field_updated = false;
  thread_data->rows_skipped = false;

  thread_data->next_task_id = 0;
  thread_data->is_row = true;
  run_threads(thread_data);

  // Run vertically.
  thread_data->next_task_id = 0;
  thread_data->is_row = false;
  run_threads(thread_data);
}

void run_threads_until_fixpoint(thread_data_t *thread_data) {
  printf("[%s] Solving ...\n", __func__);

  thread_data->skip_threshold = 10;

  field_t *field = thread_data->field;
  do {
    run_threads_once(thread_data);
    print_field(field);

    int old = thread_data->skip_threshold;
    if (thread_data->field_updated) {
      int min = INT_MAX;
      for (int i = 0; i < field->height; i++) {
        min = MIN(min, field->rows[i].num_combinations);
      }
      for (int i = 0; i < field->width; i++) {
        min = MIN(min, field->columns[i].num_combinations);
      }

      int new = min * 64;
      thread_data->skip_threshold = new < min ? INT_MAX : new;
    } else {
      int new = old * 64;
      thread_data->skip_threshold = new < old ? INT_MAX : new;
    }

    /* if (old != thread_data->skip_threshold) { */
    /*   printf("\n[%s] Setting skip threshold to %zu\n", __func__, */
    /*          thread_data->skip_threshold); */
    /* } */

    thread_data->num_iterations++;
    printf("[%s] Iteration %d\r", __func__, thread_data->num_iterations);
    fflush(stdout);
  } while (thread_data->field_updated || thread_data->rows_skipped);
  printf("\n");
}

void wait_for_all_idle(thread_data_t *thread_data) {
  // Wait for threads to wait for me.
  printf("[%s] Synchronizing threads...\n", __func__);
  while (thread_data->num_threads_idle != thread_data->num_threads) {
    pthread_cond_wait(thread_data->waitForResults, thread_data->mutex);
  }
  printf("[%s] Done\n", __func__);
}

void solve_multi(field_t *field, int num_threads) {
  // Initialize thread coordination resources.
  pthread_mutex_t mutex;
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_t waitForJobs;
  pthread_cond_init(&waitForJobs, NULL);
  pthread_cond_t waitForResults;
  pthread_cond_init(&waitForResults, NULL);

  thread_data_t thread_data = {
      .mutex = &mutex,
      .waitForJobs = &waitForJobs,
      .waitForResults = &waitForResults,
      .field = field,
      .num_threads = num_threads,
      .num_threads_idle = 0,
      .num_iterations = 0,
  };

  // Initialize threads.
  pthread_mutex_lock(&mutex);

  printf("[%s] Launching threads\n", __func__);
  pthread_t threads[num_threads];
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(threads + i, NULL, solve_single, (void *)&thread_data)) {
      fprintf(stderr, "[%s] Error creating thread\n", __func__);
      return;
    }
  }

  wait_for_all_idle(&thread_data);

  // First try intelligibly...
  printf("[%s] Using combinatorical approach\n", __func__);
  thread_data.method = combinatorical;
  run_threads_until_fixpoint(&thread_data);
  // ... then hit it with brute force.
  printf("[%s] Using brute force\n", __func__);
  thread_data.method = brute_force;
  run_threads_until_fixpoint(&thread_data);

  // Notify end of tasks.
  thread_data.next_task_id = -1;
  pthread_cond_broadcast(&waitForJobs);
  pthread_mutex_unlock(&mutex);

  // Wait for threads to terminate.
  printf("[%s] Joining threads...\n", __func__);
  for (int i = 0; i < num_threads; i++) {
    if (pthread_join(threads[i], NULL)) {
      fprintf(stderr, "[%s] Error joining thread\n", __func__);
    }
  }
  printf("[%s] Done\n", __func__);
}

int main(int argc, char *argv[]) {
  int num_threads = 2;
  /* bool print_to_window = false; */
  bool print_to_console = false;
  char *file_name = NULL;

  if (argc < 2) {
    fprintf(stderr, "[%s] Usage: ./japCrosswords [-c] [-s] file.jc\n",
            __func__);
    exit(1);
  }

  for (int i = 1; i < argc; i++) {
    if (strncmp(argv[i], "-n", 2) == 0) {
      num_threads = argv[i][2] - '0';
    } else if (strncmp(argv[i], "-c", 2) == 0) {
      print_to_console = true;
      /* } else if (strncmp(argv[i], "-s", 2) == 0) { */
      /*   print_to_window = true; */
    } else {
      file_name = argv[i];
    }
  }

  field_t field;
  int res = init_field_from_file(file_name, &field);
  if (res != 0) {
    return res;
  }

  solve_multi(&field, num_threads);

  if (print_to_console) {
    print_field(&field);
  }

  /* if (print_to_window) { */
  /*   init_sdl(&field); */
  /*   draw_field(&field); */
  /*  */
  /*   int run = 1; */
  /*   while (run) { */
  /*     handle_events(&run); */
  /*     wait(200); */
  /*   } */
  /*  */
  /*   close_sdl(); */
  /* } */

  free_field(&field);
  return 0;
}
