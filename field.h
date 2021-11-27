#ifndef FIELD_H_
#define FIELD_H_

#include <stddef.h>
#include <stdio.h>

struct num_list {
  size_t length;
  int *nums;

  // These min and max values give the range of motion of a block.
  int *min_starts;
  int *max_starts;

  // Whether the entire line is solved (can be skipped in tasks).
  char solved;
};

struct field {
  size_t w;
  size_t h;

  // Data stored as list of columns.
  char *grid_flat;
  char **grid;

  struct num_list *sides;
  struct num_list *rows;
  struct num_list *columns;
};

int init_from_file(char const *file_name, struct field *field);
void free_field(struct field *field);
void print_field(struct field *field);

#endif
