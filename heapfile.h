#ifndef HEAPFILE_H
#define HEAPFILE_H

#include "mosken.h"
#include <stdio.h>

typedef struct HeapFile {
    FILE *fp;
} HeapFile;

HeapFile *hf_open(const char *path);
void hf_close(HeapFile *hf);
uint32_t hf_num_pages(HeapFile *hf);
void hf_extend(HeapFile *hf);
void hf_read_page(HeapFile *hf, PageId pg_id, Page pg_data);
void hf_write_page(HeapFile *hf, PageId pg_id, Page pg_data);

#endif // HEAPFILE_H
