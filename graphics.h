#ifndef GRAPHICS_H_
#define GRAPHICS_H_

void init_sdl(int w, int h);
void close_sdl(void);

void draw_field(char **);
void handle_events(int *run);
void wait(int);

#endif /* GRAPHICS_H_ */
