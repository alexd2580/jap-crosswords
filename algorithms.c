#include <wchar.h>

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
__attribute__((pure)) bool can_place(int pos, int len, algorithm_data_t const* const d) {
    // Check that the left side is available.
    if(pos > 0 && d->field[pos - 1] == 'X') {
        return false;
    }

    for(int i = 0; i < len; i++) {
        if(d->field[pos + i] == ' ') {
            return false;
        }
    }

    // Check that the right side is available.
    if(pos + len < d->field_length && d->field[pos + len] == 'X') {
        return false;
    }

    return true;
}

void print_debug_blocks(algorithm_data_t const* const d) {
    fwprintf(stderr, L"Blocks:\n");
    for(int i = 0; i < d->blocks->size; i++) {
        fwprintf(stderr, L"%d [%d .. %d]\n", d->blocks->blocks[i], d->blocks->min_starts[i],
                 d->blocks->max_starts[i] + d->blocks->blocks[i] - 1);
    }

    fwprintf(stderr, L"Field:\n");
    for(int i = 0; i < d->field_length; i++) {
        fputc(d->field[i], stderr);
    }
    fputc('\n', stderr);
}

// Merge the suggestion with the field data.
// Check that we don't merge unmergable stuff.
void merge_solution_into_field(algorithm_data_t* d) {
    blocks_t* blocks = d->blocks;
    char* suggest = (char*)d->suggest;
    char* field = (char*)d->field;
    blocks->solved = SOLVED;

    for(int i = 0; i < d->field_length; i++) {
        char s = suggest[i];
        if(s == '?') {
            blocks->solved = UNSOLVED;
        } else if(s != '\0' && (s == field[i] || field[i] == '?')) {
            field[i] = s;
        } else if(s != '\0') {
            fwprintf(stderr, L"[%s] Cannot merge. Suggested '%c' where '%c'.\n", __func__, s, field[i]);
            fwprintf(stderr, L"[%s] Field  : %.*s\n", __func__, d->field_length, field);
            fwprintf(stderr, L"[%s] Suggest: %.*s\n", __func__, d->field_length, suggest);
            blocks->solved = UNSOLVED;
            return;
        }
    }
}

// Returns true if entire suggestion is unknown and it's pointless to search further.
int attempts = 0;
bool merge_positions_into_suggest(algorithm_data_t const* const d) {
    attempts++;
    if(attempts % 1000000 == 0) {
        wprintf(L"%d\n", attempts);
    }

    blocks_t* blocks = d->blocks;
    char* suggest = d->suggest;

    // Validate solution against field.
    int pos = 0;
    for(int block = 0; block < blocks->size; block++) {
        int start = d->positions[block];
        int len = blocks->blocks[block];

        // Space.
        while(pos < start) {
            if(d->field[pos] == 'X') {
                return false;
            }
            pos++;
        }

        // Fill block.
        while(pos < start + len) {
            if(d->field[pos] == ' ') {
                return false;
            }
            pos++;
        }
    }

    // Put space.
    while(pos < d->field_length) {
        if(d->field[pos] == 'X') {
            return false;
        }
        pos++;
    }

    wprintf(L"F: %.*s\n", d->field_length, d->field);
    wprintf(L"S: %.*s\nC: ", d->field_length, suggest);
    pos = 0;
    for(int block = 0; block < blocks->size; block++) {
        int start = d->positions[block];
        int len = blocks->blocks[block];

        // Space.
        while(pos < start) {
            wprintf(L" ");
            pos++;
        }

        // Fill block.
        while(pos < start + len) {
            wprintf(L"X");
            pos++;
        }
    }

    // Put space.
    while(pos < d->field_length) {
        wprintf(L" ");
        pos++;
    }
    wprintf(L"\n");

    // Move the positions over.
    pos = 0;
    for(int block = 0; block < blocks->size; block++) {
        int start = d->positions[block];
        int len = blocks->blocks[block];

        // Put space.
        while(pos < start) {
            suggest[pos] = (suggest[pos] == ' ' || suggest[pos] == '\0') ? ' ' : '?';
            pos++;
        }

        // Fill block.
        while(pos < start + len) {
            suggest[pos] = (suggest[pos] == 'X' || suggest[pos] == '\0') ? 'X' : '?';
            pos++;
        }
    }

    // Put space.
    while(pos < d->field_length) {
        suggest[pos] = (suggest[pos] == ' ' || suggest[pos] == '\0') ? ' ' : '?';
        pos++;
    }

    for(int i = 0; i < d->field_length; i++) {
        if(suggest[i] != '?' && d->field[i] == '?') {
            // It's not futile yet, because suggest has some information which field doesn't have.
            return false;
        }
    }
    return true;
}

