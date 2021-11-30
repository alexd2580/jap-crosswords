#ifndef ALGORITHMS_H_
#define ALGORITHMS_H_

#include "field.h"

struct algorithm_data {
    char* field;
    int field_length;

    blocks_t* blocks;

    // For brute force.
    char* empty;
    char* filled;
    char* unknown;
    int* const positions;
    char* const suggest;
    int skip_threshold;

    bool updated;
    bool skipped;
};
typedef struct algorithm_data algorithm_data_t;

void combinatorical(algorithm_data_t*);
void brute_force(algorithm_data_t*);

#endif
