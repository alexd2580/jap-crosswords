#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>
#include<pthread.h>
#include"algorithms.h"

//Global Variables

int field_w;
int field_h;

typedef num_list numcol;
typedef num_list numrow;

char** field; //'required' level of indirection as Prof.Gollmann said (JFF)
char* raw_field_data;

num_list* sides;
numcol* vNumbers;
numrow* hNumbers;

//Init code
void initField(void)
{
  assert(sizeof(char) == 1);

  field = (char**)malloc((size_t)field_w*sizeof(char*));
  raw_field_data = (char*)malloc((size_t)(field_w*field_h));
  memset(raw_field_data, '?', (size_t)(field_w*field_h));
  
  for(int i=0; i<field_w; i++)
    field[i] = raw_field_data+i*field_h;
  
  sides = (num_list*)malloc((size_t)(field_w+field_h)*sizeof(num_list));
  vNumbers = (numcol*)sides;
  hNumbers = (numrow*)sides+field_w;
  
  for(int i=0; i<field_w+field_h; i++)
  {
    sides[i].length = 0;
    sides[i].nums = NULL;
  }
}

void freeField(void)
{
  free(field);
  free(raw_field_data);
  
  for(int i=0; i<field_w+field_h; i++)
    if(sides[i].nums != NULL)
      free(sides[i].nums);

  free(sides);
}

void printField(void)
{
  for(int i=0; i<field_h; i++, putchar('\n'))
    for(int j=0; j<field_w; j++)
      putchar(field[j][i]);    
}

int loadFile(char const* name)
{
  FILE* f = fopen(name, "r");
  if(f == NULL)
  {
    printf("[FOPEN] Could not open file\n");
    return 1;
  }
  
  char buffer[100];
  fgets(buffer, 100, f);
  sscanf(buffer, "%d", &field_h);
  fgets(buffer, 100, f);
  sscanf(buffer, "%d", &field_w);
 
  initField();
  
  int tmpLen = field_w > field_h ? field_w : field_h;
  int* tmp = (int*)malloc((size_t)tmpLen * sizeof(int));

  char* bufPtr;
  int numMatched;
  int numNum;
  for(int r=0; r<2; r++)
  {
    num_list* p;
    int elems;
    int rowLen;
    switch(r)
    {
    case 0:
      p = hNumbers;
      elems = field_h;
      rowLen = field_w;
      break;
    case 1:
      p = vNumbers;
      elems = field_w;
      rowLen = field_h;
      break;
    default:
      printf("CFAILED\n");
      exit(1);
    }
  
    for(int i=0; i<elems; i++)
    {
      numNum = 0;
      bufPtr = buffer;
      memset(tmp, 0, (size_t)tmpLen*sizeof(int));
      fgets(buffer, 100, f);
      
      char cont;
      do
      {
        numMatched = sscanf(bufPtr, "%d", tmp+numNum);
        if(numMatched == 1)
        {
          numNum++;
          cont = 0;
          while(*bufPtr >= '0' && *bufPtr <= '9' && cont == 0)
          {
            bufPtr++;
            if(*bufPtr == ',')
            {
              cont = 1;
              bufPtr++;
            }
          }
        }
      }
      while(cont);
      
      p[i].length = numNum;
      p[i].nums = (int*)malloc(3*(size_t)numNum*sizeof(int));
      memcpy(p[i].nums, tmp, (size_t)numNum*sizeof(int));
      p[i].minStarts = p[i].nums+numNum;
      p[i].maxStarts = p[i].minStarts+numNum;
      p[i].solved = UNSOLVED;
      
      for(int n=0; n<numNum; n++)
      {
        p[i].minStarts[n] = 0;
        p[i].maxStarts[n] = rowLen - p[i].nums[n];
      }
    }
  }

  fclose(f);
  free(tmp);
  return 0;
}

/** Variables shared by solver. use with care **/

int changeNoticed;
int rowSkipped;
long skipTreshold;

//these are constant while a thread is (should be) active
int g_rowLen;
char isRow;
void (*method)(data const * const);

/** MAIN CODE **/

pthread_cond_t waitForJobs;
pthread_cond_t waitForResults;
pthread_mutex_t mutex;

volatile int next_task_id;
volatile int threads_idle;

#define WAITFORJOBS pthread_cond_wait(&waitForJobs, &mutex)
#define NOTIFYJOBS pthread_cond_broadcast(&waitForJobs)
#define WAITFORRESULTS pthread_cond_wait(&waitForResults, &mutex)
#define NOTIFYRESULTS pthread_cond_signal(&waitForResults)

void* trytofuckingsolveit(void* UNUSED)
{
  (void)UNUSED; //ignore argument

  /* ITERATIVE PARALLEL BRUTEFORCE */
  int orig_bytes = field_w > field_h ? field_w : field_h;
  char* row = (char*)malloc(3*(size_t)orig_bytes);
  int raw_step;
  char* raw_ptr;
  
  data d = 
  {
    .nl = NULL,
    .row = row,
    .prepare = row+orig_bytes,
    .suggest = row+2*orig_bytes
  };

  int thisId;

  /* Wait for start signal */
  pthread_mutex_lock(&mutex);
  threads_idle++;
  NOTIFYRESULTS;
  WAITFORJOBS;
  threads_idle--;
  pthread_mutex_unlock(&mutex);
    
  while(1) //worker
  {
    /* Get job id (start of critical section) */
    pthread_mutex_lock(&mutex);
    
    while(next_task_id >= (isRow ? field_h : field_w))
    {
      threads_idle++;
      NOTIFYRESULTS;
      WAITFORJOBS;
      threads_idle--;
    }
    
    thisId = next_task_id;
    next_task_id += next_task_id >= 0;
    
    pthread_mutex_unlock(&mutex);
    /* End of critical section */
    
    /* Terminate if task_id == -1 */
    if(thisId == -1)
      break;
    
    /* Launch *method */
    d.nl = (isRow ? hNumbers : vNumbers) + thisId;
    if(d.nl->solved == UNSOLVED)
    {
      raw_step = isRow ? field_h : 1;
      raw_ptr = raw_field_data + thisId * (isRow ? 1 : field_h);
    
      for(int i=0; i<g_rowLen; i++)
        row[i] = raw_ptr[i*raw_step];
        
      method(&d);

      for(int i=0; i<g_rowLen; i++)
        raw_ptr[i*raw_step] = row[i];
    }
  }
  
  free(row);
  
  return NULL;
}

