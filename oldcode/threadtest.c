#include<sys/wait.h> //usleep as well
#define __USE_BSD	//to get usleep
#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<pthread.h>

char isRow = 0;
int field_w = 20;
int field_h = 20;

pthread_cond_t doShit;
pthread_cond_t waitForShit;
pthread_mutex_t mutex;
int row_id;
int done;

void* trytofuckingsolveit(void* UNUSED)
{
  /* ITERATIVE BRUTEFORCE */

  int thisId;

  pthread_mutex_lock(&mutex);
  pthread_cond_wait(&doShit, &mutex);
  pthread_mutex_unlock(&mutex);
  
  printf("[thr] initted\n");
  fflush(stdout);
  while(1)
  {
    printf("[thr] next iteration\n");
    fflush(stdout);
    pthread_mutex_lock(&mutex);
    thisId = row_id;
    
    if(row_id >= (isRow ? field_h : field_w))
    {
      printf("[thr] done\n");
      fflush(stdout);
      done++;
      pthread_cond_signal(&waitForShit);
      pthread_cond_wait(&doShit, &mutex);
      thisId = row_id;
    }
    
    if(row_id >= 0)
      row_id++;
    
    printf("[thr] working on %d\n", row_id);
    fflush(stdout);
    
    usleep(2000+rand()% 500);

    pthread_mutex_unlock(&mutex);
    
    if(thisId == -1)
      break;
      
    printf("started id %d\n", thisId);
    fflush(stdout);
  }
  
  return NULL;
}

void trytomultithreaddedfuckingsolveit(void)
{
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&waitForShit, NULL);
	pthread_cond_init(&doShit, NULL);
		
	pthread_t tid[4];
	for(int i=0; i<4; i++)
	{
    if(pthread_create(tid+i, NULL, trytofuckingsolveit, NULL))
    {
      fprintf(stderr, "Error creating thread\n");
      return;
    }
  }
  
  printf("threads created\n");
  fflush(stdout);
  
  long iterations = 0;
  int changeNoticed = 1;
  
  pthread_mutex_lock(&mutex);
  while(changeNoticed != 0)
  {
    iterations++;
    changeNoticed = 0;
    
    row_id = 0;
  	isRow = 1;
  	done = 0;
  	
  	for(int i=0; i<4; i++)
  	{
  	  pthread_cond_signal(&doShit);
  	  printf("doshit\n");
  	  fflush(stdout);
	  }
	  
	  while(done != 4)
	  {
	    printf("waiting for threads to be done %d\n", done);
	    fflush(stdout);
      pthread_cond_wait(&waitForShit, &mutex);
      printf("one thread finished\n");
      fflush(stdout);
    }
      
    row_id = 0;
    isRow = 0;
    done = 0;
    
    for(int i=0; i<4; i++)
  	  pthread_cond_signal(&doShit);
	  while(done != 4)
      pthread_cond_wait(&waitForShit, &mutex);
  }
  
  row_id = -1;
	for(int i=0; i<4; i++)
	{
	  pthread_cond_signal(&doShit);
	  printf("doshit\n");
	  fflush(stdout);
  }
  pthread_mutex_unlock(&mutex);
  
  for(int i=0; i<4; i++)
  {
    if(pthread_join(tid[i], NULL)) 
    {
      fprintf(stderr, "Error joining thread\n");
      return;
    }
  }

  printf("Solved in %ld iterations\n", iterations);
}

int main(void)
{
  trytomultithreaddedfuckingsolveit();
}
