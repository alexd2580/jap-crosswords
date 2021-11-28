#include <alloca.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algorithms.h"
#include "field.h"
#include "graphics.h"
#include "macros.h"

/** MAIN CODE **/

struct thread_data {
  pthread_mutex_t *mutex;
  pthread_cond_t *waitForJobs;
  pthread_cond_t *waitForResults;

  struct field *field;

  volatile size_t num_threads;
  volatile size_t threads_idle;
  volatile size_t next_task_id;
  volatile size_t iterations;

  // Solving function.
  void (*method)(struct algorithm_data const *const);

  // Skip combinatorical solving if more than this many possibilities.
  volatile size_t skip_threshold;
  volatile char is_row;

  volatile int field_updated;
  volatile int row_skipped;
};

char is_terminal_task_id(size_t task_id) { return task_id == SIZE_MAX; }

char is_invalid_task_id(struct thread_data *thread_data) {
  size_t const field_length =
      thread_data->is_row ? thread_data->field->h : thread_data->field->w;
  return thread_data->next_task_id >= field_length &&
         !is_terminal_task_id(thread_data->next_task_id);
}

void *solve_single(void *something) {
  struct thread_data *thread_data = (struct thread_data *)something;
  struct field *field = thread_data->field;

  size_t buffer_size = MAX(field->w, field->h);
  char *row = (char *)alloca(3 * buffer_size);

  // These define how to navigate `raw_ptr`.
  size_t raw_offset;
  size_t raw_step;
  char *raw_ptr;

  struct algorithm_data algorithm_data = {
      .row = row,
      .prepare = row + buffer_size,
      .suggest = row + 2 * buffer_size,
      .field_updated = &thread_data->field_updated,
      .row_skipped = &thread_data->row_skipped,
  };

  printf("[%s] Synchronizing\n", __func__);

  // Wait for start signal.
  pthread_mutex_lock(thread_data->mutex);
  printf("[%s] Reporting idle, waiting for jobs\n", __func__);
  thread_data->threads_idle++;
  pthread_cond_signal(thread_data->waitForResults);
  pthread_cond_wait(thread_data->waitForJobs, thread_data->mutex);
  printf("[%s] Job received\n", __func__);
  thread_data->threads_idle--;
  pthread_mutex_unlock(thread_data->mutex);

  size_t task_id;
  while (1) {
    pthread_mutex_lock(thread_data->mutex);

    // Wait for jobs when the current batch is done.
    while (is_invalid_task_id(thread_data)) {
      thread_data->threads_idle++;
      pthread_cond_signal(thread_data->waitForResults);
      pthread_cond_wait(thread_data->waitForJobs, thread_data->mutex);
      thread_data->threads_idle--;
    }

    // Now `next_task_id` is a valid id.
    task_id = thread_data->next_task_id;
    thread_data->next_task_id += thread_data->next_task_id != SIZE_MAX;

    pthread_mutex_unlock(thread_data->mutex);

    if (is_terminal_task_id(task_id)) {
      break;
    }

    algorithm_data.numbers =
        (thread_data->is_row ? field->rows : field->columns) + task_id;
    algorithm_data.unit_len = thread_data->is_row ? field->w : field->h;
    algorithm_data.skip_threshold = thread_data->skip_threshold;

    if (algorithm_data.numbers->solved == UNSOLVED) {
      raw_offset = thread_data->is_row ? 1 : field->h;
      raw_step = thread_data->is_row ? field->h : 1;

      // Get the initial offset
      raw_ptr = field->grid_flat + task_id * raw_offset;

      for (size_t i = 0; i < algorithm_data.unit_len; i++) {
        row[i] = raw_ptr[i * raw_step];
      }

      // Is it faster to run `method` without copying the original data?
      thread_data->method(&algorithm_data);

      for (size_t i = 0; i < algorithm_data.unit_len; i++) {
        raw_ptr[i * raw_step] = row[i];
      }
    }
  }

  return NULL;
}

void run_threads(struct thread_data *thread_data) {
  pthread_cond_broadcast(thread_data->waitForJobs);
  do {
    pthread_cond_wait(thread_data->waitForResults, thread_data->mutex);
  } while (thread_data->threads_idle != thread_data->num_threads);
}

