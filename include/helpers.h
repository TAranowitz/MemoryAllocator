#ifndef HELPERS_H
#define HELPERS_H
#include <stdlib.h>
#include <stdint.h>
#include "icsmm.h"

/* Helper function declarations go here */
//ics_free_header *freelist_head
//returns the address the head of the freeListBlock
void * setUpHeap(int numPages);
void * firstFitBlock(int minBlockSize);
int getMinBlockSize(int size);
void * requestAndSetupBlock(int numPages);
int checkValidFreeAddress(void * startOfHeap, void * ptr);
void * getBlockFooter(void * header);
int isFreeBlock(void * header);
void removeBlockFromList(void * head);

void mergeAdjacentBlocks(void * block1, void * block2);
#endif