// All valid solutions are merged into `suggest`.
bool iterate_solutions_until_futile(algorithm_data_t* d, int block, int start) {
    blocks_t* blocks = d->blocks;
    if(block >= blocks->size) {
        return merge_positions_into_suggest(d);
    }

    start = MAX(start, blocks->min_starts[block]);

    // This can be precomputed per line.
    int num_required = 0;
    for(int i = start; i < d->field_length; i++) {
        num_required += d->field[i] == 'X';
    }

    int num_available = 0;
    for(int i = block; i < blocks->size; i++) {
        num_available += blocks->blocks[i];
    }

    /* This check reduces the amount of solutions to check from 226K to 206K for kanzler.jc. */
    /* For the first 129 iterations of 200x120.jc: */
    if(num_available < num_required) {
        return false;
    }

    int len = blocks->blocks[block];

    // This condition (prev != X) reduces the amount of solutions to check from 294K to 226K for kanzler.jc.
    while(start <= blocks->max_starts[block] && (start == 0 || d->field[start - 1] != 'X')) {
        if(can_place(start, len, d)) {
            // Recursively fit the rest of the blocks.
            d->positions[block] = start;
            if(iterate_solutions_until_futile(d, block + 1, start + len + 1)) {
                // If it's futile to try further -> return;
                return true;
            }
        }
        start++;
    }
    return false;
}

__attribute__((const)) uint64_t factorial(uint64_t n) {
    uint64_t res = 1;
    for(uint64_t i = 2; i <= n; i++) {
        if(res > res * i) {
            return 0;
        }
        res = res * i;
    }
    return res;
}

void brute_force(algorithm_data_t* d) {
    int num_combinations = 1;
    // Multiply the number of different positions of every block.
    for(int i = 0; i < d->blocks->size; i++) {
        int next = num_combinations * (1 + d->blocks->max_starts[i] - d->blocks->min_starts[i]);
        if(next < num_combinations) {
            num_combinations = INT_MAX;
            break;
        }
        num_combinations = next;
    }
    d->blocks->num_combinations = num_combinations;

    // This doesn't work because it can't take into account fixed blocks.
    //
    // The number of combinations is also given by
    // the number of distinc partitionings of n = count(free_spaces) + 2
    // into k = count(blocks) + 1 partitions.
    // That is equal to (n - 1 `nCr` `k - 1`).
    // Given f = count(free_spaces) and b = count(blocks)
    // Ergo (f + 1)! / (b! * (f + 1 - b)!)
    /* int num_blocks = d->blocks->size; */
    /* int free_spaces = d->field_length; */
    /* for (int i = 0; i < d->blocks->size; i++) { */
    /*   free_spaces -= d->blocks->blocks[i]; */
    /* } */
    /*  */
    /* uint64_t f_f_p_1 = factorial((uint64_t)(free_spaces + 1)); */
    /* uint64_t f_n = factorial((uint64_t)(num_blocks)); */
    /* uint64_t f_f_p_1_m_n = factorial((uint64_t)(free_spaces + 1 - num_blocks));
     */
    /* uint64_t denominator = f_n * f_f_p_1_m_n; */
    /* if (denominator < f_n || denominator < f_f_p_1_m_n) { */
    /*   denominator = 0; */
    /* } */
    /*  */
    /* int num_partitionings = INT_MAX; */
    /* if (f_f_p_1 != 0 && denominator != 0) { */
    /*   num_partitionings = (int)MIN(f_f_p_1 / denominator, (uint64_t)INT_MAX);
     */
    /* } */

    // Solve trivially.
    /* if(num_combinations == 1) { */
    /*     d->blocks->solved = SOLVED; */
    /*  */
    /*     // TODO What does this do? */
    /*     for(int i = 0; i < d->field_length; i++) { */
    /*         // TODO '?' are spaces? */
    /*         d->field[i] = d->field[i] == 'X' ? 'X' : ' '; */
    /*     } */
    /*     return; */
    /* } */

    // If too many then skip.
    if(num_combinations > d->skip_threshold) {
        d->skipped = true;
        return;
    }

    // Clear `suggest` buffer.
    // `suggest` uses symbols + 0, which means undefined.
    memset(d->suggest, 0, (size_t)d->field_length);

    // If brute force is not futile, merge the produced result.
    if(!iterate_solutions_until_futile(d, 0, 0)) {
        merge_solution_into_field(d);
        d->updated = true;
    }
}

