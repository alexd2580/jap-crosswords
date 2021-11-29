#include <alloca.h>
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algorithms.h"
#include "field.h"
#include "macros.h"

// Check if a `len` sized block can be put at position `pos`.
__attribute__((pure)) bool can_place(int pos, int len,
                                     algorithm_data_t const *const d) {
  // Check that the left side is available.
  if (pos > 0 && d->field[pos - 1] == 'X') {
    return false;
  }

  for (int i = 0; i < len; i++) {
    if (d->field[pos + i] == ' ') {
      return false;
    }
  }

  // Check that the right side is available.
  if (pos + len < d->field_length && d->field[pos + len] == 'X') {
    return false;
  }

  return true;
}

void print_debug_blocks(algorithm_data_t const *const d) {
  fprintf(stderr, "Blocks:\n");
  for (int i = 0; i < d->blocks->size; i++) {
    fprintf(stderr, "%d [%d .. %d]\n", d->blocks->blocks[i],
            d->blocks->min_starts[i],
            d->blocks->max_starts[i] + d->blocks->blocks[i] - 1);
  }

  fprintf(stderr, "Field:\n");
  for (int i = 0; i < d->field_length; i++) {
    fputc(d->field[i], stderr);
  }
  fputc('\n', stderr);
}

// Merge the suggestion with the field data.
// Check that we don't merge unmergable stuff.
void merge_suggest_into_field(algorithm_data_t *d) {
  blocks_t *blocks = d->blocks;
  char *suggest = (char *)d->suggest;
  char *field = (char *)d->field;
  blocks->solved = SOLVED;

  for (int i = 0; i < d->field_length; i++) {
    if (suggest[i] == '?') {
      // When there is a field that i am not sure about.
      blocks->solved = UNSOLVED;
    } else if (suggest[i] == ' ') {
      // When we deduced a field to be a space.
      if (field[i] == '?' || field[i] == ' ') {
        d->updated = d->updated || field[i] != ' ';
        field[i] = ' ';
      } else {
        fprintf(stderr, "[%s] Cannot merge. Suggested space where field=%c.\n",
                __func__, field[i]);
        blocks->solved = UNSOLVED;
        return;
      }
    } else {
      // When we deduced the field to be filled.
      if (field[i] == '?' || field[i] == 'X') {
        d->updated = d->updated || field[i] != 'X';
        field[i] = 'X';
      } else {
        fprintf(stderr,
                "[%s] At pos %d: Cannot merge. Suggested block %d where "
                "field=%c.\n",
                __func__, i, suggest[i], field[i]);
        blocks->solved = UNSOLVED;
        return;
      }
    }
  }
}

char merge_prepare_into_suggest(algorithm_data_t const *const d) {
  int block = 0;
  int block_length = 0;
  blocks_t *blocks = d->blocks;

  // Move over the entire field, validating block placements.
  for (int i = 0; i < d->field_length; i++) {
    if (d->prepare[i] == (char)block) {
      block_length++;

      // If it's one of these, the configuration is invalid:
      // - we try to place something where there's a ' '
      // - the current block is invalid (nonexisting index)
      // - the length of the current block is larger than expected
      if (d->field[i] == ' ' || block >= blocks->size ||
          block_length > blocks->blocks[block]) {
        return INVALID;
      }
    } else if (d->prepare[i] == SPACE) {
      // Can't put an EMPTY where a 'X' must be.
      if (d->field[i] == 'X') {
        return INVALID;
      }

      // If this is the first space after a block.
      if (block_length != 0) {
        // TODO This will never trigger, right?
        if (block >= blocks->size || block_length < blocks->blocks[block]) {
          assert(0);
          return INVALID;
        }

        // Reset block counters.
        block_length = 0;
        block++;
      }
    } else {
      // Found an unexpected marker in `prepare`.
      return INVALID;
    }
  }

  // Field ends in block that is too short.
  if (block == blocks->size - 1 && block_length != blocks->blocks[block]) {
    // TODO can this actually happen?
    assert(0);
    return INVALID;
  }

  // Some blocks are straightout missing.
  if (block < blocks->size - 1) {
    // TODO can this actually happen?
    assert(0);
    return INVALID;
  }

  // Merge this (probably valid) configuration into `suggest`.
  char futile = FUTILE;
  char prep;

  for (int i = 0; i < d->field_length; i++) {
    prep = d->prepare[i];
    if (d->suggest[i] == 0) {
      d->suggest[i] = prep == SPACE ? ' ' : 'X';
    } else if (d->suggest[i] == 'X' && prep != SPACE) {
      d->suggest[i] = 'X';
    } else if (d->suggest[i] == ' ' && prep == SPACE) {
      d->suggest[i] = ' ';
    } else {
      d->suggest[i] = '?';
    }

    // If this run still contains new information then it is not yet futile.
    if (d->suggest[i] != '?' && d->field[i] == '?') {
      futile = VALID;
    }
  }

  return futile;
}

