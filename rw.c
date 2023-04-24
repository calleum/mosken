#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int i;
    unsigned long k;
    unsigned long j;
} MyType;

void readfile(char *name, int object_num) {
    printf("Reading from file [%s]\n", name);
    long offset = sizeof(MyType) * (object_num - 1);
    size_t ret;
    FILE *file = fopen(name, "rb");
    if (file == NULL) {
        fprintf(stderr, "fopen() failed.\n");
        exit(EXIT_FAILURE);
    }

    if (offset > 0)
        fseek(file, offset, SEEK_SET);
    MyType *t = malloc(sizeof(MyType));
    ret = fread(t, sizeof(MyType), 1, file);
    if (ret != 1) {
        perror("fread() failed");
        exit(EXIT_FAILURE);
    }

    printf("i: [%d], k: [%zu], j: [%zu]\n", t->i, t->k, t->j);
    free(t);
    fclose(file);
}

void writefile(char *name, int object_num) {
    printf("Writing to file [%s]\n", name);
    MyType arr[object_num];
    FILE *file = fopen(name, "wb");
    int idx;
    for (idx = 0; idx < object_num; idx++) {
        arr[idx].i = 44 * (idx + 1);
        arr[idx].k = 12L * (idx + 1);
        arr[idx].j = 11L * (idx + 1);
    }
    if (file == NULL) {
        fprintf(stderr, "fopen() failed.\n");
        exit(EXIT_FAILURE);
    }
    fwrite(arr, sizeof(MyType) * object_num, object_num, file);
    fclose(file);
}

int main(void)
{
   char *name = "rw_test.bin";
   writefile(name, 20);
   readfile(name, 1);
   readfile(name, 10); 

   return EXIT_SUCCESS;
}
