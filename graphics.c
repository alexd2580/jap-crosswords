
#include"graphics.h"
#include<math.h>
#include<stdio.h>
#include<stdlib.h>
#include<SDL/SDL.h>

int putenv(char*);

int sizeX;
int sizeY;

int fieldX;
int fieldY;

int ppp;

SDL_Surface* screen;

Uint32 black;
Uint32 white;

void initSDL(int field_w, int field_h)
{
	printf("[GRAPHICS] Loading SDL\n");

  fieldX = field_w;
  fieldY = field_h;

	if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
	{
		printf("[GRAPHICS] Video initialization failed: %s\n", SDL_GetError());
		exit(1);
	}
	
  const SDL_VideoInfo* info = SDL_GetVideoInfo();
  sizeX = info->current_w - 100;
  sizeY = info->current_h - 100;
  
  ppp = sizeX / field_w < sizeY / field_h ? sizeX / field_w : sizeY / field_h;
  sizeX = ppp * field_w;
  sizeY = ppp * field_h;

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	//SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);

  char envVar[] = "SDL_VIDEO_WINDOW_POS=center";
	SDL_putenv(envVar);
	screen = SDL_SetVideoMode(sizeX, sizeY, 32, SDL_SWSURFACE);
	if(screen == NULL)
	{
		printf("[GRAPHICS] Video mode set failed: %s\n", SDL_GetError());
		exit(1);
	}
	
	white = SDL_MapRGBA(screen->format, 255, 255, 255, 255);
	black = SDL_MapRGBA(screen->format, 0, 0, 0, 255);
	SDL_FillRect(screen, NULL, white);
	
	die = 0;
	printf("[GRAPHICS] Done loading SDL\n");
}

void closeSDL(void)
{
	printf("[GRAPHICS] Closing SDL\n");
	SDL_Quit();
	printf("[GRAPHICS] SDL closed\n");
}

void handleEvents(void)
{
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
		case SDL_QUIT:
			printf("[GRAPHICS] Close requested\n");
			die = 1;
			break;
		case SDL_VIDEORESIZE:
			printf("[GRAPHICS] Resize requested\n");
			break;
		case SDL_VIDEOEXPOSE:
			printf("expose\n");
			break;
		default:
			break;
		}
	}
}

void wait(int ms)
{
  SDL_Delay((Uint32)ms);
}

void drawField(char** field)
{
  SDL_Rect o =
  {
    .x = (Sint16)0,
    .y = (Sint16)0,
    .w = (Uint16)ppp,
    .h = (Uint16)ppp,
  };

  for(int i=0; i<fieldX; i++)
  {
    o.x = (Sint16)(i*ppp);
    for(int j=0; j<fieldY; j++)
    {
      o.y = (Sint16)(j*ppp); 
      SDL_FillRect(screen, &o, field[i][j] == 'X' ? black : white);
    }
  }
  
  SDL_Flip(screen);
}