// Move block between `start` and `end`, recursing for next blocks.
// All valid solutions are merged into `suggest`.
char brute_force_(algorithm_data_t *d, int block, int start, int end) {
  blocks_t *blocks = d->blocks;

  // Terminate recursion by merging `prepare` into `suggest`.
  if (block >= blocks->size) {
    return merge_prepare_into_suggest(d);
  }

  int len = blocks->blocks[block];
  int min_start = blocks->min_starts[block];
  int max_start = blocks->max_starts[block];

  // `rest_len` gives the minimum right padding if all right blocks are moved
  // to the rightmost position.
  int rest_len = 0;
  for (int b = block + 1; b < blocks->size; b++) {
    rest_len += blocks->blocks[b] + 1;
  }

  // Write the block into `prepare` at `pos`.
  int pos = MAX(min_start, start);
  memset(d->prepare + pos, (char)block, (size_t)len - 1);

  // Iterate while the starting index is valid.
  while (pos < MIN(max_start + len, end)) {
    // If the end index is OOB then break.
    if (pos + len + rest_len > end) {
      break;
    }

    // Put last block here, because now we know that the last block is at a
    // valid position.
    d->prepare[pos + len - 1] = (char)block;

    if (can_place(pos, len, d)) {
      // Try recursively.
      if (brute_force_(d, block + 1, pos + len + 1, end) == FUTILE) {
        return FUTILE;
      }
    }

    // Remove first.
    d->prepare[pos] = SPACE;
    pos++;
  }

  // Entirely remove the block from `prepare`.
  memset(d->prepare + pos, SPACE, (size_t)len - 1);

  return INVALID;
}

void brute_force(algorithm_data_t *d) {
  int num_combinations = 1;
  // Multiply the number of different positions of every block.
  for (int i = 0; i < d->blocks->size; i++) {
    int next = num_combinations *
               (1 + d->blocks->max_starts[i] - d->blocks->min_starts[i]);
    if (next < num_combinations) {
      num_combinations = INT_MAX;
      break;
    }
    num_combinations = next;
  }
  d->blocks->num_combinations = num_combinations;

  // Solve trivially.
  if (num_combinations == 1) {
    d->blocks->solved = SOLVED;

    // TODO What does this do?
    for (int i = 0; i < d->field_length; i++) {
      // TODO '?' are spaces?
      d->field[i] = d->field[i] == 'X' ? 'X' : ' ';
    }
    return;
  }

  // If too many then skip.
  if (num_combinations > d->skip_threshold) {
    d->skipped = true;
    return;
  }

  // Clear buffers.
  // `prepare` uses block indices and `SPACE`.
  memset(d->prepare, SPACE, (size_t)d->field_length);
  // `suggest` uses symbols + 0, which means undefined.
  memset(d->suggest, 0, (size_t)d->field_length);

  // If brute force is not futile, merge the produced result.
  // TODO What about invalid?
  if (brute_force_(d, 0, 0, d->field_length) != FUTILE) {
    merge_suggest_into_field(d);
  }
}

