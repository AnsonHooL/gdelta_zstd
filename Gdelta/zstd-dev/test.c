#include <zconf.h>
#include <stdlib.h>
#include <stdio.h>
//
// Created by haoliang on 2020/2/17.
//
int fseenins(void* src,int nbseq,void* dst);
typedef struct seqDef_s1 {
    u_int32_t offset;
    u_int16_t litLength;
    u_int16_t matchLength;
} seqDef1;

#define  NUM 100
int main()
{

    u_int32_t * num = (u_int32_t*)malloc(3403440);


    void* dst =malloc(1024*1024*1000);
    seqDef1* src = (seqDef1*)malloc(1024*1024*1000);
    FILE* f1 = fopen("/home/thl/test/seq","r");
    fread(src,1,414010152,f1);
    fclose(f1);

    FILE*f2 = fopen("/home/thl/test/num","r");
    fread(num,1,3403440,f2);
    fclose(f2);

    int nbseq =414010152/8;
    int size;
    int seqnum;
    u_int32_t readlen=0;
    int count = 0;
    while(nbseq > NUM)
    {
        seqnum = num[count];
        count++;
        int i = fseenins((void*)(src+readlen),seqnum,dst);
        readlen +=seqnum;
        nbseq -= seqnum;
        size +=i;
    }

    printf("%d\n",size);
//    for(int i=0;i<size;i++)
//    {
//        printf("%c\n",((char*)dst)[i]);
//    }
    return 0;
}