#include"algorithms.h"
#include<stdio.h>
#include<stdlib.h>

/* ############################################################ */
/* ######################## BRUTEFORCE ######################## */

/**
 * check neighbour positions.
 */
__attribute__((pure)) char piecePlaceable(int pos, int len, data const * const d)
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
      streak++;

      //assuming short circuit here POTENTIAL BUG SOURCE
      if(d->row[i] == ' ' || pc >= nl->length || streak > nl->nums[pc])
        return INVALID;
    }
    else if(d->prepare[i] == SPACE)
    {
      if(d->row[i] == 'X')
        return INVALID;
      
      if(streak != 0)
      {
        if(pc >= nl->length || streak < nl->nums[pc]) //and here
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
    
  /* Looks like the configuration is valid */
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
  }
  
  return futile;
}

char bruteForce_(data const * const, int, int, int);
void bruteForce(data const * const d)
{
  long availCombinations = 1;
  for(int i=0; i<d->nl->length; i++)
    availCombinations *= 1 + d->nl->maxStarts[i] - d->nl->minStarts[i];

  if(availCombinations == 1)
  {
    d->nl->solved = SOLVED;
    // POTENTIAL BUG SOURCE (assuming that undef blocks are space here)
    for(int i=0; i<g_rowLen; i++)
      d->row[i] = d->row[i] == 'X' ? 'X' : ' ';
    
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
  
  if(bruteForce_(d, 0, 0, g_rowLen) != FUTILE)
    mergeDown(d);
}

char bruteForce_(data const * const d, int cblock, int start, int end)
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
  int len = nl->nums[cblock];
  int minstart = nl->minStarts[cblock];
  int maxstart = nl->maxStarts[cblock];
  
  int restLen = 0;
  for(int b=cblock+1; b<nl->length; b++)
    restLen += nl->nums[b] + 1;
  
  int pos=MAX(minstart,start);
  for(int i=pos; i<pos+len-1; i++)
    d->prepare[i] = (char)cblock;
  
  while(pos<MIN(maxstart+len,end))
  {
    if(pos+len+restLen > end)
      break;
      
    //put last block
    d->prepare[pos+len-1] = (char)cblock;

    if(piecePlaceable(pos, len, d) == VALID)
      if(bruteForce_(d, cblock+1, pos+len+1, end) == FUTILE)
        //d->prepare is garbage now and will be ignored
        return FUTILE;

    //remove first
    d->prepare[pos] = SPACE;
    pos++;
  }
  
  //cleanup
  for(int i=pos; i<pos+len-1; i++)
    d->prepare[i] = SPACE;
  
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
char computePieceBounds_(data const * const, int, int, int*);
void computePieceBounds(data const * const d)
{
  for(int i=0; i<g_rowLen; i++)
    d->prepare[i] = SPACE;
  int end = g_rowLen;
  
  computePieceBounds_(d, 0, 0, &end);
  variateSingleBlocks(d);
}

char computePieceBounds_(data const * const d, int curBlock, int start, int* end)
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
  
  int len = nl->nums[curBlock];
  int restLen = 0;
  for(int b=curBlock+1; b<nl->length; b++)
    restLen += 1 + nl->nums[b];
  
  if(start+restLen > *end)
    return INVALID;

  int curPos = start;
  int curEnd;
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
    d->prepare[curPos+i] = (char)curBlock;
  
  /** Try next piece. Content of end is modified after this call **/
  computePieceBounds_(d, curBlock + 1, curPos + len + 1, end);
  
  /** Rollback **/
  for(int i=0; i<len; i++)
    d->prepare[curPos+i] = SPACE;
  
  int lastValidPos = curPos;
  
  curEnd = curPos + len;
  d->prepare[curPos] = SPACE;
  d->prepare[curEnd] = (char)curBlock;   
  curPos++;
  curEnd++;
  
  int e = *end;
  while(curEnd <= e)
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
  
  int streakPos;
  int streakLen;
  
  int probablyBlock;

  //for each active pixel
  for(int streakIter=0; streakIter < g_rowLen; streakIter++)
    //for each streak
    if(row[streakIter] == 'X')
    {
      streakPos = streakIter;
      while(row[streakIter] == 'X' && streakIter < g_rowLen)
        streakIter++;
      streakLen = streakIter - streakPos;
      
      //search matching block. if multiple blocks found -> abort
      probablyBlock = -1;
      for(int block=0; block<nl->length; block++)
        if
        (
          nl->minStarts[block] <= streakPos && // if minstarts is on the left
          nl->maxStarts[block] + nl->nums[block] >= streakPos + streakLen // and endpos on the right
        )
        {
          if(probablyBlock != -1) // ambiguous, next streak
          {
            probablyBlock = -2;
            break;
          }
          probablyBlock = block;
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
        printf("Streak of len %d at pos %d not matched by any block.\n", streakLen, streakPos);
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
        
        changeNoticed += nl->minStarts[b] != newMi || nl->maxStarts[b] != newMa ? 1 : 0;
        
        nl->minStarts[b] = newMi;
        nl->maxStarts[b] = newMa;
      }      
    }
}

/* ####################### FIXED BLOCKS ####################### */
/* ############################################################ */

void combinatoricalForce(data const * const d)
{
  computePieceBounds(d);
  fixBlocks(d);
}
