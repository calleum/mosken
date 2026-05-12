#include "heapfile.h"
#include <stdlib.h>
#include <sys/stat.h>

HeapFile *hf_open(const char *path)
{
    struct stat buffer;
    if (stat(path, &buffer) != 0) {
        FILE *fp = fopen(path, "w");
        if (fp) fclose(fp);
    }

    FILE *fp = fopen(path, "r+");
    if (!fp) return NULL;

    HeapFile *hf = malloc(sizeof(HeapFile));
    hf->fp = fp;
    return hf;
}

void hf_close(HeapFile *hf)
{
    if (hf) {
        fclose(hf->fp);
        free(hf);
    }
}

uint32_t hf_num_pages(HeapFile *hf)
{
    fseek(hf->fp, 0L, SEEK_END);
    long file_sz = ftell(hf->fp);
    return (uint32_t)(file_sz / BLKSZ);
}

void hf_extend(HeapFile *hf)
{
    Page page = malloc(BLKSZ);
    page_init(page);
    fseek(hf->fp, 0L, SEEK_END);
    fwrite(page, BLKSZ, 1, hf->fp);
    fflush(hf->fp);
    free(page);
}

static int check_file_offset_bound(HeapFile *hf, long offset)
{
    fseek(hf->fp, 0L, SEEK_END);
    long file_sz = ftell(hf->fp);

    if (offset > file_sz) {
        fprintf(stderr, "Offset of the page is invalid.\n");
        return -1;
    }

    return 1;
}

void hf_read_page(HeapFile *hf, PageId pg_id, Page pg_data)
{
    long offset = (long)pg_id * BLKSZ;

    if (check_file_offset_bound(hf, offset) == -1)
        return;

    fseek(hf->fp, offset, SEEK_SET);
    if (ferror(hf->fp))
        perror("Error seeking to offset in page file");

    fread(pg_data, BLKSZ, 1, hf->fp);

    if (ferror(hf->fp))
        perror("Error reading page from file");
}

void hf_write_page(HeapFile *hf, PageId pg_id, Page pg_data)
{
    long offset = (long)pg_id * BLKSZ;

    if (check_file_offset_bound(hf, offset) == -1)
        return;

    fseek(hf->fp, offset, SEEK_SET);
    if (ferror(hf->fp))
        perror("Error seeking to the offset in the page file");

    fwrite(pg_data, BLKSZ, 1, hf->fp);

    if (ferror(hf->fp))
        perror("Error writing page to file");
}