void fill_min_max_overlaps(algorithm_data_t *const d) {
  char empty[d->field_length];
  memset(empty, ' ', (size_t)d->field_length);
  char filled[d->field_length];
  memset(filled, 'X', (size_t)d->field_length);

  blocks_t *blocks = d->blocks;
  char *field = d->field;

  int prev_max_end = 0;

  // For each block...
  for (int block = 0; block < blocks->size; block++) {
    int len = blocks->blocks[block];
    int min_start = blocks->min_starts[block];
    int max_start = blocks->max_starts[block];
    int min_end = min_start + len;
    int max_end = max_start + len;

    // Empty fields in front of block.
    if (prev_max_end < min_start) {
      size_t overlap_size = (size_t)(min_start - prev_max_end);
      char *begin = field + prev_max_end;
      d->updated = d->updated || memcmp(begin, empty, overlap_size) != 0;
      memset(begin, ' ', overlap_size);
    }

    // Overlapping min/max block areas.
    if (max_start < min_end) {
      size_t overlap_size = (size_t)(min_end - max_start);
      char *begin = field + max_start;
      d->updated = d->updated || memcmp(begin, filled, overlap_size) != 0;
      memset(begin, 'X', overlap_size);
    }

    prev_max_end = max_end;
  }

  // Check trailing empty area.
  if (prev_max_end < d->field_length) {
    size_t overlap_size = (size_t)(d->field_length - prev_max_end);
    char *begin = field + prev_max_end;
    d->updated = d->updated || memcmp(begin, empty, overlap_size) != 0;
    memset(begin, ' ', overlap_size);
  }
}

void compute_block_position_bounds(algorithm_data_t const *const d) {
  blocks_t *const blocks = d->blocks;

  // TODO This doesn't check that the combination is really valid, there may be
  // a `valid` config where more than the given amount of blocks are fit into
  // the line.

  // Try to fit all the blocks from the left.
  // For the 0th iteration we start at -1 to simplify loop logic.
  int start = -1;
  for (int block = 0; block < blocks->size; block++) {
    // Add one block of spacing.
    start++;

    int len = blocks->blocks[block];

    // Skip to the first in-bounds solution.
    start = MAX(start, blocks->min_starts[block]);

    // Find the leftmost fit.
    // TODO in theory this can segfault if there are no valid solutions.
    while (!can_place(start, len, d)) {
      start++;
    }

    // Now `start` is the leftmost valid position of `block`.
    // Move min start position to the new min.
    blocks->min_starts[block] = MAX(blocks->min_starts[block], start);

    // Move to end of block.
    start += len;
  }

  // Try to fit all the blocks from the right.
  // We explicitly increase the length by 1 to simplify loop logic.
  int end = d->field_length + 1;
  for (int block = blocks->size - 1; block > -1; block--) {
    // Add one block of spacing.
    end--;

    int len = blocks->blocks[block];

    // Skip to the first in-bounds solution.
    end = MIN(end, blocks->max_starts[block] + len);

    // Find the rightmost fit.
    // TODO in theory this can segfault if there are no valid solutions.
    while (!can_place(end - len, len, d)) {
      end--;
    }

    // Now `end` is the rightmost valid end position of `block`.
    // Move max start position to the new max.
    blocks->max_starts[block] = MIN(blocks->max_starts[block], end - len);

    // Move to start of block.
    end -= len;
  }
}

__attribute__((pure)) bool block_fits_sequence(algorithm_data_t const *d,
                                               int block, int seq_start,
                                               int seq_len) {
  int block_len = d->blocks->blocks[block];

  bool left = d->blocks->min_starts[block] <= seq_start;
  bool length = seq_len <= block_len;
  bool right = seq_start + seq_len <= d->blocks->max_starts[block] + block_len;

  if (!left || !length || !right) {
    return false;
  }

  int check_start = seq_start + seq_len - block_len;
  int check_end = seq_start;
  for (int pos = check_start; pos <= check_end; pos++) {
    if (can_place(pos, block_len, d)) {
      return true;
    }
  }
  return false;
}

