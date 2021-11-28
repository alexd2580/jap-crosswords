#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algorithms.h"
#include "field.h"
#include "macros.h"

// Check if a `len` sized block can be put at position `pos`.
__attribute__((pure)) char can_place(size_t pos, size_t len,
                                     struct algorithm_data const *const d) {
  // Check that the left side is available.
  if (pos > 0 && d->row[pos - 1] == 'X') {
    return 0;
  }

  for (size_t i = 0; i < len; i++) {
    if (d->row[pos + i] == ' ') {
      return 0;
    }
  }

  // Check that the right side is available.
  if (pos + len < d->unit_len && d->row[pos + len] == 'X') {
    return 0;
  }

  return 1;
}

void print_debug_numbers(struct algorithm_data const *const d) {
  fprintf(stderr, "Numbers:\n");
  for (size_t i = 0; i < d->numbers->length; i++) {
    fprintf(stderr, "%zu [%zu .. %zu]\n", d->numbers->nums[i],
            d->numbers->min_starts[i],
            d->numbers->max_starts[i] + d->numbers->nums[i] - 1);
  }

  fprintf(stderr, "Row:\n");
  for (size_t i = 0; i < d->unit_len; i++) {
    fputc(d->row[i], stderr);
  }
  fputc('\n', stderr);
}

// Merge the suggestion with the row data.
// Check that we don't merge unmergable stuff.
void merge_suggest_into_row(struct algorithm_data const *const d) {
  struct num_list *numbers = d->numbers;
  char *suggest = (char *)d->suggest;
  char *row = (char *)d->row;
  numbers->solved = SOLVED;

  for (size_t i = 0; i < d->unit_len; i++) {
    if (suggest[i] == '?') {
      // When there is a field that i am not sure about.
      numbers->solved = UNSOLVED;
    } else if (suggest[i] == ' ') {
      // When we deduced a field to be a space.
      if (row[i] == '?' || row[i] == ' ') {
        *d->field_updated += row[i] != ' ';
        row[i] = ' ';
      } else {
        fprintf(stderr, "[%s] Cannot merge. Suggested space where row=%c.\n",
                __func__, row[i]);
        numbers->solved = UNSOLVED;
        return;
      }
    } else {
      // When we deduced the field to be filled.
      if (row[i] == '?' || row[i] == 'X') {
        *d->field_updated += row[i] != 'X';
        row[i] = 'X';
      } else {
        fprintf(stderr,
                "[%s] At pos %zu: Cannot merge. Suggested block %d where "
                "row=%c.\n",
                __func__, i, suggest[i], row[i]);
        numbers->solved = UNSOLVED;
        return;
      }
    }
  }
}

