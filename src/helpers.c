#include "helpers.h"
#include "icsmm.h"
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>


/* Helper function definitions go here */
void create_prologue_epilogue(void* init_brk){

  //prologue
  ics_header* temp = (ics_header*) init_brk; // cast
  temp->requested_size = 0;
  temp->unused = 0xaaaaaaaa;
  temp->block_size = 9;



  //epilogue
  char* temp2 = (char*) init_brk;
  temp2 += 4088; // go to the end -8
  ics_header* temp3 = (ics_header*) temp2;
  temp3->requested_size = 0;
  temp3->unused = 0xaaaaaaaa;
  temp3->block_size = 9;

}

ics_free_header* find_fit(size_t size, ics_free_header** freelist_next){
  //find the best fit given the size (next fit)
  //size includes header + footer
  ics_free_header* temp;
  ics_free_header* land_mark;
  //memory the original place
  temp = *freelist_next;
  land_mark = *freelist_next;
  while (temp != NULL){
    if (read_header_block_size(&((*freelist_next)->header)) >= size){
      //current block is able to hold the data
      return *freelist_next;
      // the next ptr fits
    }

    temp = temp -> next;
    *freelist_next = temp;
    if (temp == NULL){
      *freelist_next = freelist_head;
      temp = freelist_head;
      // move the next ptr to the head (1st time)
    }
    // the temp should reach the landmark before get to reset to head
    // if get stuck in the infinit loop here
    if (temp == land_mark){
      //going one round
      return NULL;
    }
  }
  return NULL;
  //not fit
}

uint64_t read_header_block_size(ics_header* header){
  //get the size of a block
  return (uint64_t)(header->block_size);
}
uint64_t read_footer_block_size(ics_footer* footer){
  // get block size info from a footer
  return (uint64_t)(footer->block_size);
}

uint64_t read_requested_size(ics_header header){
  //get the requested_size
  return (uint64_t)(header.requested_size);
}

int read_allocate(ics_header* header){
  // get allocated or NOT
  return (size_t)(header->block_size % 2);
}

void insert_to_free_list(ics_free_header** freelist_head, ics_free_header** freelist_next, ics_free_header* node){
  // insert to the front of list
  if (*freelist_head == NULL){
    // nothing in the list
    *freelist_head = node;
    *freelist_next = node;
  }
  else{
    ics_free_header* temp;
    temp = *freelist_head;
    temp->prev = node;
    node->prev = NULL;
    node->next = temp;
    *freelist_head = node;
  }
  // set the freelist head to the newly insert node

}

void remove_from_free_list(ics_free_header** freelist_head, ics_free_header** freelist_next, ics_free_header* node){
  // remove from the free freelist
  ics_free_header* temp;
  temp = *freelist_head;
  while (temp != NULL){
    if (temp == node){
      //removal
      if (temp->prev != NULL){
        // has a prev
        ics_free_header* previous_free;
        previous_free = temp->prev;
        previous_free->next = temp->next;
        break;
      }
      if (temp->next != NULL){
        // has a next
        ics_free_header* next_free;
        next_free = temp->next;
        next_free->prev = temp->prev;
        break;
      }
      // no prev or next
      *freelist_head = NULL;
      *freelist_next = NULL;

    }
    temp = temp->next;
  }
}

void* allocate(ics_free_header* free_block, uint64_t block_size, uint64_t requested_size, ics_free_header** freelist_head, ics_free_header** freelist_next){
  // allocate a specific size of block in a free block
  // should check if the free block is ok to fit beforehand
  ics_free_header* mem_next;
  mem_next = free_block->next;
  // memorize the next free block in case it got overwritten
  uint64_t block_size_of_current_free_block;
  block_size_of_current_free_block = read_header_block_size(&(free_block->header));
  //set_header(&(free_block->header), block_size + 1, requested_size);
  // set the allocated block's header, block size+1 to indicate it is allocated
  remove_from_free_list(freelist_head, freelist_next, free_block);
  // remove the current block from free list
  // check if it needs to split
  if (block_size <= block_size_of_current_free_block - 32){
    // needs to split
    set_header(&(free_block->header), block_size + 1, requested_size);
    set_footer(&(free_block->header), block_size + 1);
    // set the allocated footer in the middle
    ics_header* temp;
    temp = &(free_block->header);
    char* next_header;
    next_header = (char*)temp;
    next_header += block_size;
    // move the ptr to the starting address of next free block
    set_header((ics_header*)next_header, block_size_of_current_free_block - block_size, 0);
    // set header of the next free block
    set_footer((ics_header*)next_header, block_size_of_current_free_block - block_size);
    // set footer of the next free block
    ics_free_header* newly_splitted = (ics_free_header*) next_header;
    newly_splitted->header.block_size = block_size_of_current_free_block - block_size;
    newly_splitted->header.requested_size = 0;
    newly_splitted->header.unused = 0xaaaaaaaa;
    newly_splitted->prev = NULL;
    newly_splitted->next = NULL;
    // make a new ics_free_header
    insert_to_free_list(freelist_head, freelist_next, newly_splitted);
    // insert the splitted new free block to freelist
    if (mem_next == NULL){
      // put next ptr to the head
      *freelist_next = *freelist_head;
    }
    else{
      *freelist_next = mem_next;
    } // move the next ptr
  }
  else{
    // does not need to split
    set_header(&(free_block->header), block_size_of_current_free_block+1, requested_size);
    set_footer(&(free_block->header), block_size_of_current_free_block+1);
    if (mem_next != NULL){
      *freelist_next = mem_next;
    }
    else{
      *freelist_next = * freelist_head;
    }
  }
  // set the allocated block's footer, block size+1 to indicate it is allocated
  /* finish work with the allocated block */
  ics_header* temp;
  temp = &(free_block->header);
  char* payload;
  payload = (char*)temp;
  payload += 8;
  // move the temp to the start of payload
  return (void*)payload;
  // return the starting address of the payload
}

