//
// Created by haoliang on 2020/3/25.
//

#include <iostream>
#include <compress.h>
#include <FileOperator.h>
#include <sys/time.h>





void gdeltatest(char* datapath)
{
    printf("%s\n",datapath);
    FileOperator fileOperator(datapath, FileOpenType::Read);

    uint32_t inputsize32;
    uint32_t basesize32;
    uint8_t *inpsize=(uint8_t *)malloc(sizeof(u_int64_t));
    uint8_t *baseize=(uint8_t *)malloc(sizeof(u_int64_t));



    double sumbytes,predeltasize;
    double tEncode_x,tDecode_x;
    uint32_t round = 0;
    struct timeval TvStart,TvEND;
    uint8_t * inp = (uint8_t *)malloc(10*MBSIZE); //input
    uint8_t * bas = (uint8_t *)malloc(10*MBSIZE);

    uint8_t * seq = (uint8_t *)malloc(10*MBSIZE);
    uint8_t * restore = (uint8_t *)malloc(10*MBSIZE);
    uint8_t * data = (uint8_t *)malloc(10*MBSIZE);
    while(fileOperator.read(inpsize,sizeof(U64))  )
    {


        inputsize32 = *((u_int64_t *) inpsize);
        fileOperator.read(inp, *((u_int64_t *) inpsize));
        fileOperator.read(baseize, sizeof(U64));
        basesize32 = *((u_int64_t *) baseize);
        fileOperator.read(bas, *((u_int64_t *) baseize));

        if(inputsize32 >= 16383) continue;
//        if(inputsize32 >= 128*1024) continue;





        round++;
        if(round%5000 == 0) printf("Round:%d\n",round);
        sumbytes+=inputsize32;



        U32 datasize;
        U32 seqsize;
        U32 rsize;
        if(round == 3831)
        {
            int a =0;
        }


        {
            FILE* f1 = fopen("/home/thl/data/inputwrong","w");
            fwrite(inp,1,inputsize32,f1);
            fclose(f1);

            FILE* f2 = fopen("/home/thl/data/basewrong","w");
            fwrite(bas,1,basesize32,f2);
            fclose(f2);
        }
//        basesize32 = 0;

        gettimeofday(&TvStart, NULL);
//        printf("round；%d\n",round);

        gdelta_Encode(inp, inputsize32,bas,basesize32,data,seq,&datasize,&seqsize);

        gettimeofday(&TvEND, NULL);
        tEncode_x += (TvEND.tv_sec - TvStart.tv_sec) * 1000 + (TvEND.tv_usec - TvStart.tv_usec) / 1000.0;

        Gdelta_decompress1(data,datasize,bas,basesize32,restore,&rsize,inputsize32);

        assert(memcmp(restore,inp,rsize) == 0);

        predeltasize += datasize;

    }


    free(bas);
    free(restore);
    free(inp);
    free(seq);
    free(data);
    free(inpsize);

    free(baseize);


    printf("=================gdelta=====================\n");
    printf("Inputsize:%.3fMB\n", sumbytes/1024/1024);
    printf("Gdeltasize:%.3fMB\n", predeltasize/1024/1024 );
//    printf("all to zstd:%.3fMB\n",delta_all_zstdsize_x/1024/1024);

//    printf("Xdeltasize(+zstd):%.3fMB\n", deltasize_x/1024/1024 );
    printf("Encode speed :%.3fMB/s\n", sumbytes / 1024 / 1024 / tEncode_x * 1000);
//    printf("Decode speed :%.3fMB/s\n", sumbytes / 1024 / 1024 / tDecode_x * 1000);
//    printf("Encode speed(+zstd) :%.3fMB/s\n", sumbytes / 1024 / 1024 / (tEncode_x + tfseEncode_x) * 1000);
//    printf("Encode speed(+all to zstd) :%.3fMB/s\n", sumbytes / 1024 / 1024 / (tEncode_x + tallzstdEncode_x) * 1000);
//    printf("Decode speed(+zstd) :%.3fMB/s\n", sumbytes / 1024 / 1024 / (tDecode_x + tfseDecode_x)* 1000);


}

int main()
{
    char* datapath = "/data/OdessHome/similarChunks/WEB";
    gdeltatest(datapath);
}