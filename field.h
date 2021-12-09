#ifndef FIELD_H_
#define FIELD_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

struct blocks {
  int size;
  int *blocks;
  int num_combinations;

  // These min and max values give the range of motion of a block.
  int *min_starts;
  int *max_starts;

  // Whether the entire line is solved (can be skipped in tasks).
  bool solved;
};
typedef struct blocks blocks_t;

struct field {
  int height;
  int width;

  blocks_t *rows;
  blocks_t *columns;

  // Data stored as list of columns.
  char *flat_grid;
};
typedef struct field field_t;

int init_field_from_file(char const *file_name, field_t *field);
void free_field(field_t *field);
void print_field(field_t *field);
void print_field_unicode_with_padding(field_t *field, int padding_h, int padding_w);
void print_field_unicode(field_t *field);

#endif
