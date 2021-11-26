#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>
#include <pthread.h>

//Global Variables

int field_w;
int field_h;

struct num_list_
{
  short length;
  short* nums;
  short* minStarts;
  short* maxStarts;
  char solved;
};

typedef struct num_list_ num_list;
typedef num_list numcol;
typedef num_list numrow;

char** field;
numcol* vNumbers;
numrow* hNumbers;

//Init code

void initField(void)
{
  assert(sizeof(char) == 1);

  field = (char**)malloc(field_w*sizeof(char*));
  for(int i=0; i<field_w; i++)
  {
    field[i] = (char*)malloc(field_h);
    memset(field[i], '?', field_h);
  }
  
  vNumbers = (numcol*)malloc(field_w*sizeof(numcol));
  hNumbers = (numrow*)malloc(field_h*sizeof(numrow));
  
  for(int i=0; i<field_w; i++)
  {
    vNumbers[i].length = 0;
    vNumbers[i].nums = NULL;
  }
  
  for(int i=0; i<field_h; i++)
  {
    hNumbers[i].length = 0;
    hNumbers[i].nums = NULL;
  }
}

void freeField(void)
{
  for(int x=0; x<field_w; x++)
    free(field[x]);
  free(field);
  
  for(int x=0; x<field_w; x++)
    if(vNumbers[x].nums != NULL)
      free(vNumbers[x].nums);

  free(vNumbers);
  
  for(int y=0; y<field_h; y++)
    if(hNumbers[y].nums != NULL)
      free(hNumbers[y].nums);

  free(hNumbers);
}

void printField(void)
{
  for(int i=0; i<field_h; i++, putchar('\n'))
    for(int j=0; j<field_w; j++)
      putchar(field[j][i]);    
}

