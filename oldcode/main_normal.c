#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>

//Global Variables

int field_w;
int field_h;

struct num_list_
{
  int length;
  int* nums;
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
  sscanf(buffer, "%d", &field_w);
  fgets(buffer, 100, f);
  sscanf(buffer, "%d", &field_h);
 
  initField();
  
  int tmpLen = field_w > field_h ? field_w : field_h;
  int* tmp = (int*)malloc(tmpLen * sizeof(int));

  char* bufPtr;
  int numMatched;
  int numNum;
  for(int r=0; r<2; r++)
  {
    struct num_list_* p;
    int elems;
    switch(r)
    {
    case 0:
      p = hNumbers;
      elems = field_h;
      break;
    case 1:
      p = vNumbers;
      elems = field_w;
      break;
    }
  
    for(int i=0; i<elems; i++)
    {
      numNum = 0;
      bufPtr = buffer;
      memset(tmp, 0, tmpLen*sizeof(int));
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
      p[i].nums = (int*)malloc(numNum*sizeof(int));
      memcpy(p[i].nums, tmp, numNum*sizeof(int));
    }
  }

  fclose(f);
  free(tmp);
  return 0;
}

/* ########## CODE OF SOLVER ########## */

char changeNoticed;

#define VALID 0
//when all blocks are placed and their constellation does not fulfill the rules
#define INVALID 1
//unsolvable
#define UNSOLVABLE 2

num_list g_nl;
char const * g_row;
char* g_prepare;
char* g_suggest;
int g_rowLen;

/**
 * Byte 0 contains hard requirements. Byte 1 contains suggestion
 * This method checks if the full suggestion (all blocks are placed) matches
 * the hard requirements. If not, INVALID is returned.
 */
int validateRow(void)
{
  int pc = 0;
  int streak = 0;
  for(int i=0; i<g_rowLen; i++)
    if(g_prepare[i] == 'X') // prepare can contain 2 values 'X' and ' '
    {
      if(g_row[i] == ' ')
        return INVALID;
      
      if(pc >= g_nl.length)
        return INVALID;
      
      streak++;
      if(streak > g_nl.nums[pc])
        return INVALID;
    }
    else
    {
      if(g_row[i] == 'X')
        return INVALID;
      
      if(streak != 0)
      {
        if(pc >= g_nl.length)
        return INVALID;
      
        if(streak < g_nl.nums[pc])
          return INVALID;
          
        streak = 0;
        pc++;
      }
    }
  
  if(pc == g_nl.length -1 && streak == g_nl.nums[pc])
    return VALID;
    
  return pc != g_nl.length ? INVALID : VALID;
}

void makesuggestion(void)
{
  for(int i=0; i<g_rowLen; i++)
    if(g_suggest[i] == 0)
      g_suggest[i] = g_prepare[i];
    else
      g_suggest[i] = g_prepare[i] == g_suggest[i] ? g_prepare[i] : '?';
}

/**
 * Changes to row are written to the upper three bytes of each int. if any.
 * values << 8 must be ignored. algo results overwrite byte 0
 */
void recPushAndVariate_(int curBlock, int curPos, int start, int end)
{
  if(curBlock < 0 || curBlock > g_nl.length)
  {
    printf("[RECPUSHVARIATE] Error, invalid curblockId");
    return;
  }
  
  //TODO add more recursive base cases
  
  int prevPos = curPos - 1;
  int nextPos = curPos + 1;
  int len = g_nl.nums[curBlock];
  int curEnd = curPos + len;
  
  if(curBlock == g_nl.length) // basecase
  {
    int res = validateRow();
      if(res == VALID)
        makesuggestion();
    return;
  }
  else
  {
    //check start/end validity, recursive basecase
    if(curPos < start || curEnd > end)
      return;
    
    //check neighbour positions.
    char current_invalid = 0;
    if(prevPos >= 0)
      if(g_row[prevPos] == 'X' || g_prepare[prevPos] == 'X')
        current_invalid = 1;
    
    for(int i=0; i<len && !current_invalid; i++)
      if(g_row[curPos+i] == ' ')
        current_invalid = 1;
    
    if(curEnd < g_rowLen && !current_invalid)
      if(g_row[curEnd] == 'X' || g_prepare[curEnd] == 'X')
        current_invalid = 1;
        
    if(!current_invalid)
    {
      //COMMIT    
      for(int i=0; i<len; i++)
        g_prepare[curPos+i] = 'X';
        
      recPushAndVariate_(
        curBlock + 1,   //next curBlock
        curEnd,         //next curPos
        curEnd,         //startPos
        end);           //endPos
        
      for(int i=0; i<len; i++)
        g_prepare[curPos+i] = ' ';
    }
    
    return recPushAndVariate_(curBlock, nextPos, start, end);
  }
}