void match_sequences_with_blocks(algorithm_data_t *const d) {
  blocks_t *blocks = d->blocks;
  char *const field = (char *)d->field;

  // Locate sequences.
  int sequence_count = 0;
  int *sequence_starts = (int *)alloca((size_t)d->field_length * sizeof(int));
  int *sequence_lengths = (int *)alloca((size_t)d->field_length * sizeof(int));

  int pos = 0;
  while (pos < d->field_length) {
    while (field[pos] != 'X' && pos < d->field_length) {
      pos++;
    }

    int start = pos;

    while (field[pos] == 'X' && pos < d->field_length) {
      pos++;
    }

    if (pos > start) {
      sequence_starts[sequence_count] = start;
      sequence_lengths[sequence_count] = pos - start;
      sequence_count++;
    }
  }

  // TODO this might be pointlessly complex.

  // Match sequences with blocks. A sequence matches if it lies within the
  // comouted bounds of a block and is equally long or smaller than the block.
  for (int sequence = 0; sequence < sequence_count; sequence++) {
    int first = blocks->size;
    int last = -1;

    int seq_start = sequence_starts[sequence];
    int seq_len = sequence_lengths[sequence];

    // Scan blocks for first match.
    for (int block = 0; block < blocks->size; block++) {
      if (block_fits_sequence(d, block, seq_start, seq_len)) {
        first = block;
        break;
      }
    }

    // Scan blocks for last match (first from the right).
    for (int block = blocks->size - 1; block > -1; block--) {
      if (block_fits_sequence(d, block, seq_start, seq_len)) {
        last = block;
        break;
      }
    }

    // There were matches before, so we don't know which is the right block.
    if (first < last) {
      // Skip this sequence for now.
      continue;
    }

    // Found a single matching block.
    else if (first == last) {
      // Adjust the bounds of the block.
      int len = blocks->blocks[first];

      int min_start = blocks->min_starts[first];
      int max_start = blocks->max_starts[first];

      // We must check the blocks before subtracting, it might underflow.
      int new_min = MAX(min_start, seq_start + seq_len - len);
      int new_max = MIN(max_start, seq_start);

      if (new_max < new_min) {
        // If the new max is below the new min, then solving is impossible.
        fprintf(stderr,
                "[%s] Invalid bounds for block %d: [%d .. %d] => [%d .. %d]\n",
                __func__, first, min_start, max_start, new_min, new_max);
        print_debug_blocks(d);
        exit(1);
      }

      // Update the block bounds.
      d->updated = d->updated || min_start != new_min || max_start != new_max;
      blocks->min_starts[first] = new_min;
      blocks->max_starts[first] = new_max;
    }

    // No block matched the sequence.
    else if (first > last) {
      fprintf(stderr, "[%s] Unmatched sequence [%d .. %d]\n", __func__,
              seq_start, seq_start + seq_len - 1);
      print_debug_blocks(d);
      exit(1);
    }
  }
}

void combinatorical(algorithm_data_t *const d) {
  /* int *n = d->blocks->blocks; */
  /* if (d->blocks->size == 5 && n[0] == 18 && n[1] == 3 && n[2] == 4 && */
  /*     n[3] == 4 && n[4] == 6) { */
  /*   printf("\n\nBEFORE"); */
  /*   print_debug_blocks(d); */
  /*   printf("\n"); */
  /* } */

  compute_block_position_bounds(d);
  fill_min_max_overlaps(d);
  match_sequences_with_blocks(d);

  /* if (d->blocks->size == 5 && n[0] == 18 && n[1] == 3 && n[2] == 4 && */
  /*     n[3] == 4 && n[4] == 6) { */
  /*   print_debug_blocks(d); */
  /*   printf("AFTER\n\n"); */
  /* } */
}
