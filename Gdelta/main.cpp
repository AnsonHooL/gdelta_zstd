#include <iostream>
#include "compress.h"




int main() {

    char* input = "/home/thl/data/input3";
    char* base  = "/home/thl/data/base3";

    uint8_t * inp = (uint8_t *)malloc(10*MBSIZE); //input
    uint8_t * bas = (uint8_t *)malloc(10*MBSIZE);
    uint8_t * databuf = (uint8_t *)malloc(10*MBSIZE);
    uint8_t * restore = (uint8_t *)malloc(10*MBSIZE);

    FILE* F1 = fopen(input,"r");
    FILE* F2 = fopen(base,"r");

    int Isize = 0;
    int Bsize = 0;
    U32 datasize;
    U32 rsize;
    while (int chunkLen = fread(inp,1,MBSIZE,F1)) {
        Isize += chunkLen;
    }
    while (int chunkLen = fread(bas,1,MBSIZE,F2)) {
        Bsize += chunkLen;
    }
//    Bsize = 0;
    gdelta_Encode(inp, Isize,bas,Bsize,databuf,&datasize);

    Gdelta_decompress1(databuf,datasize,bas,Bsize,restore,&rsize,Isize);


    assert(memcmp(restore,inp,rsize) == 0);
    printf("delta size：%d\n",datasize);

//    FILE* F3 = fopen("/home/thl/test/delta","w");
//    fwrite(databuf,1,datasize,F3);
//
//    FILE* F4 = fopen("/home/thl/test/seq","w");
//    fwrite(seqbuf,1,seqsize,F4);


    free(inp);
    free(bas);
    free(databuf);
    free(restore);

    fclose(F1);
    fclose(F2);
//    fclose(F3);
//    fclose(F4);
    return 0;
}
