#include "helpers.h"
#include "debug.h"
#include "icsmm.h"


/* Helper function definitions go here */
void * setUpHeap(int numPages)
{
  void * startOfHeap = ics_inc_brk(numPages);
  void * endOfHeap = ics_get_brk();

  //setup the prologue
  ((ics_footer *) startOfHeap) -> block_size = 1;
  ((ics_footer *) startOfHeap) -> fid = PROLOGUE_MAGIC;
  

  //setup the epilogue
  ((ics_header *) (((char *) endOfHeap) - 8)) -> block_size = 1;
  ((ics_header *) (((char *) endOfHeap) - 8)) -> hid = EPILOGUE_MAGIC;
  ((ics_header *) (((char *) endOfHeap) - 8)) -> padding_amount = 0;

  void * startOfFree = (((char*) startOfHeap)+8);
  void * endOfFree = (((char *) endOfHeap) - 16);

  //setup freeBlock header
  ((ics_free_header *) startOfFree)->next = NULL;
  ((ics_free_header *) startOfFree) -> prev = NULL;
  ((ics_free_header *) startOfFree) -> header.block_size = (numPages*4096)-16;
  ((ics_free_header *) startOfFree) -> header.hid = HEADER_MAGIC;
  ((ics_free_header *) startOfFree) -> header.padding_amount = 0;

  //setup freeBlock footer
  ((ics_footer *) endOfFree) ->block_size = (numPages*4096)-16;
  ((ics_footer *) endOfFree) ->fid = FOOTER_MAGIC;



  //printf("%d dsadasfooter block size\n",((ics_footer *) getBlockFooter(startOfFree)) ->block_size);


  return startOfFree;
  
}

//returns the pointer of the block it just created
void * requestAndSetupBlock(int numPages)
{
  void * endOldHeap = ics_inc_brk(numPages);
  void * endOfHeap = ics_get_brk();

  //setup the epilogue
  ((ics_header *) (((char *) endOfHeap) - 8)) -> block_size = 1;
  ((ics_header *) (((char *) endOfHeap) - 8)) -> hid = EPILOGUE_MAGIC;
  ((ics_header *) (((char *) endOfHeap) - 8)) -> padding_amount = 0;

  void * startOfFree = (((char*) endOldHeap)-8);
  void * endOfFree = (((char *) endOfHeap) - 16);

  //setup freeBlock header
  ((ics_free_header *) startOfFree)->next = NULL;
  ((ics_free_header *) startOfFree) -> prev = NULL;
  ((ics_free_header *) startOfFree) -> header.block_size = (numPages*4096);
  ((ics_free_header *) startOfFree) -> header.hid = HEADER_MAGIC;
  ((ics_free_header *) startOfFree) -> header.padding_amount = 0;

  //setup freeBlock footer
  ((ics_footer *) endOfFree) ->block_size = (numPages*4096);
  ((ics_footer *) endOfFree) ->fid = FOOTER_MAGIC;


  
  return startOfFree;
}










int getMinBlockSize(int size)
{
  if(size <= 16)
  {
    return 32;
  }
  int mod = (size%16);
  int padding = 0;
  if(mod != 0)
  {
    padding = 16 - mod;
  }
  return size + padding + 16;
}
void * firstFitBlock(int minBlockSize)
{
  void * curr = freelist_head;
  
  while(curr!= NULL)
  {
    if(((ics_free_header *) curr)->header.block_size >= minBlockSize)
    {
      removeBlockFromList(curr);
      return curr;
      /*
      //if curr is HEAD block
      if(((ics_free_header *) curr)->prev == NULL)
      {
        //make freeListHead = next of curr
        freelist_head = ((ics_free_header *) curr) ->next;
        
        //if next of curr isnt null make its prev NULL so that we can detect its head
        if(((ics_free_header *) curr) ->next != NULL)
        {
          (((ics_free_header *) curr) ->next)->prev = NULL;
        }
        return curr;
      }
      */
    }
    curr = ((ics_free_header *) curr)->next;
    
  }
  return NULL;
  
 /*
  while(curr!= NULL)
  {
    if(((ics_free_header *) curr)->header.block_size >= minBlockSize)
    {
      //if curr is HEAD block
      if(((ics_free_header *) curr)->prev == NULL)
      {
        //make freeListHead = next of curr
        freelist_head = ((ics_free_header *) curr) ->next;
        
        //if next of curr isnt null make its prev NULL so that we can detect its head
        if(((ics_free_header *) curr) ->next != NULL)
        {
          (((ics_free_header *) curr) ->next)->prev = NULL;
        }
        return curr;
      }
    }
    curr = ((ics_free_header *) curr)->next;
  }
  return NULL;
  */
}
 


//returns 0 if false or 1 if true
int checkValidFreeAddress(void * startOfHeap, void * ptr)
{
  void * header = ptr - 8;
  void * epilogue = ics_get_brk() - 8;
  int size = ((ics_header *)header)->block_size - 1;
  void * footer = getBlockFooter(header);
  
  if(header < (void *)(((char *) startOfHeap)+8) || header >= epilogue)
  {
    return 0;
  }
  if( ((ics_header *)header) ->block_size %2 == 0)
  {
    return 0;
  }
  if(((ics_header *)header)->hid !=HEADER_MAGIC)
  { 
    return 0;
  }
  if(((ics_footer *) footer)->fid != FOOTER_MAGIC)
  {
    return 0;
  }
  if(((ics_footer *) footer)->block_size != ((ics_header *)header) ->block_size)
  {
    return 0;
  }

  return 1;
}

void * getBlockFooter(void * header)
{
  int isAllocated = (((ics_header *) header)->block_size) %2;
  int increment = (((ics_header *) header)->block_size) - isAllocated - 8;
  void * footer = ((char *) header) +increment;
  
  return footer;
}

int isFreeBlock(void * header)
{
  //first check if it is a valid header
  if(((ics_header *)(header))->hid != HEADER_MAGIC)
  {
    return -1;
  }


  //second check if it is free
  if(((ics_header *)(header))->block_size %2 == 0)
  {
    return 1;
  }
  else
  {return -1;}
}

void removeBlockFromList(void * head)
{
  //void * footer = getBlockFooter(head);

  //if head is the head of the freeList
  if(((ics_free_header * )(head))->prev == NULL)
  {
    //make next the head
    freelist_head = ((ics_free_header * )(head))->next;
    if((((ics_free_header * )(head))->next) != NULL)
    {
      (((ics_free_header * )(head))->next)->prev = NULL;
    }
  }
  else
  {
    (((ics_free_header * )(head))->prev)->next = ((ics_free_header * )(head))->next;
    if((((ics_free_header * )(head))->next) != NULL)
    {
      (((ics_free_header * )(head))->next)->prev = ((ics_free_header * )(head))->prev;
    }
  }

}
void mergeAdjacentBlocks(void * block1, void * block2)
{
  //first make the size = sum of block one vs some of block 2
  int isAllocated = (((ics_header *) block1)->block_size) %2;
  int block1Size = (((ics_header *) block1)->block_size) - isAllocated;

  void * block2Footer = getBlockFooter(block2);
  int block2Size = ((ics_footer *)(block2Footer))->block_size;

  int newSize = block1Size + block2Size;
  (((ics_header *) block1)->block_size) = newSize + 1;
  ((ics_footer *)(block2Footer)) ->block_size = newSize +1;
  
}