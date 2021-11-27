#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "algorithms.h"
#include "field.h"
#include "macros.h"

// Check if a `len` sized block can be put at position `pos`.
__attribute__((pure)) char can_place(int pos, int len,
                                     struct algorithm_data const *const d) {
  // Check that the left side is available.
  if (pos > 0 && d->row[pos - 1] == 'X') {
    return 0;
  }

  for (int i = 0; i < len; i++) {
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

// Merge the suggestion with the row data.
// Check that we don't merge unmergable stuff.
void merge_suggest_into_row(struct algorithm_data const *const d) {
  struct num_list *numbers = d->numbers;
  char *suggest = (char *)d->suggest;
  char *row = (char *)d->row;
  numbers->solved = SOLVED;

  for (int i = 0; i < d->unit_len; i++) {
    if (suggest[i] == '?') {
      // When there is a field that i am not sure about.
      numbers->solved = UNSOLVED;
    } else if (suggest[i] == ' ') {
      // When we deduced a field to be a space.
      if (row[i] == '?' || row[i] == ' ') {
        *d->field_updated += row[i] != ' ';
        row[i] = ' ';
      } else {
        printf("[MERGEDOWN] Cannot merge. Suggested space where row=%c.\n",
               row[i]);
        numbers->solved = UNSOLVED;
        return;
      }
    } else {
      // When we deduced the field to be filled.
      if (row[i] == '?' || row[i] == 'X') {
        *d->field_updated += row[i] != 'X';
        row[i] = 'X';
      } else {
        printf("[MERGEDOWN] At pos %d: Cannot merge. Suggested block %d where "
               "row=%c.\n",
               i, suggest[i], row[i]);
        numbers->solved = UNSOLVED;
        return;
      }
    }
  }
}

char merge_prepare_into_suggest(struct algorithm_data const *const d) {
  int block = 0;
  int block_length = 0;
  struct num_list *numbers = d->numbers;

  // Move over the entire row, validating block placements.
  for (int i = 0; i < d->unit_len; i++) {
    if (d->prepare[i] == block) {
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
      // Can't put a ' ' where an 'X' must be.
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

  for (int i = 0; i < d->unit_len; i++) {
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
char brute_force_(struct algorithm_data const *const d, int block, int start,
                  int end) {
  struct num_list *numbers = d->numbers;

  // Terminate recursion by merging `prepare` into `suggest`.
  if (block >= numbers->length) {
    return merge_prepare_into_suggest(d);
  }

  int len = numbers->nums[block];
  int min_start = numbers->min_starts[block];
  int max_start = numbers->max_starts[block];

  // `rest_len` gives the minimum right padding if all right blocks are moved
  // to the rightmost position.
  int rest_len = 0;
  for (int b = block + 1; b < numbers->length; b++) {
    rest_len += numbers->nums[b] + 1;
  }

  // Write the block into `prepare` at `pos`.
  int pos = MAX(min_start, start);
  for (int i = pos; i < pos + len - 1; i++) {
    d->prepare[i] = (char)block;
  }

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
  for (int i = pos; i < pos + len - 1; i++) {
    d->prepare[i] = SPACE;
  }

  return INVALID;
}

void brute_force(struct algorithm_data const *const d) {
  long num_combinations = 1;
  // Multiply the number of different positions of every block.
  for (int i = 0; i < d->numbers->length; i++) {
    num_combinations *=
        1 + d->numbers->max_starts[i] - d->numbers->min_starts[i];
  }

  // Solve trivially.
  if (num_combinations == 1) {
    d->numbers->solved = SOLVED;

    // TODO What does this do?
    for (int i = 0; i < d->unit_len; i++) {
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
  for (int i = 0; i < d->unit_len; i++) {
    // `prepare` uses the indices of blocks.
    d->prepare[i] = SPACE;
    // `suggest` uses symbols + 0, which means undefined.
    d->suggest[i] = 0;
  }

  // If brute force is not futile, merge the produced result.
  // TODO What about invalid?
  if (brute_force_(d, 0, 0, d->unit_len) != FUTILE) {
    merge_suggest_into_row(d);
  }
}

/* void variateSingleBlocks(struct algorithm_data const *const d) { */
/*   num_list *nl = d->nl; */
/*   char *row = (char *)d->row; */
/*   int minX = g_rowLen, maxX = 0; */
/*  */
/*   for (int i = 0; i < nl->length; i++) { */
/*     int blockLen = nl->nums[i]; */
/*     int minEnd = nl->minStarts[i] + blockLen; */
/*     int maxStart = nl->maxStarts[i]; */
/*  */
/*     minX = MIN(minX, nl->minStarts[i]); */
/*     maxX = MAX(maxX, nl->maxStarts[i] + blockLen); */
/*  */
/*     for (int p = maxStart; p < minEnd; p++) { */
/*       changeNoticed += row[p] != 'X'; */
/*       row[p] = 'X'; */
/*     } */
/*  */
/*     if (minEnd - maxStart == blockLen) { */
/*       if (minEnd < g_rowLen) { */
/*         changeNoticed += row[minEnd] != ' '; */
/*         row[minEnd] = ' '; */
/*       } */
/*       if (maxStart > 0) { */
/*         changeNoticed += row[maxStart - 1] != ' '; */
/*         row[maxStart - 1] = ' '; */
/*       } */
/*     } */
/*   } */
/*  */
/*   for (int i = 0; i < minX; i++) { */
/*     changeNoticed += row[i] != ' '; */
/*     row[i] = ' '; */
/*   } */
/*   for (int i = maxX; i < g_rowLen; i++) { */
/*     changeNoticed += row[i] != ' '; */
/*     row[i] = ' '; */
/*   } */
/* } */
/*  */
/* char computePieceBounds_(struct algorithm_data const *const d, int curBlock, */
/*                          int start, int *end) { */
/*   num_list *const nl = d->nl; */
/*   if (curBlock < 0 || curBlock > nl->length) { */
/*     printf("[RECPUSHVARIATE] Error, invalid curblockId"); */
/*     return INVALID; */
/*   } else if (curBlock == nl->length) // basecase */
/*     return VALID; */
/*  */
/*   // TODO add more recursive base cases */
/*  */
/*   int len = nl->nums[curBlock]; */
/*   int restLen = 0; */
/*   for (int b = curBlock + 1; b < nl->length; b++) */
/*     restLen += 1 + nl->nums[b]; */
/*  */
/*   if (start + restLen > *end) */
/*     return INVALID; */
/*  */
/*   int curPos = start; */
/*   int curEnd; */
/*   char current_status = INVALID; */
/*  */
/*   #<{(|* Find first fit for piece (ffffp) *|)}># */
/*   do { */
/*     if (curPos + len + restLen > *end) */
/*       return INVALID; */
/*  */
/*     current_status = can_place(curPos, len, d) ? VALID : INVALID; */
/*     curPos++; */
/*   } while (current_status == INVALID); */
/*  */
/*   curPos--; */
/*   nl->minStarts[curBlock] = MAX(nl->minStarts[curBlock], curPos); */
/*   // notify change? */
/*  */
/*   #<{(|* Commit position *|)}># */
/*   for (int i = 0; i < len; i++) */
/*     d->prepare[curPos + i] = (char)curBlock; */
/*  */
/*   #<{(|* Try next piece. Content of end is modified after this call *|)}># */
/*   computePieceBounds_(d, curBlock + 1, curPos + len + 1, end); */
/*  */
/*   #<{(|* Rollback *|)}># */
/*   for (int i = 0; i < len; i++) */
/*     d->prepare[curPos + i] = SPACE; */
/*  */
/*   int lastValidPos = curPos; */
/*  */
/*   curEnd = curPos + len; */
/*   d->prepare[curPos] = SPACE; */
/*   d->prepare[curEnd] = (char)curBlock; */
/*   curPos++; */
/*   curEnd++; */
/*  */
/*   int e = *end; */
/*   while (curEnd <= e) { */
/*     // check ground */
/*     if (can_place(curPos, len, d)) */
/*       lastValidPos = curPos; */
/*     curPos++; */
/*     curEnd++; */
/*   } */
/*  */
/*   // DEBUG HERE TODO */
/*   int newms = MIN(lastValidPos, nl->maxStarts[curBlock]); */
/*   if (newms < nl->minStarts[curBlock]) { */
/*     printf("Error, setting maxStart to value lower than minStart\n"); */
/*     printf("oldMin=%d, oldMax=%d, newMax=%d\n", nl->minStarts[curBlock], */
/*            nl->maxStarts[curBlock], newms); */
/*   } */
/*   nl->maxStarts[curBlock] = newms; */
/*  */
/*   // notifyhange? */
/*   *end = lastValidPos - 1; */
/*  */
/*   return VALID; */
/* } */
/*  */
/* void computePieceBounds(struct algorithm_data const *const d) { */
/*   for (int i = 0; i < g_rowLen; i++) { */
/*     d->prepare[i] = SPACE; */
/*   } */
/*   int end = g_rowLen; */
/*  */
/*   computePieceBounds_(d, 0, 0, &end); */
/*   variateSingleBlocks(d); */
/* } */
/*  */
/* #<{(|* */
/*  * For this method the piece bounds must be computed. */
/*  |)}># */
/* void fixBlocks(struct algorithm_data const *const d) { */
/*   char *const row = (char *)d->row; */
/*   num_list *nl = d->nl; */
/*  */
/*   int streakPos; */
/*   int streakLen; */
/*  */
/*   int probablyBlock; */
/*  */
/*   // for each active pixel */
/*   for (int streakIter = 0; streakIter < g_rowLen; streakIter++) */
/*     // for each streak */
/*     if (row[streakIter] == 'X') { */
/*       streakPos = streakIter; */
/*       while (row[streakIter] == 'X' && streakIter < g_rowLen) */
/*         streakIter++; */
/*       streakLen = streakIter - streakPos; */
/*  */
/*       // search matching block. if multiple blocks found -> abort */
/*       probablyBlock = -1; */
/*       for (int block = 0; block < nl->length; block++) */
/*         if (nl->minStarts[block] <= streakPos && // if minstarts is on the left */
/*             nl->maxStarts[block] + nl->nums[block] >= */
/*                 streakPos + streakLen // and endpos on the right */
/*         ) { */
/*           if (probablyBlock != -1) // ambiguous, next streak */
/*           { */
/*             probablyBlock = -2; */
/*             break; */
/*           } */
/*           probablyBlock = block; */
/*         } */
/*  */
/*       if (probablyBlock == -1) { */
/*         printf("Numbers:\n"); */
/*         for (int i = 0; i < nl->length; i++) */
/*           printf("l:%d; min:%d; max:%d\n", nl->nums[i], nl->minStarts[i], */
/*                  nl->maxStarts[i]); */
/*         printf("Row:\n"); */
/*         for (int i = 0; i < g_rowLen; i++) */
/*           putchar(d->row[i]); */
/*         putchar('\n'); */
/*         printf("Streak of len %d at pos %d not matched by any block.\n", */
/*                streakLen, streakPos); */
/*         exit(1); */
/*       } else if (probablyBlock != -2) // unambiguous */
/*       { */
/*         int b = probablyBlock; */
/*  */
/*         int newMi = MAX(streakPos + streakLen - nl->nums[b], nl->minStarts[b]); */
/*         int newMa = MIN(streakPos, nl->maxStarts[b]); */
/*  */
/*         if (newMa < newMi) { */
/*           printf("Numbers:\n"); */
/*           for (int i = 0; i < nl->length; i++) */
/*             printf("l:%d; min:%d; max:%d\n", nl->nums[i], nl->minStarts[i], */
/*                    nl->maxStarts[i]); */
/*           printf("Row:\n"); */
/*           for (int i = 0; i < g_rowLen; i++) */
/*             putchar(d->row[i]); */
/*           putchar('\n'); */
/*  */
/*           printf("Error, setting invalid bounds for block %d\n", b); */
/*           printf("oldMin=%d, oldMax=%d, newMin=%d, newMax=%d\n", */
/*                  nl->minStarts[b], nl->maxStarts[b], newMi, newMa); */
/*           exit(1); */
/*         } */
/*  */
/*         changeNoticed += */
/*             nl->minStarts[b] != newMi || nl->maxStarts[b] != newMa ? 1 : 0; */
/*  */
/*         nl->minStarts[b] = newMi; */
/*         nl->maxStarts[b] = newMa; */
/*       } */
/*     } */
/* } */
/*  */
/* void combinatoricalForce(struct algorithm_data const *const d) { */
/*   computePieceBounds(d); */
/*   fixBlocks(d); */
/* } */
