#ifndef HELPERS_H
#define HELPERS_H
#include <stdlib.h>
#include "icsmm.h"

/* Helper function declarations go here */
void create_prologue_epilogue(void* init_brk);
ics_free_header* find_fit(size_t size, ics_free_header** freelist_head);
uint64_t read_header_block_size(ics_header* header);
uint64_t read_footer_block_size(ics_footer* footer);
uint64_t read_requested_size(ics_header header);
int read_allocate(ics_header* header);
void insert_to_free_list(ics_free_header** freelist_head, ics_free_header** freelist_next,ics_free_header* node);
void remove_from_free_list(ics_free_header** freelist_head, ics_free_header** freelist_next,ics_free_header* node);
void* allocate(ics_free_header* free_block, uint64_t block_size, uint64_t requested_size, ics_free_header** freelist_head, ics_free_header** freelist_next);
void set_header(ics_header* header, uint64_t block_size, uint64_t requested_size);
void set_footer(ics_header* header, uint64_t block_size);
int create_new_page(ics_free_header** freelist_head, ics_free_header** freelist_next);
void coalesce(ics_free_header* freed_header, ics_free_header** freelist_head, ics_free_header** freelist_next);
int legal_free(void* header, void* heap_start, int pageCount);
void mycpy(void* dest, void* src, size_t num);
#endif