void mergeDown(void)
{
  char* row = (char*)g_row;
  for(int i=0; i<g_rowLen; i++)
    if(g_suggest[i] == 'X')
    {
      if(g_row[i] == '?' || g_row[i] == 'X')
      {
        changeNoticed += row[i] != 'X';
        row[i] = 'X';
      }
      else
      {
        printf("[MERGEDOWN] Cannot merge. Invalid data produced.\n");
        return;
      }
    }
    else if(g_suggest[i] == ' ')
    {
      if(g_row[i] == '?' || g_row[i] == ' ')
      {
        changeNoticed += row[i] != ' ';
        row[i] = ' ';
      }
      else
      {
        printf("[MERGEDOWN] Cannot merge. Invalid data produced.\n");
        return;
      }
    }
}

/**
 * Use the given block as the variable one.
 * Push everything else away and variate this block marking
 * Use backtracking for piece placement.
 * Set initial borders.
 * For each block (rec.) \ b -> push block away
 * pushblock routine is recursive too, try one step
 * If impossible return IMPOSSIBLE (no value chosen), else update borders.
 * Go down.
 * Fix comment.
 */
void recPushAndVariate(void)
{
  for(int i=0; i<g_rowLen; i++)
  {
    g_suggest[i] = 0;
    g_prepare[i] = ' ';
  }
  recPushAndVariate_(0, 0, 0, g_rowLen);
  mergeDown();
}

void trytofuckingsolveit(void)
{
  /* ITERATIVE BRUTEFORCE */
  
  int orig_bytes = field_w > field_h ? field_w : field_h;
  char* row = (char*)malloc(orig_bytes);
  g_row = row;
  g_suggest = (char*)malloc(orig_bytes);
  g_prepare = (char*)malloc(orig_bytes);
  
  g_rowLen = field_w;
  //gehe alle zeilen durch
  for(int i_0=0; i_0<field_h; i_0++)
  {
    printf("v%d\n", i_0);
    //assign rowNumber struct
    g_nl = hNumbers[i_0];
    
    //Make copy of row
    for(int i_1=0; i_1<field_w; i_1++)
      row[i_1] = field[i_1][i_0];
    
    recPushAndVariate();
    
    //commit
    for(int i_1=0; i_1<field_w; i_1++)
      field[i_1][i_0] = row[i_1];
  }
  
  g_rowLen = field_h;
  //gehe alle spalten durch
  for(int i_0=0; i_0<field_w; i_0++)
  {
    printf("h%d\n", i_0);
    //assign rowNumber struct
    g_nl = vNumbers[i_0];
    
    //Make copy of row
    for(int i_1=0; i_1<field_h; i_1++)
      row[i_1] = field[i_0][i_1];
    
    recPushAndVariate();
    
    //commit
    for(int i_1=0; i_1<field_h; i_1++)
      field[i_0][i_1] = row[i_1];
  }
  
  free(row);
  free(g_suggest);
  free(g_prepare);
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
    
  long iterations = 0;
  changeNoticed = 1;
  while(changeNoticed != 0)
  {
    //printf("[MAIN] Iteration %ld\n", iterations);
    iterations++;
    printField();
    //getchar();
    changeNoticed = 0;
    trytofuckingsolveit();
  }
  
  printf("Solved in %ld iterations\n", iterations);
  printField();
  
  freeField();
  return 0;
}

