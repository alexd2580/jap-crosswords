#include <alloca.h>
#include <pthread.h>
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

  volatile int num_threads;
  volatile int threads_idle;
  volatile int next_task_id;
  volatile int iterations;

  // Solving function.
  void (*method)(struct algorithm_data const *const);

  // Skip combinatorical solving if more than this many possibilities.
  volatile long skip_threshold;
  volatile char is_row;

  volatile int field_updated;
  volatile int row_skipped;
};

void *solve_single(void *something) {
  struct thread_data *thread_data = (struct thread_data *)something;
  struct field *field = thread_data->field;

  size_t buffer_size = MAX(field->w, field->h);
  char *row = (char *)alloca(3 * buffer_size);

  // These define how to navigate `raw_ptr`.
  int raw_offset;
  int raw_step;
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

  int task_id;
  while (1) {
    pthread_mutex_lock(thread_data->mutex);

    // Wait for jobs when the current batch is done.
    while (thread_data->next_task_id >=
           (thread_data->is_row ? field->h : field->w)) {
      thread_data->threads_idle++;
      pthread_cond_signal(thread_data->waitForResults);
      pthread_cond_wait(thread_data->waitForJobs, thread_data->mutex);
      thread_data->threads_idle--;
    }

    // Now `next_task_id` is a valid id.
    task_id = thread_data->next_task_id;
    thread_data->next_task_id += thread_data->next_task_id >= 0;

    pthread_mutex_unlock(thread_data->mutex);

    if (task_id == -1) {
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

      for (int i = 0; i < algorithm_data.unit_len; i++) {
        row[i] = raw_ptr[i * raw_step];
      }

      // Is it faster to run `method` without copying the original data?
      thread_data->method(&algorithm_data);

      for (int i = 0; i < algorithm_data.unit_len; i++) {
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

void run_threads_until_fixpoint(struct thread_data *thread_data) {
  printf("[%s] Solving ...\n", __func__);
  do {
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

    // If rows were skipped but nothing else changed, then increase the
    // skip_threshold.
    printf("\nchanged %d skipped %d\n", thread_data->field_updated, thread_data->row_skipped);
    if (thread_data->field_updated == 0 && thread_data->row_skipped != 0) {
      // TODO what's the best parameter here?
      thread_data->skip_threshold *= 20;
      printf("\n[%s] Increasing skip threshold to %zu\n", __func__, thread_data->skip_threshold);
    }

    thread_data->iterations++;
    printf("[%s] Iteration %d\r", __func__, thread_data->iterations);
    fflush(stdout);
  } while (thread_data->field_updated != 0 || thread_data->row_skipped != 0);
  printf("\n");
}

void wait_for_all_idle(struct thread_data* thread_data) {
  // Wait for threads to wait for me.
  printf("[%s] Synchronizing threads...\n", __func__);
  while (thread_data->threads_idle != thread_data->num_threads) {
    pthread_cond_wait(thread_data->waitForResults, thread_data->mutex);
  }
  printf("[%s] Done\n", __func__);
}

void solve_multi(struct field *field, int num_threads) {
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
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(threads + i, NULL, solve_single, (void *)&thread_data)) {
      fprintf(stderr, "Error creating thread\n");
      return;
    }
  }

  wait_for_all_idle(&thread_data);

  // First try intelligibly...
  /* printf("[%s] Using combinatorical approach\n", __func__) */
  /* thread_data.method = combinatorical; */
  /* run_threads_until_fixpoint(&thread_data); */
  // ... then hit it with brute force.
  printf("[%s] Using brute force\n", __func__);
  thread_data.method = brute_force;
  run_threads_until_fixpoint(&thread_data);

  // Notify end of tasks.
  thread_data.next_task_id = -1;
  // Not sure this ends well. Have all threads entered waiting state here (ready
  // for broadcast)?
  pthread_mutex_unlock(&mutex);

  wait_for_all_idle(&thread_data);

  pthread_cond_broadcast(&waitForJobs);

  // Wait for threads to terminate.
  printf("[%s] Joining threads...\n", __func__);
  for (int i = 0; i < num_threads; i++) {
    if (pthread_join(threads[i], NULL)) {
      fprintf(stderr, "Error joining thread\n");
    }
  }
  printf("[%s] Done\n", __func__);
}

int main(int argc, char *argv[]) {
  char *file_name = NULL;
  int print_to_window = 0;
  int print_to_console = 0;

  if (argc < 2) {
    printf("[MAIN] Usage: ./japCrosswords [-c] [-s] file.jc\n");
    exit(1);
  }

  for (int i = 1; i < argc; i++) {
    if (strncmp(argv[i], "-c", 2) == 0) {
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

  solve_multi(&field, 1);

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