#define SOLVED 1
#define UNSOLVED 0

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
  short* tmp = (short*)malloc(tmpLen * sizeof(short));

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
    }
  
    for(int i=0; i<elems; i++)
    {
      numNum = 0;
      bufPtr = buffer;
      memset(tmp, 0, tmpLen*sizeof(short));
      fgets(buffer, 100, f);
      
      char cont;
      do
      {
        numMatched = sscanf(bufPtr, "%hd", tmp+numNum);
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
      p[i].nums = (short*)malloc(3*numNum*sizeof(short));
      memcpy(p[i].nums, tmp, numNum*sizeof(short));
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

/* ########## CODE OF SOLVER ########## */

char changeNoticed;
int rowSkipped;
long skipTreshold;

#define VALID 0
#define INVALID 1
#define FUTILE 2

#define SPACE (-1) //for prepare

#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

int g_rowLen;
char isRow;

typedef struct data data;
struct data
{
  num_list    *        nl;
  char const  * const  row;
  char        * const  prepare;
  char        * const  suggest;
};

/**
 * check neighbour positions.
 */
char piecePlaceable(short pos, short len, data const * const d)
{
  if(pos > 0)
    if(d->row[pos-1] == 'X')
      return INVALID;
  
  for(int i=0; i<len; i++)
    if(d->row[pos+i] == ' ')
      return INVALID;
  
  if(pos+len < g_rowLen)
    if(d->row[pos+len] == 'X')
      return INVALID;
  
  return VALID;
}

void (*method)(data const * const);

/* ############################################################ */
/* ######################## BRUTEFORCE ######################## */

/**
 * Moves the suggestion (d->suggest) to the row, with a small sanity check.
 */
void mergeDown(data const * const d)
{
  char* row = (char*)d->row;
  d->nl->solved = SOLVED;
  for(int i=0; i<g_rowLen; i++)
    if(d->suggest[i] == '?')
      d->nl->solved = UNSOLVED;
    else if(d->suggest[i] == ' ')
    {
      if(row[i] == '?' || row[i] == ' ')
      {
        changeNoticed += row[i] != ' ';
        row[i] = ' ';
      }
      else
      {
        printf("[MERGEDOWN] Cannot merge. Suggested space where row=%c.\n", row[i]);
        d->nl->solved = UNSOLVED;
        return;
      }
    }
    else //suggest_x
    {
      if(row[i] == '?' || row[i] == 'X')
      {
        changeNoticed += row[i] != 'X';
        row[i] = 'X';
      }
      else
      {
        printf("[MERGEDOWN] At pos %d: Cannot merge. Suggested block %d where row=%c.\n", i, d->suggest[i], row[i]);
        d->nl->solved = UNSOLVED;
        return;
      }
    }
  /*for(int i=0; i<g_rowLen; i++)
    printf("%c", row[i]);
  printf(" after merge\n");*/
}

char validateAndPush(data const * const d)
{
  //for(int i=0; i<g_rowLen; i++)
  //  printf("%c", d->prepare[i] == SPACE ? '_' : d->prepare[i]+'A');
  //printf(" testing\n");

  int pc = 0;
  int streak = 0;
  num_list* const nl = d->nl;
  for(int i=0; i<g_rowLen; i++)
  {
    if(d->prepare[i] == pc) // prepare can contain -1 OR blockNum
    {
      if(d->row[i] == ' ')
        return INVALID;
      
      if(pc >= nl->length)
        return INVALID;
      
      streak++;
      if(streak > nl->nums[pc])
        return INVALID;
    }
    else if(d->prepare[i] == SPACE)
    {
      if(d->row[i] == 'X')
        return INVALID;
      
      if(streak != 0)
      {
        if(pc >= nl->length)
        return INVALID;
      
        if(streak < nl->nums[pc])
          return INVALID;
          
        streak = 0;
        pc++;
      }
    }
    else
      return INVALID;
  }
  
  //Not terminated block
  if(pc == nl->length -1 && streak != nl->nums[pc])
    return INVALID;
  //Terminated block
  else if(pc < nl->length-1)
    return INVALID;
    
  //printf("valid\n");
  
  char futile = FUTILE;
  char prep;
  for(int i=0; i<g_rowLen; i++)
  {
    prep = d->prepare[i];
    if(d->suggest[i] == 0)
      d->suggest[i] = prep != SPACE ? 'X' : ' ';
    else
      d->suggest[i] = d->suggest[i] == 'X' && prep != SPACE ? 'X' :
        d->suggest[i] == ' ' && prep == SPACE ? ' ' : '?';
    
    if(d->suggest[i] != '?' && d->row[i] == '?')
      futile = VALID;
      
    //printf("%c", d->suggest[i]);
  }
  //printf(" now suggested\n");
  
  return futile;
}

char bruteForce_(data const * const, short, short, short);
void bruteForce(data const * const d)
{
  long availCombinations = 1;
  for(int i=0; i<d->nl->length; i++)
    availCombinations *= 1 + d->nl->maxStarts[i] - d->nl->minStarts[i];
  //printf("%ld\n", availCombinations);
  //fflush(stdout);

  if(availCombinations == 1)
  {
    d->nl->solved = SOLVED;
    return;
  }
  else if(availCombinations > skipTreshold)
  {
    rowSkipped++;
    return;
  }

  for(int i=0; i<g_rowLen; i++)
  {
    d->prepare[i] = SPACE;
    d->suggest[i] = 0; //undef
  }
  
  bruteForce_(d, 0, 0, g_rowLen);
  mergeDown(d);
}

char bruteForce_(data const * const d, short cblock, short start, short end)
{
  /*
  if cblock >= d->nl->length return
  for each pos [max(minStart,start)/min(maxStart,end)]
  put block to prepare, check
  if valid then try next block
  move current forward until next free space, try next
  */
  
  if(cblock >= d->nl->length)
    return validateAndPush(d);
  
  num_list* nl = d->nl;
  short len = nl->nums[cblock];
  short minstart = nl->minStarts[cblock];
  short maxstart = nl->maxStarts[cblock];
  
  short restLen = 0;
  for(int b=cblock+1; b<nl->length; b++)
    restLen += 1 + nl->nums[b];
  
  for(int pos=MAX(minstart,start); pos<MIN(maxstart+len,end); pos++)
  {
    if(pos+len+restLen > end)
      return INVALID;
  
    if(piecePlaceable(pos, len, d) == VALID)
    {
      for(int i=pos; i<pos+len; i++)
        d->prepare[i] = cblock;
        
      if(bruteForce_(d, cblock+1, pos+len+1, end) == FUTILE)
        return FUTILE;
      
      for(int i=pos; i<pos+len; i++)
        d->prepare[i] = SPACE;
    }
  }
  
  return INVALID;
}

/* ######################## BRUTEFORCE ######################## */
/* ############################################################ */

/* ############################################################ */
/* ####################### PIECE BOUNDS ####################### */

void variateSingleBlocks(data const * const d)
{
  num_list* nl = d->nl;
  char* row = (char*)d->row;
  int minX = g_rowLen, maxX = 0;
  
  for(int i=0; i< nl->length; i++)
  {
    int blockLen = nl->nums[i];
    int minEnd = nl->minStarts[i] + blockLen;
    int maxStart = nl->maxStarts[i];
    
    minX = MIN(minX, nl->minStarts[i]);
    maxX = MAX(maxX, nl->maxStarts[i]+blockLen);
    
    for(int p=maxStart; p<minEnd; p++)
    {
      changeNoticed += row[p] != 'X';
      row[p] = 'X';
    }
      
    if(minEnd - maxStart == blockLen)
    {
      if(minEnd < g_rowLen)
      {
        changeNoticed += row[minEnd] != ' ';
        row[minEnd] = ' ';
      }
      if(maxStart > 0)
      {
        changeNoticed += row[maxStart-1] != ' ';
        row[maxStart-1] = ' ';
      }
    }
  }
  
  for(int i=0; i<minX; i++)
  {
    changeNoticed += row[i] != ' ';
    row[i] = ' ';
  }
  for(int i=maxX; i<g_rowLen; i++)
  {
    changeNoticed += row[i] != ' ';
    row[i] = ' ';
  }
}

/**
 * Computes min and maxBounds of pieces
 */
char computePieceBounds_(data const * const, short, short, short*);
void computePieceBounds(data const * const d)
{
  for(int i=0; i<g_rowLen; i++)
    d->prepare[i] = SPACE;
  short end = g_rowLen;
  
  computePieceBounds_(d, 0, 0, &end);
  variateSingleBlocks(d);
}

char computePieceBounds_(data const * const d, short curBlock, short start, short* end)
{
  num_list* const nl = d->nl;
  if(curBlock < 0 || curBlock > nl->length)
  {
    printf("[RECPUSHVARIATE] Error, invalid curblockId");
    return INVALID;
  }
  else if(curBlock == nl->length) // basecase
    return VALID;
  
  //TODO add more recursive base cases
  
  short len = nl->nums[curBlock];
  short restLen = 0;
  for(int b=curBlock+1; b<nl->length; b++)
    restLen += 1 + nl->nums[b];
  
  if(start+restLen > *end)
    return INVALID;

  short curPos = start;
  short curEnd;
  char current_status = INVALID;
  
  /** Find first fit for piece (ffffp) **/
  do
  {
    if(curPos + len + restLen > *end)
      return INVALID;
    
    current_status = piecePlaceable(curPos, len, d);
    curPos++;
  }
  while(current_status == INVALID);
  
  curPos--;
  nl->minStarts[curBlock] = MAX(nl->minStarts[curBlock],curPos);
  //notify change?
  
  /** Commit position **/
  for(int i=0; i<len; i++)
    d->prepare[curPos+i] = curBlock;
  
  /** Try next piece. Content of end is modified after this call **/
  computePieceBounds_(d, curBlock + 1, curPos + len + 1, end);
  
  /** Rollback **/
  for(int i=0; i<len; i++)
    d->prepare[curPos+i] = SPACE;
  
  short lastValidPos = curPos;
  
  curEnd = curPos + len;
  d->prepare[curPos] = SPACE;
  d->prepare[curEnd] = curBlock;   
  curPos++;
  curEnd++;

  while(curEnd <= *end)
  {
    //check ground
    if(piecePlaceable(curPos, len, d) == VALID)
      lastValidPos = curPos;
    curPos++;
    curEnd++;
  }
  
  //DEBUG HERE TODO
  int newms = MIN(lastValidPos,nl->maxStarts[curBlock]);
  if(newms < nl->minStarts[curBlock])
  {
    printf("Error, setting maxStart to value lower than minStart\n");
    printf("oldMin=%d, oldMax=%d, newMax=%d\n", 
      nl->minStarts[curBlock], nl->maxStarts[curBlock], newms);
  }
  nl->maxStarts[curBlock] = newms;
  
  //notifyhange?
  *end = lastValidPos - 1;
  
  return VALID;
}

/* ####################### PIECE BOUNDS ####################### */
/* ############################################################ */

/* ############################################################ */
/* ####################### FIXED BLOCKS ####################### */

/**
 * For this method the piece bounds must be computed.
 */
void fixBlocks(data const * const d)
{
  char * const row = (char*)d->row;
  num_list* nl = d->nl;
  
  int streakIter = 0;
  int streakPos;
  int streakLen;
  
  int probablyBlock = -1;
  int len;
  int start;
  int end;
  
  //for each active pixel
  while(streakIter < g_rowLen)
  {
    //for each streak
    if(row[streakIter] == 'X')
    {
      streakPos = streakIter;
      streakLen = 0;
      while(row[streakIter] == 'X')
      {
        streakLen++;
        streakIter++;
      }
      
      //printf("X at pos=%d, len=%d\n", streakPos, streakLen);
      
      //search block. if multiple block found abort
      for(int block=0; block<nl->length; block++)
      {
        len = nl->nums[block];
        start = nl->minStarts[block];
        end = nl->maxStarts[block];
        
        if(start <= streakPos && end + len >= streakPos + streakLen)
        {
          if(probablyBlock != -1) // ambiguous, next streak
          {
            probablyBlock = -2;
            break;
          }
          probablyBlock = block;
          //printf("Block %d is in range\n", block);
        }
      }
      
      if(probablyBlock == -1)
      {
        printf("Numbers:\n");
        for(int i=0; i<nl->length; i++)
          printf("l:%d; min:%d; max:%d\n", nl->nums[i], nl->minStarts[i], nl->maxStarts[i]);
        printf("Row:\n");
        for(int i=0; i<g_rowLen; i++)
          putchar(d->row[i]);
        putchar('\n');
        printf("Streak of len %d at pos %d not mathed by any block.\n", streakLen, streakPos);
        exit(1);
      }
      else if(probablyBlock != -2) // unambiguous
      {
        int b = probablyBlock;
        
        int newMi = MAX(streakPos+streakLen-nl->nums[b], nl->minStarts[b]);
        int newMa = MIN(streakPos, nl->maxStarts[b]);
        
        if(newMa < newMi)
        {
          printf("Numbers:\n");
          for(int i=0; i<nl->length; i++)
            printf("l:%d; min:%d; max:%d\n", nl->nums[i], nl->minStarts[i], nl->maxStarts[i]);
          printf("Row:\n");
          for(int i=0; i<g_rowLen; i++)
            putchar(d->row[i]);
          putchar('\n');
        
          printf("Error, setting invalid bounds for block %d\n", b);
          printf("oldMin=%d, oldMax=%d, newMin=%d, newMax=%d\n", 
            nl->minStarts[b], nl->maxStarts[b], newMi, newMa);
          exit(1);
        }
        
        nl->maxStarts[b] = newMa;
        nl->minStarts[b] = newMi;
        //notify change
      }      
    }
    streakIter++;
  }
  
  return;
}

/* ####################### FIXED BLOCKS ####################### */
/* ############################################################ */

void combinatoricalForce(data const * const d)
{
  computePieceBounds(d);
  fixBlocks(d);
}

/** MAIN CODE **/

pthread_cond_t waitForJobs;
pthread_cond_t waitForResults;
pthread_mutex_t mutex;

int next_task_id;
int threads_idle;

#define WAITFORJOBS pthread_cond_wait(&waitForJobs, &mutex)
#define NOTIFYJOBS pthread_cond_broadcast(&waitForJobs)
#define WAITFORRESULTS pthread_cond_wait(&waitForResults, &mutex)
#define NOTIFYRESULTS pthread_cond_signal(&waitForResults)

void* trytofuckingsolveit(void* UNUSED)
{
  (void)UNUSED;

  /* ITERATIVE PARALLEL BRUTEFORCE */
  int orig_bytes = field_w > field_h ? field_w : field_h;
  char* row = (char*)malloc(3*orig_bytes);
  
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
    
    if(next_task_id >= 0)
      next_task_id++;
    
    pthread_mutex_unlock(&mutex);
    /* End of critical section */
    
    /* Terminate if task_id == -1 */
    if(thisId == -1)
      break;
      
    if(isRow)
    {
      d.nl = hNumbers+thisId;
      
      if(d.nl->solved == UNSOLVED)
      {
        for(int i=0; i<field_w; i++)
          row[i] = field[i][thisId];

        method(&d);

        for(int i=0; i<field_w; i++)
          field[i][thisId] = row[i];
      }
    }
    else
    {
      d.nl = vNumbers+thisId;
      
      if(d.nl->solved == UNSOLVED)
      {
        for(int i=0; i<field_h; i++)
          row[i] = field[thisId][i];
        
        method(&d);
        
        for(int i=0; i<field_h; i++)
          field[thisId][i] = row[i];
      }
    }
  }
  
  free(row);
  
  return NULL;
}

#define THREADS 4

void trytomultithreaddedfuckingsolveit(void)
{
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&waitForJobs, NULL);
	pthread_cond_init(&waitForResults, NULL);
	
	/* Initialize threads */
	threads_idle = 0;
	pthread_t tid[THREADS];
	for(int i=0; i<THREADS; i++)
    if(pthread_create(tid+i, NULL, trytofuckingsolveit, NULL))
    {
      fprintf(stderr, "Error creating thread\n");
      return;
    }
  
  long iterations = 0;
  
  pthread_mutex_lock(&mutex);
  
  /* Wait for threads to wait for me */
  while(threads_idle != THREADS)
    WAITFORRESULTS;
  /* Thank you for helping us helping you help us all */

#define EXECTHREADS \
  NOTIFYJOBS; \
  do WAITFORRESULTS; \
  while(threads_idle != THREADS);

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
    EXECTHREADS \
    /* Switch direction */ \
    next_task_id = 0; \
    isRow = 0; \
    g_rowLen = field_h; \
    /* Signal threads and wait for results */ \
    EXECTHREADS \
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
  for(int i=0; i<THREADS; i++)
    if(pthread_join(tid[i], NULL)) 
    {
      fprintf(stderr, "Error joining thread\n");
      return;
    }

  printf("Solved in %ld iterations\n", iterations);
  printField();
}

int main(int argc, char* argv[])
{
  if(argc < 2)
  {
    printf("[MAIN] Not enough arguments. Gief filename.\n");
    return 1;
  }
  
  int res = loadFile(argv[1]);
  if(res != 0) 
    return res; 
    
  trytomultithreaddedfuckingsolveit();
  
  freeField();
  return 0;
}

int mailn(void)
{
  g_rowLen = 10;
  int numPieces = 2;
  char* row = (char*)malloc(3*g_rowLen);
  for(int i=0; i<g_rowLen; i++)
    row[i] = '?';
  row[0] = 'X';
  
  short nums[3*5];
  
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
}