void fill_min_max_overlaps(algorithm_data_t* const d) {
    blocks_t* blocks = d->blocks;
    char* field = d->field;

    int prev_max_end = 0;

    // For each block...
    for(int block = 0; block < blocks->size; block++) {
        int len = blocks->blocks[block];
        int min_start = blocks->min_starts[block];
        int max_start = blocks->max_starts[block];
        int min_end = min_start + len;
        int max_end = max_start + len;

        // Empty fields in front of block.
        if(prev_max_end < min_start) {
            size_t overlap_size = (size_t)(min_start - prev_max_end);
            char* begin = field + prev_max_end;
            d->updated = d->updated || memcmp(begin, d->empty, overlap_size) != 0;
            memset(begin, ' ', overlap_size);
        }

        // Overlapping min/max block areas.
        if(max_start < min_end) {
            size_t overlap_size = (size_t)(min_end - max_start);
            char* begin = field + max_start;
            d->updated = d->updated || memcmp(begin, d->filled, overlap_size) != 0;
            memset(begin, 'X', overlap_size);
        }

        prev_max_end = max_end;
    }

    // Check trailing empty area.
    if(prev_max_end < d->field_length) {
        size_t overlap_size = (size_t)(d->field_length - prev_max_end);
        char* begin = field + prev_max_end;
        d->updated = d->updated || memcmp(begin, d->empty, overlap_size) != 0;
        memset(begin, ' ', overlap_size);
    }
}

__attribute__((pure)) int count_filled(algorithm_data_t const* const d, int start, int end) {
    int count = 0;
    for(int i = start; i < end; i++) {
        count += d->field[i] == 'X';
    }
    return count;
}

__attribute__((pure)) int sum_blocks(algorithm_data_t const* const d, int start, int end) {
    int count = 0;
    for(int i = start; i < end; i++) {
        count += d->blocks->blocks[i];
    }
    return count;
}

