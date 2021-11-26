#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>

//#define EXIT_SUCCESS 0 // already defined
#define EXIT_CBROKEN 1
#define EXIT_ALGORITHMERROR 2
#define EXIT_INSANEINPUT 3

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
  {
    if(vNumbers[x].nums != NULL)
      free(vNumbers[x].nums);
  }
  free(vNumbers);
  
  for(int y=0; y<field_h; y++)
  {
    if(hNumbers[y].nums != NULL)
      free(hNumbers[y].nums);
  }
  free(hNumbers);
}

void printWithMargin(void)
{
  int maxCol = 0, maxRow = 0;
  
  for(int i=0; i<field_h; i++)
    if(maxRow < hNumbers[i].length)
      maxRow = hNumbers[i].length; 
  
  for(int i=0; i<field_w; i++)
    if(maxCol < vNumbers[i].length)
      maxCol = vNumbers[i].length; 
  
  for(int y=0; y<maxCol; y++)
  {
    for(int x=0; x<maxRow; x++)
      putchar(' ');
    putchar('|');
    int off;
    for(int x=0; x<field_w; x++)
    {
      off = maxCol - vNumbers[x].length;
      putchar(y >= off ? '0' + vNumbers[x].nums[y-off] : ' ');
    }
    putchar('\n');
  }
  
  for(int x=0; x<maxRow; x++)
    putchar('-');
  putchar('+');
  for(int x=0; x<field_w; x++)
    putchar('-');
  
  putchar('\n');
  
  for(int y=0; y<field_h; y++)
  {
    int off = maxRow - hNumbers[y].length;
    for(int x=0; x<maxRow; x++)
      putchar(x >= off ? '0' + hNumbers[y].nums[x-off] : ' ');
    putchar('|');
    
    for(int x=0; x<field_w; x++)
      putchar(field[x][y]);      
    putchar('\n');
  }
}