long trytomultithreaddedfuckingsolveit(int NUMTHREADS)
{
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&waitForJobs, NULL);
	pthread_cond_init(&waitForResults, NULL);
	
	/* Initialize threads */
  pthread_mutex_lock(&mutex);

	threads_idle = 0;
	pthread_t tid[NUMTHREADS];
	for(int i=0; i<NUMTHREADS; i++)
    if(pthread_create(tid+i, NULL, trytofuckingsolveit, NULL))
    {
      fprintf(stderr, "Error creating thread\n");
      return -1;
    }
  
  long iterations = 0;
  
  /* Wait for threads to wait for me */
  while(threads_idle != NUMTHREADS)
    WAITFORRESULTS;
  /* Thank you for helping us helping you help us all */

#define EXECNUMTHREADS \
  NOTIFYJOBS; \
  do WAITFORRESULTS; \
  while(threads_idle != NUMTHREADS);

#define WHILECHANGES \
  changeNoticed = 1; \
  while(changeNoticed != 0 || rowSkipped != 0) \
  { \
    /* Per-iteration tasks */ \
    iterations++; \
    changeNoticed = rowSkipped = 0; \
    /* Variable initialization */ \
    next_task_id = 0; \
    isRow = 1; \
    g_rowLen = field_w; \
    /* Signal threads and wait for results */ \
    EXECNUMTHREADS \
    /* Switch direction */ \
    next_task_id = 0; \
    isRow = 0; \
    g_rowLen = field_h; \
    /* Signal threads and wait for results */ \
    EXECNUMTHREADS \
    if(rowSkipped != 0) skipTreshold *= 100; \
  } \

  skipTreshold = 10L;
  method = combinatoricalForce;
  WHILECHANGES
  method = bruteForce;
  WHILECHANGES
  
  /* Notify end of tasks */
  next_task_id = -1;
  pthread_mutex_unlock(&mutex);
  NOTIFYJOBS;
  
  /* Wait for threads to terminate */
  for(int i=0; i<NUMTHREADS; i++)
    if(pthread_join(tid[i], NULL)) 
      fprintf(stderr, "Error joining thread\n");

  return iterations;
}

#include"graphics.h"

#define CONSOLE   1 /* -c */
#define SDL       2 /* -s */

int flags;
char* file;

void handleArguments(int argc, char* argv[])
{
  flags = 0;
  file = NULL;
  
  if(argc < 2)
  {
    printf("[MAIN] Not enough arguments. Gief filename.\n");
    exit(1);
  }
  
  for(int i=1; i<argc; i++)
  {
    if(strncmp(argv[i], "-c", 2) == 0)
      flags |= CONSOLE;
    else if(strncmp(argv[i], "-s", 2) == 0)
      flags |= SDL;
    else
      file = argv[1];
  }
}

int main(int argc, char* argv[])
{
  handleArguments(argc, argv);
  int res = loadFile(file);
  if(res != 0) 
    return res; 
    
  trytomultithreaddedfuckingsolveit(1);
  
  if(flags & CONSOLE)
    printField();
  
  if(flags & SDL)
  {
    initSDL(field_w, field_h);
    drawField(field);

    while(die == 0)
    {
      handleEvents();
      wait(200);
    }

    closeSDL();
  }

  freeField();
  return 0;
}

/* ####################################################### */
/* ####################################################### */

/*int mailn(void)
{
  g_rowLen = 10;
  int numPieces = 2;
  char* row = (char*)malloc(3*g_rowLen);
  for(int i=0; i<g_rowLen; i++)
    row[i] = '?';
  row[0] = 'X';
  
  int nums[3*5];
  
  for(int i=0; i<5; i++)
    nums[i] = 0;
    
  nums[0] = 2;
  nums[1] = 4;
  nums[2] = 1;
  
  num_list nl =
  {
    .length = numPieces,
    .nums = nums + 0,
    .minStarts = nums + 5,
    .maxStarts = nums + 10,
    .solved = UNSOLVED
  };
  
  data d = 
  {
    .nl = &nl,
    .row = row,
    .prepare = row+g_rowLen,
    .suggest = row+2*g_rowLen
  };
  
  computePieceBounds(&d);

  for(int i=0; i<numPieces; i++)
  {
    printf("Block %d: length=%d, minPos=%d, maxPos=%d\n",
      i, nl.nums[i], nl.minStarts[i], nl.maxStarts[i]);
  }
  
  for(int i=0; i<g_rowLen; i++)
    putchar(row[i]);
  putchar('\n');
  
  fixBlocks(&d);
  
  free(row);
  return 0;
}*/

/* ####################################################### */
/* ####################################################### */

