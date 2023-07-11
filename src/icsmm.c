#include "icsmm.h"
#include "debug.h"
#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>

int alreadyAlloc = 0;
int pagesAllocated = 0;
/*
 * The allocator MUST store the head of its free list in this variable. 
 * Doing so will make it accessible via the extern keyword.
 * This will allow ics_freelist_print to access the value from a different file.
 */
ics_free_header *freelist_head = NULL;
void * startOfHeap;
/*
 * This is your implementation of malloc. It acquires uninitialized memory from  
 * ics_inc_brk() that is 16-byte aligned, as needed.
 *
 * @param size The number of bytes requested to be allocated.
 *
 * @return If successful, the pointer to a valid region of memory of at least the
 * requested size is returned. Otherwise, NULL is returned and errno is set to 
 * ENOMEM - representing failure to allocate space for the request.
 * 
 * If size is 0, then NULL is returned and errno is set to EINVAL - representing
 * an invalid request.
 */
void *ics_malloc(size_t size) {
  //if firstCall to malloc we must set up heap and add block to freeList
  if(alreadyAlloc == 0)
  {
    startOfHeap = ics_get_brk();
    //+32 becayse prologue, epilogue header and footer
    int bytesNeeded = size +32;
    alreadyAlloc = 1;
    int pageNum = bytesNeeded / 4096;
    if(pageNum * 4096 < bytesNeeded)
    {
      pageNum++;
    }
    
    
    if(pageNum >6)
    {
      errno = ENOMEM;
      return NULL;
    }
    freelist_head = setUpHeap(pageNum);
    pagesAllocated += pageNum;

    
    
  }

  //get minBlockSize we are looking for
  int minBlockSize = getMinBlockSize(size);
  //printf("****************minblockSizeIS: %d\n", minBlockSize);
  //gets the first block that meets minBlockSize requirement and remove it from list
  void * freeBlock = firstFitBlock(minBlockSize);

  //if freeBlock is null request more space
  if(freeBlock == NULL)
  {
    int bytesNeeded = size + 16;
    int pageNum = bytesNeeded / 4096;
    if(pageNum * 4096 < bytesNeeded)
    {
      pageNum++;
    }
    if(pagesAllocated+pageNum >6)
    {
      errno = ENOMEM;
      return NULL;
    }
    pagesAllocated += pageNum;
    
    void * newBlock = requestAndSetupBlock(pageNum);
    if(freelist_head == NULL)
    {
      freelist_head = newBlock;
    }
    else
    {
      freelist_head->prev = newBlock;
      ((ics_free_header *)newBlock)->next = freelist_head;
      freelist_head = newBlock;
    }
    freeBlock = firstFitBlock(minBlockSize);
  }
  //printf("freeblock size %d\n", ((ics_header *) freeBlock) ->block_size);

  //if the block size is not big enough to make a subblock
  if ((((ics_free_header *) freeBlock) -> header.block_size) - minBlockSize < 32)
  {
    
    void * footerPointer = getBlockFooter(freeBlock);
    //printf("%d footer size\n", ((ics_footer *) footerPointer)->block_size);
    //printf("%d footer size\n", ((ics_footer *) footerPointer) ->block_size);
    
    ((ics_header *) freeBlock) -> padding_amount = (((ics_header *) freeBlock) ->block_size) - 16 - size;
    ((ics_header *) freeBlock) ->block_size += 1;
    //update the footer as well
    ((ics_footer *) footerPointer) ->block_size +=1;
    return ((char *)freeBlock)+ 8;
  }
  else //use minimum block size and make new block out of rest
  {
    void * freeFooterPointer = ((char *)freeBlock) + ((((ics_header *) freeBlock) ->block_size)-8);
    void * allocatedFooterPointer = ((char *)freeBlock) + minBlockSize - 8;
    void * newFreeBlockHeader = ((char *)freeBlock) + minBlockSize;

    int remainderBlockSize = (((ics_header *) freeBlock) ->block_size) - minBlockSize;
    //update header of frontblock
    ((ics_header *) freeBlock) -> block_size = minBlockSize + 1;
    ((ics_header *) freeBlock) -> padding_amount = (((ics_header *) freeBlock) ->block_size) - 17 - size;
    //update the footer of frontblock
    ((ics_footer *) allocatedFooterPointer)->block_size = minBlockSize +1;
    ((ics_footer *) allocatedFooterPointer)->fid = FOOTER_MAGIC;

    //update newFreeBlock
    ((ics_free_header *)newFreeBlockHeader)-> header.block_size = remainderBlockSize;
    ((ics_footer *) freeFooterPointer)->block_size = remainderBlockSize;
    ((ics_free_header *)newFreeBlockHeader)->header.padding_amount = 0;
    ((ics_free_header *)newFreeBlockHeader)->header.hid = HEADER_MAGIC;
    ((ics_free_header *)newFreeBlockHeader) ->next = freelist_head;
    ((ics_free_header *)newFreeBlockHeader) ->prev = NULL;

    if(freelist_head != NULL)
    {
      freelist_head->prev = newFreeBlockHeader;
    }
    freelist_head = newFreeBlockHeader;

    
    return ((char *)freeBlock)+ 8;
  }
  

  //get from freeListHeader the block that satisfies our size requirement nd remove fromm free list.
  return NULL;
}

/*
 * Marks a dynamically allocated block as no longer in use and coalesces with 
 * adjacent free blocks (as specified by Homework Document). 
 * Adds the block to the appropriate bucket according to the block placement policy.
 *
 * @param ptr Address of dynamically allocated memory returned by the function
 * ics_malloc.
 * 
 * @return 0 upon success, -1 if error and set errno accordingly.
 */
int ics_free(void *ptr)
{
  //check if valid address
  if(alreadyAlloc == 0 || checkValidFreeAddress(startOfHeap, ptr)!=1)
  {
    
    errno = EINVAL;
    return -1;
  }
  void * header = ptr - 8;
  void * epilogue = ics_get_brk() - 8;
  int size = ((ics_header *)header)->block_size - 1;
  void * footer = getBlockFooter(header);
  void * nextHeader = footer + 8;

  //second check if the next block is a block and not a epilogue and  if it is free
  if(isFreeBlock(nextHeader) == 1)
  {
    //remove block from freelist
    removeBlockFromList(nextHeader);
    mergeAdjacentBlocks(header, nextHeader);
    //set footer to the new footer
    footer = getBlockFooter(header);

  }
  //set the block last bit to 0
  ((ics_header *)header)->block_size -= 1;
  ((ics_footer *)(footer)) ->block_size -=1;

  //insert at the head of list
  if(freelist_head!= NULL)
  {
    freelist_head->prev = header;
  }
  ((ics_free_header *) (header))->next = freelist_head;
  freelist_head = header;
  return 0;

  

  
}

/********************** EXTRA CREDIT ***************************************/

/*
 * Resizes the dynamically allocated memory, pointed to by ptr, to at least size 
 * bytes. See Homework Document for specific description.
 *
 * @param ptr Address of the previously allocated memory region.
 * @param size The minimum size to resize the allocated memory to.
 * @return If successful, the pointer to the block of allocated memory is
 * returned. Else, NULL is returned and errno is set appropriately.
 *
 * If there is no memory available ics_malloc will set errno to ENOMEM. 
 *
 * If ics_realloc is called with an invalid pointer, set errno to EINVAL. See ics_free
 * for more details.
 *
 * If ics_realloc is called with a valid pointer and a size of 0, the allocated     
 * block is free'd and return NULL.
 */
void *ics_realloc(void *ptr, size_t size) {
    return NULL;
}
