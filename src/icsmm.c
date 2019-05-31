/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 * If you want to make helper functions, put them in helpers.c
 */
#include "icsmm.h"
#include "helpers.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#define BLOCKALIGNMENT 16

ics_free_header *freelist_head = NULL;
ics_free_header *freelist_next = NULL;
void* brk = NULL;
void* heap_start = NULL;
int pageCount = 0;

void *ics_malloc(size_t size) {
  size_t asize = size; //adjusted block size
  //size_t extendsize = 0; //amount to extend head if not fit
  if (size <= 0){
    strerror(EINVAL);
    return NULL;
  }
  if (size > 4096*4){
    //exceed the maximum 4 page limit
    strerror(ENOMEM);
    return NULL;
  }
  if (freelist_head == NULL){
    // first time run ics_malloc
    //incbrk
    heap_start = ics_inc_brk();
    brk = heap_start;
    pageCount++;
    if (*(int*)brk == -1){
      //oops ENOMEM
      strerror(ENOMEM);
      return NULL;
    }
    //brk = ics_get_brk()-4096;
    create_prologue_epilogue(brk);
    //create prologue and epilogue
    ics_free_header* header;
    char* temp = brk;
    temp += 8;
    header = (ics_free_header*)temp;
    header->header.block_size = 4080;
    header->header.unused = 0xaaaaaaaa;
    header->header.requested_size = 0;
    header->prev = NULL;
    header->next = NULL;
    freelist_head = header;
    freelist_next = header;
    temp += 4072;
    ics_footer* footer;
    footer = (ics_footer*) temp;
    footer->block_size = 4080;
    footer->unused = 0xffffffffffff;
    //place the first free block (of size 4096-16 = 4080byte)
  }

  if (size <= BLOCKALIGNMENT){
    asize = BLOCKALIGNMENT * 2;
  }
  else{
    asize = BLOCKALIGNMENT * ((size + (BLOCKALIGNMENT) + (BLOCKALIGNMENT - 1)) / BLOCKALIGNMENT);
  }// adjust size
  ics_free_header* fit_free_block;

  fit_free_block = find_fit(asize, &freelist_next);

  while (fit_free_block == NULL){
    if (pageCount >= 4){
      //exceeds the maximum 4 page limit
      strerror(ENOMEM);
      return NULL;
    }
    //needs to sbrk
    //1. create_new_page (inside it will do coalesce)
    if (create_new_page(&freelist_head, &freelist_next) < 0){
      // fail inc brk
      strerror(ENOMEM);
      return NULL;
    }
    pageCount++;
    fit_free_block = find_fit(asize, &freelist_next);
  }
  void* payload_add;

  payload_add = allocate(fit_free_block, asize, size, &freelist_head, &freelist_next);
  return payload_add;
}

void *ics_realloc(void *ptr, size_t size) {
  if (legal_free(ptr, heap_start, pageCount)<0){
    // ptr illegal
    strerror(EINVAL);
    return NULL;
  }
  if (size == 0){
    // ptr valid, size 0, free ptr and return NULL
    ics_free(ptr);
    return NULL;
  }
  void* newspace = ics_malloc(size);
  // create a new space
  if (newspace == NULL){
    // ics_malloc failed and NOEMEM
    strerror(ENOMEM);
    return NULL;
  }
  mycpy(newspace, ptr, size);
  // memcpy
  ics_free(ptr);
  //free the old space
  return newspace;
}

int ics_free(void *ptr) {
  int r = legal_free(ptr, heap_start, pageCount);
  if (r < 0){
    // something goes wrong
    strerror(EINVAL);
    return -1;
  }
  char* temp1byte = (char*)ptr - 8;
  // move from payload to header
  ics_free_header* temp_free_header = (ics_free_header*) temp1byte;
  temp_free_header->header.block_size--;
  // decrement block size by 1
  temp1byte = (char*)temp_free_header + read_header_block_size(&(temp_free_header->header)) - 8;
  ics_footer* temp_footer = (ics_footer*)temp1byte;
  // footer of the block
  temp_footer->block_size--;
  // decrement block size by 1
  insert_to_free_list(&freelist_head, &freelist_next, temp_free_header);
  // insert to freelist
  coalesce(temp_free_header, &freelist_head, &freelist_next);
  //done working
  return 0;
}
