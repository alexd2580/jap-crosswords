#ifndef GRAPHICS_H_
#define GRAPHICS_H_

void initSDL(int w, int h);
void closeSDL(void);

void drawField(char**);
void handleEvents(void);
void wait(int);

char die;

#endif /* GRAPHICS_H_ */