void run_threads_once(struct thread_data* thread_data) {
    // Run horizontally.
    thread_data->field_updated = 0;
    thread_data->row_skipped = 0;

    thread_data->next_task_id = 0;
    thread_data->is_row = 1;
    run_threads(thread_data);

    // Run vertically.
    thread_data->next_task_id = 0;
    thread_data->is_row = 0;
    run_threads(thread_data);
}

void run_threads_until_fixpoint(struct thread_data *thread_data) {
  printf("[%s] Solving ...\n", __func__);

  char field_updated;
  char rows_skipped;

  do {
    run_threads_once(thread_data);

    size_t old = thread_data->skip_threshold;
    if (thread_data->field_updated) {
      size_t min = SIZE_MAX;
      for (size_t i=0; i<thread_data->field->h; i++) {
        min = MIN(min, thread_data->field->rows[i].combinations);
      }
      for (size_t i=0; i<thread_data->field->w; i++) {
        min = MIN(min, thread_data->field->columns[i].combinations);
      }

      size_t new = min * 64;
      thread_data->skip_threshold = new < min ? SIZE_MAX : new;
    } else {
      size_t new = old * 64;
      thread_data->skip_threshold = new < old ? SIZE_MAX : new;
    }

    /* if (old != thread_data->skip_threshold) { */
    /*   printf("\n[%s] Setting skip threshold to %zu\n", __func__, */
    /*          thread_data->skip_threshold); */
    /* } */

    thread_data->iterations++;
    printf("[%s] Iteration %zu\r", __func__, thread_data->iterations);
    fflush(stdout);
  } while (thread_data->field_updated != 0 || thread_data->row_skipped != 0);
  printf("\n");
}

void wait_for_all_idle(struct thread_data *thread_data) {
  // Wait for threads to wait for me.
  printf("[%s] Synchronizing threads...\n", __func__);
  while (thread_data->threads_idle != thread_data->num_threads) {
    pthread_cond_wait(thread_data->waitForResults, thread_data->mutex);
  }
  printf("[%s] Done\n", __func__);
}

void solve_multi(struct field *field, size_t num_threads) {
  // Initialize thread coordination resources.
  pthread_mutex_t mutex;
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_t waitForJobs;
  pthread_cond_init(&waitForJobs, NULL);
  pthread_cond_t waitForResults;
  pthread_cond_init(&waitForResults, NULL);

  struct thread_data thread_data = {
      .mutex = &mutex,
      .waitForJobs = &waitForJobs,
      .waitForResults = &waitForResults,
      .field = field,
      .num_threads = num_threads,
      .threads_idle = 0,
      .iterations = 0,
      .skip_threshold = 10,
  };

  // Initialize threads.
  pthread_mutex_lock(&mutex);

  printf("[%s] Launching threads\n", __func__);
  pthread_t threads[num_threads];
  for (size_t i = 0; i < num_threads; i++) {
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
  /* run_threads_once(&thread_data); */
  /* run_threads_until_fixpoint(&thread_data); */

  // Notify end of tasks.
  thread_data.next_task_id = SIZE_MAX;
  // Not sure this ends well. Have all threads entered waiting state here (ready
  // for broadcast)?
  pthread_mutex_unlock(&mutex);

  wait_for_all_idle(&thread_data);

  pthread_cond_broadcast(&waitForJobs);

  // Wait for threads to terminate.
  printf("[%s] Joining threads...\n", __func__);
  for (size_t i = 0; i < num_threads; i++) {
    if (pthread_join(threads[i], NULL)) {
      fprintf(stderr, "[%s] Error joining thread\n", __func__);
    }
  }
  printf("[%s] Done\n", __func__);
}

int main(int argc, char *argv[]) {
  size_t num_threads = 2;
  char print_to_window = 0;
  char print_to_console = 0;
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
      print_to_console = 1;
    } else if (strncmp(argv[i], "-s", 2) == 0) {
      print_to_window = 1;
    } else {
      file_name = argv[i];
    }
  }

  struct field field;
  int res = init_from_file(file_name, &field);
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