char merge_prepare_into_suggest(struct algorithm_data const *const d) {
  size_t block = 0;
  size_t block_length = 0;
  struct num_list *numbers = d->numbers;

  // Move over the entire row, validating block placements.
  for (size_t i = 0; i < d->unit_len; i++) {
    if (d->prepare[i] == (char)block) {
      block_length++;

      // If it's one of these, the configuration is invalid:
      // - we try to place something where there's a ' '
      // - the current block is invalid (nonexisting index)
      // - the length of the current block is larger than expected
      if (d->row[i] == ' ' || block >= numbers->length ||
          block_length > numbers->nums[block]) {
        return INVALID;
      }
    } else if (d->prepare[i] == SPACE) {
      // Can't put an EMPTY where a 'X' must be.
      if (d->row[i] == 'X') {
        return INVALID;
      }

      // If this is the first space after a block.
      if (block_length != 0) {
        // TODO This will never trigger, right?
        if (block >= numbers->length || block_length < numbers->nums[block]) {
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

  // Row ends in block that is too short.
  if (block == numbers->length - 1 && block_length != numbers->nums[block]) {
    // TODO can this actually happen?
    assert(0);
    return INVALID;
  }

  // Some blocks are straightout missing.
  if (block < numbers->length - 1) {
    // TODO can this actually happen?
    assert(0);
    return INVALID;
  }

  // Merge this (probably valid) configuration into `suggest`.
  char futile = FUTILE;
  char prep;

  for (size_t i = 0; i < d->unit_len; i++) {
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
    if (d->suggest[i] != '?' && d->row[i] == '?') {
      futile = VALID;
    }
  }

  return futile;
}

// Move block between `start` and `end`, recursing for next blocks.
// All valid solutions are merged into `suggest`.
char brute_force_(struct algorithm_data const *const d, size_t block,
                  size_t start, size_t end) {
  struct num_list *numbers = d->numbers;

  // Terminate recursion by merging `prepare` into `suggest`.
  if (block >= numbers->length) {
    return merge_prepare_into_suggest(d);
  }

  size_t len = numbers->nums[block];
  size_t min_start = numbers->min_starts[block];
  size_t max_start = numbers->max_starts[block];

  // `rest_len` gives the minimum right padding if all right blocks are moved
  // to the rightmost position.
  size_t rest_len = 0;
  for (size_t b = block + 1; b < numbers->length; b++) {
    rest_len += numbers->nums[b] + 1;
  }

  // Write the block into `prepare` at `pos`.
  size_t pos = MAX(min_start, start);
  memset(d->prepare + pos, (char)block, len - 1);

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
  memset(d->prepare + pos, SPACE, len - 1);

  return INVALID;
}

void brute_force(struct algorithm_data const *const d) {
  uint64_t num_combinations = 1;
  // Multiply the number of different positions of every block.
  for (size_t i = 0; i < d->numbers->length; i++) {
    size_t next = num_combinations *
                  (1 + d->numbers->max_starts[i] - d->numbers->min_starts[i]);
    if (next < num_combinations) {
      num_combinations = SIZE_MAX;
      printf("RIP\n\n\n\n\n");
      exit(1);
      break;
    }
    num_combinations = next;
  }
  d->numbers->combinations = num_combinations;

  // Solve trivially.
  if (num_combinations == 1) {
    d->numbers->solved = SOLVED;

    // TODO What does this do?
    for (size_t i = 0; i < d->unit_len; i++) {
      // TODO '?' are spaces?
      d->row[i] = d->row[i] == 'X' ? 'X' : ' ';
    }
    return;
  }

  // If too many then skip.
  if (num_combinations > d->skip_threshold) {
    (*d->row_skipped)++;
    return;
  }

  // Clear buffers.
  // `prepare` uses block indices and `SPACE`.
  memset(d->prepare, SPACE, d->unit_len);
  // `suggest` uses symbols + 0, which means undefined.
  memset(d->suggest, 0, d->unit_len);

  // If brute force is not futile, merge the produced result.
  // TODO What about invalid?
  if (brute_force_(d, 0, 0, d->unit_len) != FUTILE) {
    merge_suggest_into_row(d);
  }
}

void fill_min_max_overlaps(struct algorithm_data const *const d) {
  char spaces[d->unit_len];
  memset(spaces, ' ', d->unit_len);
  char blocks[d->unit_len];
  memset(blocks, 'X', d->unit_len);

  struct num_list *numbers = d->numbers;
  char *row = d->row;

  int field_updated = 0;

  // For each block...
  size_t len, prev_max_end, min_start, max_start, min_end, max_end;

  prev_max_end = 0;
  for (size_t block = 0; block < numbers->length; block++) {
    len = numbers->nums[block];
    min_start = numbers->min_starts[block];
    max_start = numbers->max_starts[block];
    min_end = min_start + len;
    max_end = max_start + len;

    // Empty fields in front of block.
    if (prev_max_end < min_start) {
      size_t overlap_size = min_start - prev_max_end;
      field_updated += memcmp(row + prev_max_end, spaces, overlap_size) != 0;
      memset(row + prev_max_end, ' ', overlap_size);
    }

    // Overlapping min/max block areas.
    if (max_start < min_end) {
      // Fill overlapping areas.
      size_t overlap_size = min_end - max_start;
      field_updated += memcmp(row + max_start, blocks, overlap_size) != 0;
      memset(row + max_start, 'X', overlap_size);
    }

    prev_max_end = max_end;
  }

  // Check trailing empty area.
  if (prev_max_end < d->unit_len) {
    size_t overlap_size = d->unit_len - prev_max_end;
    field_updated += memcmp(row + prev_max_end, spaces, overlap_size) != 0;
    memset(row + prev_max_end, ' ', overlap_size);
  }

  // Mark everything before the first block with a space.
  size_t min = numbers->min_starts[0];
  field_updated += memcmp(row, spaces, min) != 0;
  memset(row, ' ', min);

  // Mark everything after the last block with a space.
  size_t max_index = numbers->length - 1;
  size_t max = numbers->max_starts[max_index] + numbers->nums[max_index];
  field_updated += memcmp(row + max, spaces, d->unit_len - max) != 0;
  memset(row + max, ' ', d->unit_len - max);

  *(d->field_updated) += field_updated;
}

void compute_block_position_bounds(struct algorithm_data const *const d) {
  struct num_list *const numbers = d->numbers;

  // TODO This doesn't check tha the combination is really valid, there may be
  // a `valid` config where more than the given amount of blocks are fit into
  // the line.

  // Try to fit all the blocks from the left.
  size_t start = SIZE_MAX;
  for (size_t block = 0; block < numbers->length; block++) {
    // Add one block of spacing.
    // The first time we cause an integer overflow to get to 0!
    start++;

    size_t len = numbers->nums[block];

    // Skip to the first in-bounds solution.
    start = MAX(start, numbers->min_starts[block]);

    // Find the leftmost fit.
    while (!can_place(start, len, d)) {
      start++;
    }

    // Not enough space to fit rest of blocks.
    // TODO Remove this by fixing bugs.
    if (start + len > d->unit_len) {
      fprintf(stderr, "[%s] Trying to put block %zu outside field\n", __func__,
              block);
      print_debug_numbers(d);
      exit(1);
    }

    // Now `start` is the leftmost valid position of `block`.
    // Move min start position to the new min.
    numbers->min_starts[block] = MAX(numbers->min_starts[block], start);

    // Move to end of block.
    start += len;
  }

  // Try to fit all the blocks from the right.
  // We explicitly increase the length by 1 to simplify loop logic.
  // The for-condition relies on integer underflow!
  size_t end = d->unit_len + 1;
  for (size_t block = numbers->length - 1; block != SIZE_MAX; block--) {
    // Add one block of spacing.
    // The first time we remove the added `1`.
    end--;

    size_t len = numbers->nums[block];

    // Skip to the first in-bounds solution.
    end = MIN(end, numbers->max_starts[block] + len);

    // Find the rightmost fit.
    while (!can_place(end - len, len, d)) {
      end--;
    }

    // Not enough space to fit rest of blocks.
    // TODO Remove this by fixing bugs.
    if (end - len == SIZE_MAX) {
      fprintf(stderr, "[%s] Trying to put block %zu outside field\n", __func__,
              block);
      print_debug_numbers(d);
      exit(1);
    }

    // Now `end` is the rightmost valid end position of `block`.
    // Move max start position to the new max.
    numbers->max_starts[block] = MIN(numbers->max_starts[block], end - len);

    // Move to start of block.
    end -= len;
  }
}

void match_sequences_with_blocks(struct algorithm_data const *const d) {
  struct num_list *numbers = d->numbers;
  char *const row = (char *)d->row;

  // Try to match every sequence of 'X's within the row with a block.
  size_t start, block;
  for (size_t end = 0; end < d->unit_len; end++) {
    // Ignore spaces.
    if (row[end] != 'X') {
      continue;
    }

    // Locate extents of this block.
    start = end;
    do {
      end++;
    } while (row[end] == 'X' && end < d->unit_len);

    // Search matching block. Abort if multiple blocks match.
    block = SIZE_MAX;
    for (size_t b = 0; b < numbers->length; b++) {
      /* printf("%zu %zu %zu %zu\n", numbers->min_starts[b], start, pos,
       * numbers->max_starts[b] + numbers->nums[b]); */
      char left = numbers->min_starts[b] > start;
      char right = end > numbers->max_starts[b] + numbers->nums[b];

      if (left || right) {
        // Block doesn't match, skip.
        continue;
      }

      if (block == SIZE_MAX) {
        // Block is first to match sequence.
        block = b;
        continue;
      }

      // There were matches before, so we don't know which is the right block.
      // Skip this sequence for now.
      block = SIZE_MAX - 1;
    }

    if (block == SIZE_MAX - 1) {
      // Multiple matches, skip this sequence.
      continue;
    }

    if (block == SIZE_MAX) {
      // No block matched the sequence.
      fprintf(stderr, "[%s] Unmatched sequence [%zu .. %zu]\n", __func__, start,
              end - 1);
      print_debug_numbers(d);
      exit(1);
    }

    // Found a single matching block.
    // Adjust the bounds of the block sine we know it must match this sequence.
    size_t min_start = numbers->min_starts[block];
    size_t max_start = numbers->max_starts[block];

    // We must check the numbers before subtracting, it might underflow.
    size_t new_min = MAX(
        end > numbers->nums[block] ? end - numbers->nums[block] : 0, min_start);
    size_t new_max = MIN(start, max_start);

    if (new_max < new_min) {
      // If the new max is below the new min, then solving is impossible.
      fprintf(
          stderr,
          "[%s] Invalid bounds for block %zu: [%zu .. %zu] => [%zu .. %zu]\n",
          __func__, block, min_start, max_start, new_min, new_max);
      print_debug_numbers(d);
      exit(1);
    }

    // Update the block bounds.
    *(d->field_updated) += min_start != new_min || max_start != new_max;
    numbers->min_starts[block] = new_min;
    numbers->max_starts[block] = new_max;
  }
}

void combinatorical(struct algorithm_data const *const d) {
  size_t* n = d->numbers->nums;
  if (d->numbers->length == 5 && n[0] == 18 && n[1] == 3 && n[2] == 4 && n[3] == 4 && n[4] == 6) {
    printf("\n\nBEFORE");
    print_debug_numbers(d);
    printf("\n");
  }

  compute_block_position_bounds(d);
  fill_min_max_overlaps(d);
  match_sequences_with_blocks(d);


  if (d->numbers->length == 5 && n[0] == 18 && n[1] == 3 && n[2] == 4 && n[3] == 4 && n[4] == 6) {
    print_debug_numbers(d);
    printf("AFTER\n\n");
  }
}
