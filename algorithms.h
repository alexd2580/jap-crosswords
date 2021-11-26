#ifndef ALGORITHMS_H___
#define ALGORITHMS_H___

#define VALID 0
#define INVALID 1
#define FUTILE 2

#define SPACE (-1) //for prepare

#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define SOLVED 1
#define UNSOLVED 0

typedef struct num_list_ num_list;
struct num_list_
{
  int length;
  int* nums;
  int* minStarts;
  int* maxStarts;
  char solved;
};

typedef struct data data;
struct data
{
  num_list  *        nl;
  char      * const  row;
  char      * const  prepare;
  char      * const  suggest;
};

void combinatoricalForce(data const * const d);
void bruteForce(data const * const d);

extern int changeNoticed;
extern int rowSkipped;
extern long skipTreshold;

extern int g_rowLen;

#endif
