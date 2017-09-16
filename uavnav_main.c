#include "tsip_decode.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

unsigned char * g_test_data = NULL;
uint32_t g_test_data_len;
uint32_t g_test_data_start;
bool g_verbose_output;



void test_data_load(const char * fname){
    FILE * f;
        f = fopen(fname, "rb");
        if(f == NULL){
            printf("File %s not found\n",fname);
            return;
        } else {
            printf("Loaded file %s\n",fname);
        }
    uint8_t num = 0;
    uint32_t size = 0;
    // Get the file size
    while(fread(&num,sizeof(num),1,f) > 0){
        size++;
    }
    rewind(f);
    // Load the data into memory
    g_test_data_len = size;
    if(g_test_data == NULL){
        g_test_data = (unsigned char*) malloc(size);
    } else {
        free(g_test_data);
        g_test_data = (unsigned char*) malloc(size);
    }
    g_test_data_start = 0;

    printf("Size: %u\n",size);

    for(uint32_t i = 0; i < g_test_data_len; i++ ){
        fread(&g_test_data[i],sizeof(unsigned char),1,f);
    }
    fclose(f);
}


int main(int argc, char *argv[]){
    setbuf(stdout,NULL);
    // Load the test file
    printf("Running verbose test\n");
    if(argc>1){
        // load a provided file
        test_data_load(argv[1]);
    } else {
        // or a default one if none provided
        test_data_load("data/tsip_sample");
    }
    // Run the decoder in verbose mode
    g_verbose_output = true;
    TaskUpLink200Hz();

    printf("\nRunning a timed test\n");
    // Run the decoder while timing it
    g_verbose_output = false;
    // load a large file
    test_data_load("data/tsip_sample_ext");
    clock_t start = clock(), diff;
    TaskUpLink200Hz();
    diff = clock()-start;
    int msec = diff * 1000 / CLOCKS_PER_SEC;
    printf("Time taken %d seconds %d milliseconds\n", msec/1000, msec%1000);

    free(g_test_data);
    g_test_data = NULL;
    return 0;
}
