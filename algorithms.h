#ifndef ALGORITHMS_H_
#define ALGORITHMS_H_

#include "field.h"

struct algorithm_data {
  struct num_list *numbers;

  size_t unit_len;
  size_t skip_threshold;

  char *const row;
  char *const prepare;
  char *const suggest;

  volatile int *field_updated;
  volatile int *row_skipped;
};

void combinatorical(struct algorithm_data const *const);
void brute_force(struct algorithm_data const *const);

#endif