void printField(void)
{
  for(int i=0; i<field_h; i++)
  {
    for(int j=0; j<field_w; j++)
      putchar(field[j][i]);    
    putchar('\n');
  }
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
int g_mainBlock;

/**
 * Byte 0 contains hard requirements. Byte 1 contains suggestion
 * This method checks if the full suggestion (all blocks are placed) matches
 * the hard requirements. If not, INVALID_PLACEMENT is returned.
 */
int validateRow(void)
{
  int pc = 0;
  int streak = 0;
  for(int i=0; i<g_rowLen; i++)
  {
    if(g_prepare[i] == 'X')
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
  }
  
  if(pc == g_nl.length -1 && streak == g_nl.nums[pc])
    return VALID;
    
  return pc != g_nl.length ? INVALID : VALID;
}

#define SHIFTTOLEFT(l, start, end) shiftToSide_(+1, l, start, start, end)
#define SHIFTTORIGHT(l, start, end) shiftToSide_(-1, l, (end-l), start, end)

#define IS_EMPTY(x) (BYTE0OF(x) != 'X' && BYTE1OF(x) != 'X')

char shiftToSide_(int dir, int l, int p, int start, int end)
{
  if(p < start || p+l > end)
    return INVALID; //basecase
  
  char current_invalid = 0;
  //check if there is space on left side
  if(p > 0)
    if(g_row[p-1] == 'X' || g_prepare[p-1] == 'X')
      current_invalid = 1;
  
  for(int i=0; i<l && !current_invalid; i++)
    if(g_row[p+i] == ' ')
      current_invalid = 1;
  
  //check if there is space on right side
  if(p+l < g_rowLen && !current_invalid)
    if(g_row[p+l] == 'X' || g_prepare[p+l] == 'X')
      current_invalid = 1;
      
  char res;
  if(current_invalid == 0)
  {
    //COMMIT
    for(int i=0; i<l; i++)
      g_prepare[p+i] = 'X';
      
    res = validateRow();
    
    printf("\nactual row\n");
    for(int i=0; i<g_rowLen; i++)
      printf("%c", g_row[i]);
    printf("\n");
    
    printf("valid? %d\n", res);
    for(int i=0; i<g_rowLen; i++)
      printf("%d ", g_prepare[i]);
    printf("\n");
    
    if(res != VALID)
      for(int i=0; i<l; i++)
        g_prepare[p+i] = ' ';
  }
  else
    res = shiftToSide_(dir, l, p+dir, start, end);
  
  return res;
}

char makesuggestion(int start, int end)
{
  char* rowcpyl = (char*)malloc(2*g_rowLen);
  memcpy(rowcpyl, g_prepare, g_rowLen);
  char* rowcpyr = rowcpyl + g_rowLen;
  memcpy(rowcpyr, g_prepare, g_rowLen);
  
  int len = g_nl.nums[g_mainBlock];
  
  char* prepare_row = g_prepare;
  g_prepare = rowcpyl;      
  int lres = SHIFTTOLEFT(len, start, end);
  g_prepare = rowcpyr;
  int rres = SHIFTTORIGHT(len, start, end);
  g_prepare = prepare_row;
  
  if(lres != VALID || rres != VALID)
    return INVALID;
 
  printf("Suggesting\n");
  //input should be valid here
  
  for(int i=0; i<g_rowLen; i++)
  {
    if(g_suggest[i] == 0)
    {
      if(rowcpyl[i] == rowcpyr[i])
        g_suggest[i] = rowcpyl[i];
      else 
        g_suggest[i] = '?';
    }
    else
    {
      if(rowcpyl[i] == rowcpyr[i] && rowcpyl[i] == g_suggest[i])
        g_suggest[i] = rowcpyl[i];
      else
        g_suggest[i] = '?';
    }
  }

  free(rowcpyl);
  
  return VALID;
}

#define LEFT 1
#define RIGHT 2

/**
 * Changes to row are written to the upper three bytes of each int. if any.
 * values << 8 must be ignored. algo results overwrite byte 0
 */
int 
recPushAndVariate_
(
  int curBlock, 
  int curPos, 
  int start, //first available space
  int end, //last available space +1
  char dir //so that the recursion knows when to variate
)
{
  if(curBlock < 0 || curBlock >= g_nl.length)
  {
    printf("[RECPUSHVARIATE] Error, invalid curblockId");
    exit(EXIT_ALGORITHMERROR);
  }
  
  //TODO add more recursive base cases
  
  int prevPos = curPos - 1;
  int nextPos = curPos + 1;
  int len = g_nl.nums[curBlock];
  int curEnd = curPos + len;
  
  if(curBlock == g_mainBlock) // basecase
  {
    if(dir == LEFT)
      return recPushAndVariate_(g_nl.length-1, end-g_nl.nums[g_nl.length-1], start, end, RIGHT);
    else if(dir == RIGHT)
      return variate(start, end);
    else
      exit(EXIT_CBROKEN);
  }
  else
  {
    int dir = curBlock < g_mainBlock ? LEFT : RIGHT;
    
    //check start/end validity, recursive basecase
    if(curPos < start || curEnd > end)
      return INVALID;
    
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
        
      int nextBlock = curBlock + (dir == LEFT ? +1 : -1); //next block to place
      recPushAndVariate_(
        nextBlock,                                          //next curBlock
        dir == LEFT ? curEnd : curPos-g_nl.nums[nextBlock], //next curPos
        dir == LEFT ? curEnd : start,                      //startPos
        dir == LEFT ? end : curPos,                        //endPos
        dir);
        
      for(int i=0; i<len; i++)
        g_prepare[curPos+i] = ' ';
    }
    
    return recPushAndVariate_(curBlock, dir == LEFT ? nextPos : prevPos, start, end, dir);
  }
  exit(EXIT_INSANEINPUT);
}