void compute_block_position_bounds(algorithm_data_t const* const d) {
    blocks_t* const blocks = d->blocks;

    // TODO This doesn't check that the combination is really valid, there may be
    // a `valid` config where more than the given amount of blocks are fit into
    // the line.

    // Try to fit all the blocks from the left.
    // For the 0th iteration we start at -1 to simplify loop logic.
    int start = -1;
    for(int block = 0; block < blocks->size; block++) {
        // Add one block of spacing.
        start++;

        int len = blocks->blocks[block];

        // Skip to the first in-bounds solution.
        start = MAX(start, blocks->min_starts[block]);

        // Find the leftmost fit.
        // TODO in theory this can segfault if there are no valid solutions.

        // Computing pre-post block count matches solves 200x120.
        int num_blocks_before = sum_blocks(d, 0, block);
        bool too_many_before = count_filled(d, 0, start) > num_blocks_before;
        int num_blocks_after = sum_blocks(d, block + 1, d->blocks->size);
        bool too_many_after = count_filled(d, start + len, d->field_length) > num_blocks_after;
        while(!can_place(start, len, d) || too_many_before || too_many_after) {
            start++;
            too_many_before = count_filled(d, 0, start) > num_blocks_before;
            too_many_after = count_filled(d, start + len, d->field_length) > num_blocks_after;
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
    for(int block = blocks->size - 1; block > -1; block--) {
        // Add one block of spacing.
        end--;

        int len = blocks->blocks[block];

        // Skip to the first in-bounds solution.
        end = MIN(end, blocks->max_starts[block] + len);

        // Find the rightmost fit.
        // TODO in theory this can segfault if there are no valid solutions.
        while(!can_place(end - len, len, d)) {
            end--;
        }

        // Now `end` is the rightmost valid end position of `block`.
        // Move max start position to the new max.
        blocks->max_starts[block] = MIN(blocks->max_starts[block], end - len);

        // Move to start of block.
        end -= len;
    }
}

__attribute__((pure)) bool block_fits_sequence(algorithm_data_t const* d, int block, int seq_start, int seq_len) {
    int block_len = d->blocks->blocks[block];

    bool left = d->blocks->min_starts[block] <= seq_start;
    bool length = seq_len <= block_len;
    bool right = seq_start + seq_len <= d->blocks->max_starts[block] + block_len;

    if(!left || !length || !right) {
        return false;
    }

    int check_start = seq_start + seq_len - block_len;
    int check_end = seq_start;
    for(int pos = check_start; pos <= check_end; pos++) {
        if(can_place(pos, block_len, d)) {
            return true;
        }
    }
    return false;
}

void match_sequences_with_blocks(algorithm_data_t* const d) {
    blocks_t* blocks = d->blocks;
    char* const field = (char*)d->field;

    // Locate sequences.
    int sequence_count = 0;
    int* sequence_starts = (int*)alloca((size_t)d->field_length * sizeof(int));
    int* sequence_lengths = (int*)alloca((size_t)d->field_length * sizeof(int));

    int pos = 0;
    while(pos < d->field_length) {
        while(field[pos] != 'X' && pos < d->field_length) {
            pos++;
        }

        int start = pos;

        while(field[pos] == 'X' && pos < d->field_length) {
            pos++;
        }

        if(pos > start) {
            sequence_starts[sequence_count] = start;
            sequence_lengths[sequence_count] = pos - start;
            sequence_count++;
        }
    }

    // TODO this might be pointlessly complex.

    // Match sequences with blocks. A sequence matches if it lies within the
    // comouted bounds of a block and is equally long or smaller than the block.
    for(int sequence = 0; sequence < sequence_count; sequence++) {
        int first = blocks->size;
        int last = -1;

        int seq_start = sequence_starts[sequence];
        int seq_len = sequence_lengths[sequence];

        // Scan blocks for first match.
        for(int block = 0; block < blocks->size; block++) {
            if(block_fits_sequence(d, block, seq_start, seq_len)) {
                first = block;
                break;
            }
        }

        // Scan blocks for last match (first from the right).
        for(int block = blocks->size - 1; block > -1; block--) {
            if(block_fits_sequence(d, block, seq_start, seq_len)) {
                last = block;
                break;
            }
        }

        // There were matches before, so we don't know which is the right block.
        if(first < last) {
            // Skip this sequence for now.
            continue;
        }

        // Found a single matching block.
        else if(first == last) {
            // Adjust the bounds of the block.
            int len = blocks->blocks[first];

            int min_start = blocks->min_starts[first];
            int max_start = blocks->max_starts[first];

            // We must check the blocks before subtracting, it might underflow.
            int new_min = MAX(min_start, seq_start + seq_len - len);
            int new_max = MIN(max_start, seq_start);

            if(new_max < new_min) {
                // If the new max is below the new min, then solving is impossible.
                fwprintf(stderr, L"[%s] Invalid bounds for block %d: [%d .. %d] => [%d .. %d]\n", __func__, first,
                         min_start, max_start, new_min, new_max);
                print_debug_blocks(d);
                exit(1);
            }

            // Update the block bounds.
            d->updated = d->updated || min_start != new_min || max_start != new_max;
            blocks->min_starts[first] = new_min;
            blocks->max_starts[first] = new_max;
        }

        // No block matched the sequence.
        else if(first > last) {
            fwprintf(stderr, L"[%s] Unmatched sequence [%d .. %d]\n", __func__, seq_start, seq_start + seq_len - 1);
            print_debug_blocks(d);
            exit(1);
        }
    }
}

void combinatorical(algorithm_data_t* const d) {
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
