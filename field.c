#include <alloca.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Unicode support.
#include <locale.h>
#include <wchar.h>

// Terminal width.
#include <sys/ioctl.h>
#include <unistd.h>

#include "field.h"
#include "macros.h"

__attribute__((const)) bool is_digit(char const c) { return c >= '0' && c <= '9'; }

void allocate_blocks(blocks_t* blocks) {
    size_t int_row_bytes = (size_t)blocks->size * sizeof(int);
    blocks->blocks = (int*)malloc(int_row_bytes);
    blocks->min_starts = (int*)malloc(int_row_bytes);
    blocks->max_starts = (int*)malloc(int_row_bytes);
}

void free_blocks(blocks_t* blocks) {
    free(blocks->blocks);
    free(blocks->min_starts);
    free(blocks->max_starts);
}

// Read a single line of `data` into `blocks`.
// Initialize trivial bounds.
void init_blocks_from_data(blocks_t* blocks, char* data) {
    int buffer[200];

    blocks->size = 0;
    while(*data != '\n') {
        while(!is_digit(*data)) {
            data++;
        }

        buffer[blocks->size] = 0;
        while(is_digit(*data)) {
            buffer[blocks->size] *= 10;
            buffer[blocks->size] += *data - '0';
            data++;
        }
        blocks->size++;
    }

    allocate_blocks(blocks);
    memcpy(blocks->blocks, buffer, (size_t)blocks->size * sizeof(int));
    blocks->num_combinations = 1;
    for(int n = 0; n < blocks->size; n++) {
        blocks->min_starts[n] = 0;
        blocks->max_starts[n] = 10000; // Can't do fields larger than 9999.
    }
    blocks->solved = UNSOLVED;
}

void allocate_field(field_t* field) {
    // Initialize center space.
    field->flat_grid = (char*)malloc((size_t)(field->height * field->width));
    field->rows = (blocks_t*)malloc((size_t)field->height * sizeof(blocks_t));
    field->columns = (blocks_t*)malloc((size_t)field->width * sizeof(blocks_t));
}

void free_field(field_t* field) {
    for(int i = 0; i < field->height; i++) {
        free_blocks(field->rows + i);
    }
    free(field->rows);
    for(int i = 0; i < field->width; i++) {
        free_blocks(field->columns + i);
    }
    free(field->columns);
    free(field->flat_grid);
}

int init_field_from_file(char const* file_name, field_t* field) {
    FILE* f = fopen(file_name, "r");
    if(f == NULL) {
        fprintf(stderr, "[%s] Could not open file\n", __func__);
        return 1;
    }

    char buffer[100];
    fgets(buffer, 100, f);
    sscanf(buffer, "%d", &field->height);
    fgets(buffer, 100, f);
    sscanf(buffer, "%d", &field->width);

    allocate_field(field);

    // Ingest rows.
    for(int i = 0; i < field->height; i++) {
        fgets(buffer, 100, f);
        init_blocks_from_data(field->rows + i, buffer);
    }

    // Ingest columns.
    for(int i = 0; i < field->width; i++) {
        fgets(buffer, 100, f);
        init_blocks_from_data(field->columns + i, buffer);
    }

    memset(field->flat_grid, '?', (size_t)(field->width * field->height));

    fclose(f);
    return 0;
}

void print_field(field_t* field) {
    for(int y = 0; y < field->height; y++, putchar('\n')) {
        for(int x = 0; x < field->width; x++) {
            putchar(field->flat_grid[x * field->height + y]);
        }
    }
}

/* int msb32(int x) { */
/*     x |= (x >> 1); */
/*     x |= (x >> 2); */
/*     x |= (x >> 4); */
/*     x |= (x >> 8); */
/*     x |= (x >> 16); */
/*     return (x & ~(x >> 1)); */
/* } */

// Empty, lower, upper, both.
wchar_t block_chars[4] = {L' ', 0x2584, 0x2580, 0x2588};

void print_field_unicode_with_padding(field_t* field, int padding_h, int padding_w) {
    // We use unicode to fit the image to the terminal viewport.
    // E.g. Process 2 lines at once, the rough font ratio is 2 height -> 1 width,
    // so to have a proportional image we use unicode halfblock and fullblock
    // characters.

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    int lines = w.ws_row;
    int columns = w.ws_col;

    // One vertical unit is half of a block.
    // One horizontal unit a block.
    int factor_width = (columns - padding_w) / field->width;
    int factor_height = ((lines - padding_h) * 2) / field->height;
    int factor = MIN(factor_height, factor_width);

    if(factor == 0) {
        factor = 1;
    }

    int pixel_height = field->height * factor;
    int pixel_width = field->width * factor;
    // Assuming `(bool)0 == false`.
    bool pixels[pixel_height * pixel_width];
    memset(pixels, 0, (size_t)(pixel_height * pixel_width) * sizeof(bool));

    for(int y_field = 0; y_field < field->height; y_field++) {
        for(int x_field = 0; x_field < field->width; x_field++) {
            bool is_x = field->flat_grid[x_field * field->height + y_field] == 'X';

            for (int y_texel = 0; y_texel < factor; y_texel++) {
                for (int x_texel = 0; x_texel < factor; x_texel++) {
                    int x_pixel = x_field * factor + x_texel;
                    int y_pixel = y_field * factor + y_texel;
                    pixels[x_pixel * pixel_height + y_pixel] = is_x;
                }
            }
        }
    }

    for(int y_pixel = 0; y_pixel < pixel_height; y_pixel += 2, wprintf(L"\n")) {
        for(int x_pixel = 0; x_pixel < pixel_width; x_pixel++) {
            char upper = pixels[x_pixel * pixel_height + y_pixel];
            char lower = y_pixel < pixel_height && pixels[x_pixel * pixel_height + y_pixel + 1];
            wchar_t c = block_chars[((int)lower) + ((int)upper << 1)];
            wprintf(L"%lc", c);
        }
    }
}

void print_field_unicode(field_t* field) {
    print_field_unicode_with_padding(field, 0, 0);
}