void mergeDown(void)
{
  printf("[MERGE] Current suggestion\n");
    for(int i=0; i<g_rowLen; i++)
      printf("%c", g_suggest[i]);
  printf("\n");

  char* row = (char*)g_row;
  for(int i=0; i<g_rowLen; i++)
  {
    if(g_suggest[i] == 'X')
    {
      if(g_row[i] == '?' || g_row[i] == 'X')
        row[i] = 'X';
      else
      {
        printf("[MERGEDOWN] Cannot merge. Invalid data produced.\n");
        exit(EXIT_INSANEINPUT);
      }
    }
    else if(g_suggest[i] == ' ')
    {
      if(g_row[i] == '?' || g_row[i] == ' ')
        row[i] = ' ';
      else
      {
        printf("[MERGEDOWN] Cannot merge. Invalid data produced.\n");
        exit(EXIT_INSANEINPUT);
      }
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
void recPushAndVariate(int block)
{
  g_mainBlock = block;

  for(int i=0; i<g_rowLen; i++)
  {
    g_suggest[i] = 0;
    g_prepare[i] = ' ';
  }

  recPushAndVariate_(0, 0, 0, g_rowLen, LEFT);

  mergeDown();
}

void trytofuckingsolveit(void)
{
  /*
  for vertical, horizontal
  {
    für jeden block in der reihe
    {
      schiebe alles nach so weit wie möglich vom block weg
      dabei auf vorhandene unbelegbare und wiederrum zu belegende felder achten
      
      schiebe block nach links, notiere position, 
      dann nach rechts und markiere überschneidungen als 'X'
      
      wenn ein stabiler block gefunden wurde, markiere felder nebenan als ' '
      wenn ein zwei nebeneinanderliegende semistabile blöcke gefunden worden sind,
      schiebe beide aufeinander zu und markiere unbelegbaren zwischenraum als ' '
    }
  }
  */
  
  /* ROWS */
  int orig_bytes = field_w > field_h ? field_w : field_h;
  char* row = (char*)malloc(orig_bytes);
  g_row = row;
  g_suggest = (char*)malloc(orig_bytes);
  g_prepare = (char*)malloc(orig_bytes);
  
  g_rowLen = field_w;
  //gehe alle zeilen durch
  for(int i_0=0; i_0<field_h; i_0++)
  {
    //assign rowNumber struct
    g_nl = hNumbers[i_0];
    
    //Make copy of row
    for(int i_1=0; i_1<field_w; i_1++)
      row[i_1] = field[i_1][i_0];
    
    recPushAndVariate(0);
    
    //commit
    for(int i_1=0; i_1<field_w; i_1++)
      field[i_1][i_0] = row[i_1];
  }
  
  /*g_rowLen = field_h;
  //gehe alle spalten durch
  for(int i_0=0; i_0<field_w; i_0++)
  {
    //assign rowNumber struct
    g_nl = vNumbers[i_0];
    
    //Make copy of row
    for(int i_1=0; i_1<field_h; i_1++)
      row[i_1] = field[i_0][i_1];
    
    //for each block as the variable one
    for(int i_2=0; i_2<g_nl.length; i_2++)
      recPushAndVariate(i_2);
    
    //commit
    for(int i_1=0; i_1<field_h; i_1++)
      field[i_0][i_1] = row[i_1];
  }*/
  
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
    
  printWithMargin();
  putchar('\n');
  
  long iterations = 0;
  changeNoticed = 1;
  while(changeNoticed != 0)
  {
    printf("[MAIN] Iteration %ld\n", iterations);
    iterations++;
    printField();
    getchar();
    changeNoticed = 0;
    trytofuckingsolveit();
  }
  
  printField();
  
  printf("[MAIN] Solved in %ld iterations\n", iterations);
  freeField();
  return 0;
}

/* ########## OLD CODE ########## */


/*
int mainq(void)
{
  int* nums = (int*)malloc(10*sizeof(int));
  char* row = (char*)malloc(10);
  g_row = row;
  g_suggest = (char*)malloc(10);
  g_prepare = (char*)malloc(10);
  g_rowLen = 10;
  g_nl.length = 3;
  g_nl.nums = nums;
  nums[0] = 2;
  nums[1] = 2;
  nums[2] = 3;
  for(int i=0; i<g_rowLen; i++)
    row[i] = '?';    
  //row[0] = 'X';
  //row[3] = 'X';
  
  recPushAndVariate(0);
  recPushAndVariate(1);
  recPushAndVariate(2);
  
  for(int i=0; i<g_rowLen; i++)
    printf("%c", row[i]);
  printf("\n");
  
  free(g_suggest);
  free(g_prepare);
  free(row);
  free(nums);
  
  return 0;
}

int variate(int start, int end)
{
  int* rowcpyl = (int*)malloc(g_rowLen*sizeof(int));
  memcpy(rowcpyl, g_row, g_rowLen*sizeof(int));
  int* rowcpyr = (int*)malloc(g_rowLen*sizeof(int));
  memcpy(rowcpyr, g_row, g_rowLen*sizeof(int));
  
  int len = g_nl.nums[g_mainBlock];
  printf("[VARIATE] Block len: %d\n", len);
  
  int* row = g_row;
  g_row = rowcpyl;      
  int lres = SHIFTTOLEFT(len, start, end);
  g_row = rowcpyr;
  int rres = SHIFTTORIGHT(len, start, end);
  g_row = row;
  
  printf("original\n");
  for(int i=0; i<g_rowLen; i++)
    printf("%d ", row[i]);
  printf("\n");
  printf("left %d\n", lres);
  for(int i=0; i<g_rowLen; i++)
    printf("%d ", rowcpyl[i]);
  printf("\n");
  printf("right %d\n", rres);
  for(int i=0; i<g_rowLen; i++)
    printf("%d ", rowcpyr[i]);
  printf("\n");
  
  if(lres != VALID || rres != VALID)
    return INVALID;
 
  //input should be valid here
  
  int beginPos = -1;
 
  char unstable = 0; 
  for(int i=0; i<g_rowLen; i++)
  {
    unstable += BYTE1OF(rowcpyl[i]) == BYTE1OF(rowcpyr[i]) ? 0 : 1;
    if(BYTE1OF(rowcpyl[i]) == BYTE1OF(rowcpyr[i]) && BYTE1OF(rowcpyl[i]) != BYTE1OF(row[i]))
    {
      if(beginPos == -1)
        beginPos = i;      
      row[i] = BYTE0OF(row[i]) | TOBYTE1('X');
      changeNoticed++;      
    }
    else
      row[i] = BYTE0OF(row[i]);
  }
  
  printf("[VARIATE] Unstable: %d\n", unstable);
  printf("[VARIATE] Changes: %d\n", changeNoticed);
  
  if(unstable == 0)
  {
    if(beginPos > 0)
      row[beginPos - 1] |= TOBYTE1(' ');
    if(beginPos + len < g_rowLen)
      row[beginPos + len] |= TOBYTE1(' ');
  }
  
  free(rowcpyl);
  free(rowcpyr);
  
  return VALID;
}*/

/*int main(int argc, char* argv[]) //seems to work
{
  int* nums = (int*)malloc(10*sizeof(int));
  int* row = (int*)malloc(10*sizeof(int));
  int rowLen = 10;
  num_list nl;
  nl.length = 3;
  nl.nums = nums;
  nums[0] = 1;
  nums[1] = 2;
  nums[2] = 3;
  for(int i=0; i<rowLen; i++)
    row[i] = '?';
  row[0] = 'X';
  row[2] = '?' | 'X' << 8;
  row[3] = '?' | 'X' << 8;
  row[5] = '?' | 'X' << 8;
  row[6] = '?' | 'X' << 8;
  row[7] = '?' | 'X' << 8;
  
  int res = validateRow(nl, row, rowLen);
  printf("%d\n", res);
  
  free(row);
  free(nums);
  
  return 0;
}*/

/*void pushAway(int n, num_list nl, int* row, int rowLen)
{
  char valid = 0;
  for(int i=0; valid == 0 && i<n; i++) //schiebe block nach links
    valid += SHIFTTOLEFT(nl.nums[i], row, rowLen);
  for(int i=nl.length-1; valid == 0 && i>n; i--) //schiebe block nach rechts
    valid += SHIFTTORIGHT(nl.nums[i], row, rowLen);
  if(valid != 0)
  {
    printf("[PUSHAWAY] Unsolvable");
    exit(2);
  }
}

void insertAndMarkCrossing(int n, num_list nl, int* row, int rowLen)
{
  int* rowcpy1 = (int*)malloc(rowLen*sizeof(int));
  memcpy(rowcpy1, row, rowLen*sizeof(int));
  int* rowcpy2 = (int*)malloc(rowLen*sizeof(int));
  memcpy(rowcpy2, row, rowLen*sizeof(int));
  
  char valid = 0;
  valid += SHIFTTOLEFT(nl.nums[n], rowcpy1, rowLen);
  valid += SHIFTTORIGHT(nl.nums[n], rowcpy2, rowLen);
  if(valid != 0)
  {
    printf("[PUSHAWAY] Unsolvable");
    exit(2);
  }
  
  for(int i=0; i<rowLen; i++)
    if(rowcpy1[i] == rowcpy2[i] && rowcpy1[i] != row[i])
    {
      row[i] = 'X';
      changeNoticed++;      
    }
    else
      row[i] = row[i] == 'Y' ? '?' : row[i];
  
  //if(oc != changeNoticed)
 // {
   //   printf("++++++++++++++++++++++\n");
    //  for(int i=0; i<rowLen; i++)
    //    putchar(oldrow[i]);
    //  printf("\n");
    //  for(int i=0; i<rowLen; i++)
    //    putchar(rowcpy1[i]);
    //  printf("\n");
    //  for(int i=0; i<rowLen; i++)
    //    putchar(rowcpy2[i]);
    //  printf("\n");
    //  for(int i=0; i<rowLen; i++)
    //    putchar(row[i]);
    //  printf("\n");
    //  printf("++++++++++++++++++++++\n");
    //  getchar();
  //}
  
  free(rowcpy1); 
  free(rowcpy2); 
}*/