void set_header(ics_header* header, uint64_t block_size, uint64_t requested_size){
  // helper to set the header
  if (block_size % 2 == 0){
    // free block, should use the ics_free_header
    ics_free_header* temp = (ics_free_header*) header;
    temp->header.block_size = block_size;
    temp->header.requested_size = requested_size;
    temp->header.unused = 0xaaaaaaaa;
    temp->next = NULL;
    temp->prev = NULL;
  }
  else{
    header->block_size = block_size;
    header->requested_size = requested_size;
    header->unused = 0xaaaaaaaa;
  }
}
void set_footer(ics_header* header, uint64_t block_size){
  // helper to set the footer
  // notice that the given param is the header
  ics_footer* footer;
  char* temp;
  uint64_t ablock_size;
  //adjust block size to do ptr arithmtic
  temp = (char*)header;
  if (block_size % 2 == 1){
    // allocated, size is one more then the actual size;
    ablock_size = block_size - 1;
  }
  else{
    ablock_size = block_size;
    // not allocated
  }
  temp += (ablock_size - 8);
  // move to the starting address of footer
  footer = (ics_footer*) temp;
  //cast
  footer->block_size = block_size;
  footer->unused = 0xffffffffffff;
}

int create_new_page(ics_free_header** freelist_head, ics_free_header** freelist_next){
  //create new page in from ics_inc_brk
  // 1.set the page epilogue
  // 2. make the rest of it a whole free block
  void* brk;
  brk = ics_inc_brk();
  if (*(int*)brk == -1){
    //oops ENOMEM
    return -1;
  }
  brk = ics_get_brk() - 4096; // end of the old page (start of new page)
  create_prologue_epilogue(brk);
  // create epilogue and prologue (will not use it in this situation)
  char* old_epilogue = (char*) brk;
  old_epilogue -= 8;
  set_header((ics_header*) old_epilogue, 4096, 0);
  // overwrite the old epilogue with new free block's header
  set_footer((ics_header*) old_epilogue, 4096);
  // set the new free block's footer
  coalesce((ics_free_header*) old_epilogue, freelist_head, freelist_next);
  // coalescing
  *freelist_next = *freelist_head;
  // point the next ptr to the head according to the document
  return 0;
}


