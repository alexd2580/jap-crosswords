#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<alloca.h>

#include "field.h"
#include "macros.h"

int is_digit(char c) { return c >= '0' && c <= '9'; }

void read_num_list(struct num_list *list, char *line, size_t field_length) {
  size_t buffer[100];

  list->length = 0;

  size_t accum = 0;
  int cont = 1;
  while (cont) {
    char c = *line;
    if (is_digit(c)) {
      accum = accum * 10 + (size_t)c - '0';
    } else {
      if (accum != 0) {
        buffer[list->length] = accum;
        accum = 0;
        list->length++;
      }

      if (c == '\0' || c == '\n') {
        cont = 0;
      }
    }

    line++;
  }

  list->nums = (size_t *)malloc(3 * list->length * sizeof(size_t));
  memcpy(list->nums, buffer, list->length * sizeof(size_t));
  list->combinations = 1;
  list->min_starts = list->nums + list->length;
  list->max_starts = list->nums + 2 * list->length;

  for (size_t n = 0; n < list->length; n++) {
    list->min_starts[n] = 0;
    list->max_starts[n] = field_length - list->nums[n];
  }

  list->solved = UNSOLVED;
}

void allocate_field(struct field *field) {
  // Initialize center space.
  field->grid = (char **)malloc(field->w * sizeof(char *));
  field->grid_flat = (char *)malloc(field->w * field->h);
  memset(field->grid_flat, '?', field->w * field->h);

  // Initialize convenience pointers.
  for (size_t i = 0; i < field->w; i++) {
    field->grid[i] = field->grid_flat + i * field->h;
  }

  field->sides = (struct num_list *)malloc((field->w + field->h) *
                                           sizeof(struct num_list));
  field->columns = field->sides;
  field->rows = field->sides + field->w;

  for (size_t i = 0; i < field->w + field->h; i++) {
    field->sides[i].length = 0;
    field->sides[i].nums = NULL;
  }
}

int init_from_file(char const *file_name, struct field *field) {
  FILE *f = fopen(file_name, "r");
  if (f == NULL) {
    fprintf(stderr, "[%s] Could not open file\n", __func__);
    return 1;
  }

  char buffer[100];
  fgets(buffer, 100, f);
  sscanf(buffer, "%zu", &field->h);
  fgets(buffer, 100, f);
  sscanf(buffer, "%zu", &field->w);

  allocate_field(field);

  // Ingest rows.
  for (size_t i = 0; i < field->h; i++) {
    fgets(buffer, 100, f);
    read_num_list(field->rows + i, buffer, field->w);
  }

  // Ingest columns.
  for (size_t i = 0; i < field->w; i++) {
    fgets(buffer, 100, f);
    read_num_list(field->columns + i, buffer, field->h);
  }

  fclose(f);
  return 0;
}

void free_field(struct field *field) {
  free(field->grid);
  free(field->grid_flat);

  for (size_t i = 0; i < field->w + field->h; i++) {
    if (field->sides[i].nums != NULL) {
      free(field->sides[i].nums);
    }
  }

  free(field->sides);
}

void print_field(struct field *field) {
  for (size_t y = 0; y < field->h; y++, putchar('\n')) {
    for (size_t x = 0; x < field->w; x++) {
      putchar(field->grid[x][y]);
    }
  }
}