void coalesce(ics_free_header* freed_header, ics_free_header** freelist_head, ics_free_header** freelist_next){
  // perform coalescing
  // 4 cases in total
  int prev_alloc = 1;
  int next_alloc = 1;
  int new_block_size = freed_header->header.block_size;
  // set the prev and next alloc to 1 to prevent unintentional operation
  char* temp = (char*)(&(freed_header->header));
  temp -= 8;
  // find the prev footer
  ics_footer* prev_footer = (ics_footer*)temp;
  if ((prev_footer->block_size) % 2 == 0){
    // prev is free
    prev_alloc = 0;
  }
  temp = (char*)(&(freed_header->header));
  temp += freed_header->header.block_size;
  ics_header* next_header = (ics_header*)temp;
  // find the next head
  if ((next_header->block_size) % 2 == 0){
    // next is free
    next_alloc = 0;
  }

  /*-------------------finish checking the next and prev---------------------*/
  if (prev_alloc == 1 && next_alloc == 1){
    // case #1 -both allocated
    // do nothing
  }
  else if (prev_alloc == 1 && next_alloc == 0){
    // case #2 -next is free
    // coalesce with the next block
    if (*freelist_next == (ics_free_header*)next_header){
      // next ptr is pointing to the coalescing block
      *freelist_next = *freelist_head;
    }
    ics_free_header* freeheadlocal = (ics_free_header*) next_header;
    new_block_size += read_header_block_size(&(freeheadlocal->header));
    //
    remove_from_free_list(freelist_head, freelist_next, freeheadlocal);
    //remove from the free list
    set_header((ics_header*)freed_header, new_block_size, 0);
    set_footer((ics_header*)freed_header, new_block_size);
    insert_to_free_list(freelist_head, freelist_next, freed_header);
    // insert new block back to free list
    // done modifying
  }
  else if (prev_alloc == 0 && next_alloc == 1){
    // case #3 -prev is free
    // coalesce with prev block
    new_block_size += read_footer_block_size((ics_footer*)prev_footer);
    char* prev_header = (char*) freed_header;
    prev_header -= read_footer_block_size((ics_footer*)prev_footer);
    if (*freelist_next == (ics_free_header*)prev_header){
      // next ptr is pointing to the coalescing block
      *freelist_next = *freelist_head;
    }
    remove_from_free_list(freelist_head, freelist_next, (ics_free_header*)prev_header);
    // remove from the freelist
    set_header((ics_header*) prev_header, new_block_size, 0);
    set_footer((ics_header*) prev_header, new_block_size);
    insert_to_free_list(freelist_head, freelist_next, (ics_free_header*)prev_header);
    // insert_to_free_list
  }
  else{
    // case #4 -both are free
    // coalesce with both next and prev
    new_block_size += read_header_block_size(&(((ics_free_header*)next_header)->header))
                      + read_footer_block_size((ics_footer*)prev_footer);
    char* prev_header = (char*)freed_header;
    prev_header -= read_footer_block_size((ics_footer*)prev_footer);
    if (*freelist_next == (ics_free_header*)prev_header || *freelist_next == (ics_free_header*)next_header){
      // next ptr is pointing to the coalescing block
      *freelist_next = *freelist_head;
    }
    remove_from_free_list(freelist_head, freelist_next, (ics_free_header*)next_header);
    remove_from_free_list(freelist_head, freelist_next, (ics_free_header*)prev_header);
    set_header((ics_header*) prev_header, new_block_size, 0);
    set_footer((ics_header*) prev_header, new_block_size);
    insert_to_free_list(freelist_head, freelist_next,(ics_free_header*) prev_header);
  }

}

int legal_free(void* header, void* heap_start, int pageCount){
  //check the given block is legal to free
  char* temp1byte;
  // 1.check in heap space
  if (header < heap_start || header > heap_start + pageCount*4096){
    // not in heap space
    return -1;
  }
  temp1byte = header - 8;
  // move from the payload to header

  // 2. check the unused field of the pointer
  ics_header* temp_head = (ics_header*) temp1byte;
  if(temp_head->unused != 0xaaaaaaaa){
    // check Failed, not a legit header
    return -2;
  }
  // it is a legit header
  // 3.check the unused field of the ptr's footer
  char* temp1 = (char*) temp_head;
  temp1 += read_header_block_size(temp_head) - 8;
  if (temp_head->block_size % 2 == 1){
    // if allocated, the size is one more than the actual_value
    temp1 --;
  }
  // move to the start of footer
  if ((void*)temp1byte < heap_start || (void*)temp1byte > heap_start + pageCount*4096){
    // not in heap space after move to footer
    return -1;
  }
  ics_footer* temp_footer = (ics_footer*) temp1;
  if (temp_footer->unused != 0xffffffffffff){
    // check failed, does not has a legit footer
    return -3;
  }
  // 4.check block size of header and footer is equal
  if(temp_footer->block_size != temp_head->block_size){
    // check failed, block size in footer and header does not match
    return -4;
  }
  // 5.check the requested_size is less than the block_size
  if(temp_head->requested_size >= read_header_block_size(temp_head)){
    // check fails, the requested_size is larger or equal to the block size
    // which is impossible if everything is right
    return -5;
  }
  // 6.check the allocated bit is set to 1 in both header and footer
  if (temp_head->block_size % 2 == 0 || temp_footer->block_size % 2 == 0){
    // check fails, the allocate bit is not all set to 1
    return -6;
  }
  return 0;
}

void mycpy(void* dest, void* src, size_t num){
  // my own version of memcpy
  char* csrc = (char*) src;
  char* cdest = (char*) dest;
  int i;
  for (i = 0; i < num; i++){
    cdest[i] = csrc[i];
  }
}
