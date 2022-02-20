//
// Created by haoliang on 2020/3/14.
//

#include "compress.h"
#include <cstdlib>
#include <cstdio>
#include <testzstd.h>
#include "educational_decoder/zstd_decompress.h"

static size_t
searchMax (matchState * ms,const BYTE* ip, const BYTE* const iLimit,
           size_t* offsetPtr);


static size_t
searchMax_Base (matchState * ms,const BYTE* ip, const BYTE* const iLimit,
           size_t* offsetPtr);

static void
G_updateDUBT(matchState* ms,
                const BYTE* ip, const BYTE* iend,
                U32 mls);


static void
G_updateDUBT_Base(matchState* ms,
             const BYTE* ip, const BYTE* iend,
             U32 mls);



static size_t
G_DUBT_findBestMatch(matchState* ms,
                          const BYTE* ip, const BYTE* const iend,
                          size_t* offsetPtr,
                          U32 mls,
                          ZSTD_dictMode_e dictMode);

static size_t
G_DUBT_findBestMatch_Base(matchState* ms,
                        const BYTE* ip, const BYTE* const iend,
                        size_t* offsetPtr,
                        U32 mls,
                        ZSTD_dictMode_e dictMode);


/** ZSTD_insertDUBT1() :
 *  sort one already inserted but unsorted position
 *  assumption : current >= btlow == (current - btmask)
 *  doesn't fail */


static void
G_insertDUBT1(matchState* ms,
              U32 current, const BYTE* inputEnd,
              U32 nbCompares, U32 btLow,
              ZSTD_dictMode_e dictMode);

static void
G_insertDUBT1_Base(matchState* ms,
              U32 current, const BYTE* inputEnd,
              U32 nbCompares, U32 btLow,
              ZSTD_dictMode_e dictMode);

void InitMSandSeq(matchState* ms, uint8_t* src, U32 srcsize,seqStore_t* seqstore)
{
    U32 safesize = srcsize + 1;
    U32 bit;
    for (bit = 0; safesize; bit++) {
        safesize >>= 1;
    }
    ms->cParams.hashLog     = bit;
    ms->cParams.chainLog    = bit + 1;
    ms->cParams.windowLog   = 19; /*TODO:How to design the windows size? */
    ms->cParams.searchLog   = 9;

    U32 hashsize        = 1U << ms->cParams.hashLog;
    U32 chainsize       = 1U << ms->cParams.chainLog;
    ms->self_hashTable  = (U32*)malloc(hashsize * sizeof(U32));
    ms->self_chainTable = (U32*)malloc(chainsize * sizeof(U32));
    memset(ms->self_hashTable,0,hashsize * sizeof(U32));
    memset(ms->self_chainTable,0,chainsize * sizeof(U32));
    ms->selfbase        = src -1;
    ms->mode            = ZSTD_noDict;
    ms->depth           = 2;
    ms->nextToUpdate    = 1; //TODO:we can set beg size later
    ms->endmatch        = 0;
    ms->endmatchlen     = 0;
    ms->endoffset       = 0;

    U32 basesize = ms->basesize + 1;
    for (bit = 0; basesize; bit++) {
        basesize >>= 1;
    }
    ms->cParams.base_hashLog     = bit;
    ms->cParams.base_chainLog    = bit + 1;
    U32 base_hashsize        = 1U << ms->cParams.base_hashLog;
    U32 base_chainsize       = 1U << ms->cParams.base_chainLog;
    ms->base_hashTable  = (U32*)malloc(base_hashsize * sizeof(U32));
    ms->base_chainTable = (U32*)malloc(base_chainsize * sizeof(U32));
    memset(ms->base_hashTable,0,base_hashsize * sizeof(U32));
    memset(ms->base_chainTable,0,base_chainsize * sizeof(U32));




    U32 maxseqNum = srcsize/MINMATCH + 1;
    seqstore->sequencesStart    = (seqDef*) malloc(sizeof(seqDef) * maxseqNum);
    seqstore->litStart          = (uint8*) malloc(srcsize);
    seqstore->lit       = seqstore->litStart;
    seqstore->sequences = seqstore->sequencesStart;
    seqstore->maxseqnum = maxseqNum;
    seqstore->longLengthID = 0;
//    seqstore->signal    =   (BYTE*)malloc(sizeof(BYTE) * maxseqNum);
//    seqstore->signalStart = seqstore->signal;

}


void DestroyMSandSeq(matchState* ms,seqStore_t* seq)
{
    free(ms->self_chainTable);
    free(ms->self_hashTable);
    free(ms->base_hashTable);
    free(ms->base_chainTable);
    free(seq->litStart);
    free(seq->sequencesStart);
//    free(seq->signalStart);
    free(seq);
    free(ms);
}

void SaveCompress(seqStore_t* seqstore,uint8_t* deltabuf,U32* deltasize)
{
    U32 sumsize;
    U32 datasize = seqstore->lit - seqstore->litStart;
    U32 seqnum   = seqstore->sequences - seqstore->sequencesStart;

//    U32 seqsize  = fseenins(seqstore->sequencesStart,seqnum,deltabuf);

//    FILE* F3 = fopen("/home/thl/test/lit","w");
//    fwrite(seqstore->litStart,1,datasize,F3);
//    fclose(F3);

    printf("lit:%dB\n",datasize);

    printf("fsenum:%d\n",seqnum);


    for(U32 i = 0;i<seqnum;i++)
    {
//        printf("offset:%d\n",seqstore->sequencesStart[i].litLength);

    }
}

int gdelta_Encode( uint8_t* newBuf, u_int32_t newSize,
                     uint8_t* baseBuf, u_int32_t baseSize,
                     uint8_t* dataBuf, u_int32_t* datasize)
{

    seqStore_t* seqstore = (seqStore_t*)malloc(sizeof(seqStore_t));
    matchState* ms       = (matchState*)malloc(sizeof(matchState));
    ms->basesize = baseSize;
    ms->basebase = baseBuf - 1;
    ms->base_nextToUpdate = 1;
    InitMSandSeq(ms,newBuf,newSize,seqstore);

    U32 begSize = 0;
    U32 endSize = 0;
    int beg     = 0;
    int end     = 0;

    size_t     matchLength = 0;
    uint16_t   offset = 0;
    uint8_t*   ip = newBuf;
    uint8_t*   istart = ip;  //copy的首地址
    uint8_t*   anchor = ip;  //insert的首地址
    uint8_t*   iend = istart + newSize;
    size_t     litLength = 0;
    U32        handlebytes = 0;

    while (begSize + 7 < baseSize && begSize + 7 < newSize) {
        if (*(uint64_t *) (baseBuf + begSize) == \
            *(uint64_t *) (newBuf + begSize)) {
            begSize += 8;
        } else
            break;
    }
    while (begSize < baseSize && begSize < newSize) {
        if (baseBuf[begSize] == newBuf[begSize]) {
            begSize++;
        } else
            break;
    }

    if (begSize > 16)
        beg = 1;
    else
        begSize = 0;

    while (endSize + 7 < baseSize && endSize + 7 < newSize) {
        if (*(uint64_t *) (baseBuf + baseSize - endSize - 8) == \
            *(uint64_t *) (newBuf + newSize - endSize - 8)) {
            endSize += 8;
        } else
            break;
    }
    while (endSize < baseSize && endSize < newSize) {
        if (baseBuf[baseSize - endSize - 1] == newBuf[newSize - endSize - 1]) {
            endSize++;
        } else
            break;
    }

    if (begSize + endSize > newSize)
        endSize = newSize - begSize;

    if (endSize > 16)
    {
        end = 1;
        ms->endmatch    = 1;
        ms->endmatchlen = endSize;
        ms->endoffset   = baseSize - endSize + 1;
    }
    else
        endSize = 0;
    /* end of detect */

    ms->base_nextToUpdate += begSize;
    ms->nextToUpdate      += begSize;

    if(beg)
    {

        matchLength = begSize, offset = 1, istart = ip;
        litLength = istart - anchor;
        storeSeq(seqstore, litLength, anchor, iend, (U32)offset, (matchLength - MINMATCH)<<1 | 1);
        anchor = ip = istart + matchLength;
        handlebytes += (matchLength+litLength);
    }

    if(baseBuf + baseSize - endSize - 8 > ms->basebase)
    {
        G_updateDUBT_Base(ms,baseBuf + baseSize - endSize - 8,baseBuf + baseSize - endSize ,4); //TODO: len 4 only now
    }


    gdelta_Encode_Helper2(newBuf + begSize,newSize - begSize - endSize ,ms,seqstore);


    size_t  csize = compressLitandSeq(dataBuf,1*1024*1024,seqstore,newSize);

//    if(1)
//    {
//        FILE* f1 = fopen("/home/thl/data/litandseq","w");
//        fwrite(dataBuf,1,csize,f1);
//        fclose(f1);
//    }



    *datasize  = csize;

    for(int i =0;i<seqstore->sequences - seqstore->sequencesStart;i++)
    {
//        printf("lit len[%d]:%d\n",i,seqstore->sequencesStart[i].litLength);
//        printf("matchlen[%d]:%d\n",i,seqstore->sequencesStart[i].matchLength);
//        printf("offset[%d]:%d\n",i,seqstore->sequencesStart[i].offset);
    }

//    for(int i = 0;i<seqstore->lit - seqstore->litStart;i++)
//    {
//        printf("lit[%d]:%d\n",i,seqstore->litStart[i]);
//    }

    Gdelta_decompress(seqstore,dataBuf,csize,newBuf,newSize,baseBuf,baseBuf,&baseSize);


    DestroyMSandSeq(ms,seqstore);
    return 0;
}


int gdelta_Encode_Helper1( uint8_t* newBuf, u_int32_t newSize,
                          matchState* ms,seqStore_t* seqstore)
{

    const BYTE* const istart = (const BYTE*)newBuf;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + newSize;
    const BYTE* const ilimit = iend - 8;
    U32   handlebytes = 0;


    const BYTE* const base = ms->selfbase;    //   TODO
    ZSTD_dictMode_e const dictMode = ms->mode;

    const U32 prefixLowestIndex = 1;  //must be 1
    const BYTE* const prefixLowest = base + prefixLowestIndex;



    ip += 1;  /* search start in position 2  */

    while(ip < ilimit) {
        size_t matchLength = 0;
        size_t offset = 0;
        const BYTE *start = ip + 1;  /* the start position of match */
        printf("ip - base =%ld\n",ip-base);
//        printf("ms->base_chainTable[25]:%d\n",ms->base_chainTable[25]);

        if(ip - base == 12)
        {
            int a = 10;
        }


        /* first search (depth 0) */
        {
            size_t offsetFound = 999999999;
            size_t const ml2 = searchMax_Base(ms, ip, iend, &offsetFound);
//            printf("matchlen:%d\n",ml2);
            if (ml2 > matchLength)
                matchLength = ml2, start = ip, offset = offsetFound;
        }

        if (matchLength < 4) {
            ip += ((ip - anchor) >> kSearchStrength) + 1;   /* jump faster over incompressible sections */
            continue;
        }




/* NOTE:
         * start[-offset+ZSTD_REP_MOVE-1] is undefined behavior.
         * (-offset+ZSTD_REP_MOVE-1) is unsigned, and is added to start, which
         * overflows the pointer, which is undefined behavior.
         */
        /* catch up */
        if (offset) {
            if (1) {
                while ( (start > anchor) && offset > 1
                        && (start[-1] == (ms->basebase + offset)[-1]) )  /* only search for offset within prefix */
                { start--; matchLength++,offset--; }
            }
        }

        /* store sequence */
        // _storeSequence: \\unused
        {
            size_t const litLength = start - anchor;
            printf("base match len:%lu\n",matchLength);
//            printf("litlen:%d\n",litLength);
            handlebytes += litLength;
            handlebytes += matchLength;
            storeSeq(seqstore, litLength, anchor, iend, (U32) offset, matchLength - MINMATCH);
            anchor = ip = start + matchLength;
        }



    }

    /* TODO:handle the last bytes*/
    if(anchor<iend)
    {
        memcpy(seqstore->lit,anchor,iend - anchor);
        seqstore->lit += iend - anchor;
        handlebytes   += iend - anchor;
    }
    assert(handlebytes==newSize);
    return 0;

}




int gdelta_Encode_Helper2( uint8_t* newBuf, u_int32_t newSize,
                          matchState* ms,seqStore_t* seqstore)
{

    const BYTE* const istart = (const BYTE*)newBuf;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + newSize;
    const BYTE* const ilimit = iend - 8;

    U32   handlebytes = 0;


    const BYTE* const base = ms->selfbase;    //   TODO
    ZSTD_dictMode_e const dictMode = ms->mode;
    const U32 prefixLowestIndex = 1;  //must be 1
    const BYTE* const prefixLowest = base + prefixLowestIndex;

    U32 rep[3];
    rep[0] = 1;
    rep[1] = 4;

    ip += 1;  /* search start in position 2  */
    U32 offset_1 = rep[0], offset_2 = rep[1], savedOffset=0;

    {
        U32 const maxRep = (U32)(ip - prefixLowest);
        if (offset_2 > maxRep) savedOffset = offset_2, offset_2 = 0;
        if (offset_1 > maxRep) savedOffset = offset_1, offset_1 = 0;
    }


    U32 depth = ms->depth;

    while(ip < ilimit) {
        size_t matchLength = 0;
        G_MATCHFLAG matchflag = NO_MATCH;
        size_t offset = 0;
        const BYTE *start = ip + 1;  /* the start position of match */
//        printf("ip - base =%d\n",ip-base);
//        printf("hashTable[4923]=%d\n",ms->self_hashTable[4923]);
        if(ip - base == 15)
        {
            int a = 10;
        }

        /* check repCode */
        if  (dictMode == ZSTD_noDict
             && (offset_1 > 0) & (MEM_read32(ip+1-offset_1) == MEM_read32(ip+1))) {
            matchLength = matchcount(ip+1+4, iend,ip+1+4-offset_1, iend) + 4;
            matchflag = SELF_MATCH;
            if (depth==0) goto _storeSequence;
        }

        /* first search (depth 0) */
        {
            size_t offsetFound = 999999999;
            size_t const ml2 = searchMax(ms, ip, iend, &offsetFound);

            size_t baseoffsetFound = 999999999;
            size_t const baseml2 = searchMax_Base(ms, ip, iend, &baseoffsetFound);

            if (ml2 > matchLength || baseml2 > matchLength )
            {
                int const gain_self = 4 * ml2      - G_highbit32(offsetFound);
                int const gain_base = 4 * baseml2  - G_highbit32(baseoffsetFound);
                if(gain_self > gain_base)
                {
                    matchLength = ml2, start = ip, offset = offsetFound;
                    matchflag = SELF_MATCH;
                }
                else{
                    matchLength = baseml2 ,start = ip,offset =baseoffsetFound;
                    matchflag = BASE_MATCH;
                }

            }

        }

        if (matchLength < 4) {
            ip += ((ip - anchor) >> kSearchStrength) + 1;   /* jump faster over incompressible sections */
            continue;
        }

        /* let's try to find a better solution */
        if (ms->depth >= 1)
            while (ip < ilimit) {
                ip++;
                if ((dictMode == ZSTD_noDict)
                    &&(offset) && ((offset_1>0) & (MEM_read32(ip) == MEM_read32(ip - offset_1)))) {
                    size_t const mlRep = matchcount(ip+4, iend,ip+4-offset_1, iend) + 4;
                    int const gain2 = (int)(mlRep * 3);
                    int const gain1 = (int)(matchLength*3 - G_highbit32((U32)offset+1) + 1);
                    if ((mlRep >= 4) && (gain2 > gain1))
                    {
                        matchLength = mlRep, offset = 0, start = ip ,matchflag = SELF_MATCH;
                    }
                }

                {
                    size_t offset2 = 999999999;
                    size_t const ml2 = searchMax(ms, ip, iend, &offset2);

                    size_t baseoffset2 = 999999999;
                    size_t const baseml2 = searchMax_Base(ms, ip, iend, &baseoffset2);

                    int const gain2     = (int) (ml2 * 4 - G_highbit32((U32) offset2 + 1));   /* raw approx */
                    int const gain1     = (int) (matchLength * 4 - G_highbit32((U32) offset + 1) + 4);
                    int const basegain2 = (int) (baseml2 * 4 - G_highbit32((U32) baseoffset2 + 1));

                    if ((ml2 >= 4 || baseml2 >= 4) && (gain2 > gain1 || basegain2 > gain1)) {
                        if(gain2 > basegain2)
                        {
                            matchLength = ml2, offset = offset2, start = ip;
                            matchflag = SELF_MATCH;
                        } else
                        {
                            matchLength = baseml2, offset = baseoffset2, start = ip;
                            matchflag = BASE_MATCH;
                        }

                        continue;   /* search a better one */
                    }
                }
                /* let's find an even better one */
                if ((ms->depth == 2) && (ip < ilimit)) {
                    ip++;
                    if ( (dictMode == ZSTD_noDict)
                         &&(offset) && ((offset_1>0) & (MEM_read32(ip) == MEM_read32(ip - offset_1)))) {
                        size_t const mlRep = matchcount(ip+4,iend, ip+4-offset_1, iend) + 4;
                        int const gain2 = (int)(mlRep * 4);
                        int const gain1 = (int)(matchLength*4 - G_highbit32((U32)offset+1) + 1);
                        if ((mlRep >= 4) && (gain2 > gain1))
                            matchLength = mlRep, offset = 0, start = ip, matchflag = SELF_MATCH;
                    }

                    {
                        size_t offset2 = 999999999;
                        size_t const ml2 = searchMax(ms, ip, iend, &offset2);

                        size_t baseoffset2 = 999999999;
                        size_t const baseml2 = searchMax_Base(ms, ip, iend, &baseoffset2);

                        int const gain2 = (int) (ml2 * 4 - G_highbit32((U32) offset2 + 1));   /* raw approx */
                        int const gain1 = (int) (matchLength * 4 - G_highbit32((U32) offset + 1) + 7);
                        int const basegain2 = (int) (baseml2 * 4 - G_highbit32((U32) baseoffset2 + 1));


                        if ((ml2 >= 4 || baseml2 >= 4) && (gain2 > gain1 || basegain2 > gain1)) {
                            if(gain2 > basegain2)
                            {
                                matchLength = ml2, offset = offset2, start = ip;
                                matchflag = SELF_MATCH;
                            } else
                            {
                                matchLength = baseml2, offset = baseoffset2, start = ip;
                                matchflag = BASE_MATCH;
                            }
                            continue;   /* search a better one */
                        }
                    }
                }
                break;
            }

/* NOTE:
         * start[-offset+ZSTD_REP_MOVE-1] is undefined behavior.
         * (-offset+ZSTD_REP_MOVE-1) is unsigned, and is added to start, which
         * overflows the pointer, which is undefined behavior.
         */
        /* catch up */

        if(matchflag == SELF_MATCH)
        {
            if (offset) {
                if (1) {
                    while ( ((start > anchor) & (start - (offset-ZSTD_REP_MOVE) > prefixLowest))
                            && (start[-1] == (start-(offset-ZSTD_REP_MOVE))[-1]) )  /* only search for offset within prefix */
                    { start--; matchLength++; }
                }

                offset_2 = offset_1; offset_1 = (U32)(offset - ZSTD_REP_MOVE);
            }

        } else if (matchflag == BASE_MATCH)
        {
            if (offset) {
                if (1) {
                    while ( (start > anchor) && offset > 1
                            && (start[-1] == (ms->basebase + offset)[-1]) )  /* only search for offset within prefix */
                    { start--; matchLength++,offset--; }
                }
            }
        }


        /* store sequence */
        _storeSequence:
        {
            if(matchflag == SELF_MATCH)
            {
                size_t const litLength = start - anchor;
//                printf("lit len:%d\n",litLength);
//                printf("self match len:%d\n",matchLength);
//            printf("litlen:%d\n",litLength);
                handlebytes += litLength;
                handlebytes += matchLength;
//                seqstore->signal[0] = 0;
//                seqstore->signal++;
                storeSeq(seqstore, litLength, anchor, iend, (U32) offset, (matchLength - MINMATCH)<<1 | 0);
                anchor = ip = start + matchLength;
            }
            else if(matchflag == BASE_MATCH)
            {
                size_t const litLength = start - anchor;
//                printf("lit len:%d\n",litLength);
//                printf("base match len:%d\n",matchLength);

                handlebytes += litLength;
                handlebytes += matchLength;
//                seqstore->signal[0] = 1;
//                seqstore->signal++;

                storeSeq(seqstore, litLength, anchor, iend, (U32) offset, (matchLength - MINMATCH)<<1 | 1);
                anchor = ip = start + matchLength;
            }

        }

//        if (dictMode != ZSTD_noDict) {
//            while ( ((ip <= ilimit) & (offset_2>0))
//                    && (MEM_read32(ip) == MEM_read32(ip - offset_2)) ) {
//                /* store sequence */
//                matchLength = matchcount(ip+4,iend, ip+4-offset_2, iend) + 4;
//                offset = offset_2; offset_2 = offset_1; offset_1 = (U32)offset; /* swap repcodes */
//                matchflag = SELF_MATCH;
//                storeSeq(seqstore, 0, anchor, iend, 0, matchLength-MINMATCH);
//                ip += matchLength;
//                anchor = ip;
//                continue;   /* faster when present ... (?) */
//            }   }

        matchflag = NO_MATCH;

    }

    /* TODO:handle the last bytes*/


    if(ms->endmatch)
    {
        U32 matchLength = ms->endmatchlen;
        U32 offset      = ms->endoffset;

        storeSeq(seqstore, iend - anchor, anchor, iend, offset, (matchLength - MINMATCH)<<1 | 1);
        handlebytes += iend - anchor;
        anchor = iend;
    }
    if(anchor<iend)
    {
        memcpy(seqstore->lit,anchor,iend - anchor);
        seqstore->lit += iend - anchor;
        handlebytes   += iend - anchor;

    }

    assert(handlebytes==newSize);
    return 0;

}








int gdelta_Encode_Helper( uint8_t* newBuf, u_int32_t newSize,
                   matchState* ms,seqStore_t* seqstore)
{

    const BYTE* const istart = (const BYTE*)newBuf;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + newSize;
    const BYTE* const ilimit = iend - 8;
    G_MATCHFLAG matchflag;
    U32   handlebytes = 0;

    const BYTE* const base = ms->selfbase;    //   TODO
    ZSTD_dictMode_e const dictMode = ms->mode;
    const U32 prefixLowestIndex = 1;  //must be 1
    const BYTE* const prefixLowest = base + prefixLowestIndex;

    U32 rep[3];
    rep[0] = 1;
    rep[1] = 4;

    ip += 1;  /* search start in position 2  */
    U32 offset_1 = rep[0], offset_2 = rep[1], savedOffset=0;

    {
        U32 const maxRep = (U32)(ip - prefixLowest);
        if (offset_2 > maxRep) savedOffset = offset_2, offset_2 = 0;
        if (offset_1 > maxRep) savedOffset = offset_1, offset_1 = 0;
    }


    U32 depth = ms->depth;

    while(ip < ilimit) {
        size_t matchLength = 0;
        size_t offset = 0;
        const BYTE *start = ip + 1;  /* the start position of match */
        printf("ip - base =%ld\n",ip-base);
//        printf("hashTable[4923]=%d\n",ms->self_hashTable[4923]);
        if(ip - base == 8340)
        {
            int a = 10;
        }

        /* check repCode */
        if  (dictMode == ZSTD_noDict
        && (offset_1 > 0) & (MEM_read32(ip+1-offset_1) == MEM_read32(ip+1))) {
            matchLength = matchcount(ip+1+4, iend,ip+1+4-offset_1, iend) + 4;
            if (depth==0) goto _storeSequence;
        }

        /* first search (depth 0) */
        {
            size_t offsetFound = 999999999;
            size_t const ml2 = searchMax(ms, ip, iend, &offsetFound);
            if (ml2 > matchLength)
                matchLength = ml2, start = ip, offset = offsetFound;
        }

        if (matchLength < 4) {
            ip += ((ip - anchor) >> kSearchStrength) + 1;   /* jump faster over incompressible sections */
            continue;
        }

        /* let's try to find a better solution */
        if (ms->depth >= 1)
            while (ip < ilimit) {
                ip++;
                if ((dictMode == ZSTD_noDict)
                    &&(offset) && ((offset_1>0) & (MEM_read32(ip) == MEM_read32(ip - offset_1)))) {
                    size_t const mlRep = matchcount(ip+4, iend,ip+4-offset_1, iend) + 4;
                    int const gain2 = (int)(mlRep * 3);
                    int const gain1 = (int)(matchLength*3 - G_highbit32((U32)offset+1) + 1);
                    if ((mlRep >= 4) && (gain2 > gain1))
                        matchLength = mlRep, offset = 0, start = ip;
                }

                {
                    size_t offset2 = 999999999;
                    size_t const ml2 = searchMax(ms, ip, iend, &offset2);
                    int const gain2 = (int) (ml2 * 4 - G_highbit32((U32) offset2 + 1));   /* raw approx */
                    int const gain1 = (int) (matchLength * 4 - G_highbit32((U32) offset + 1) + 4);
                    if ((ml2 >= 4) && (gain2 > gain1)) {
                        matchLength = ml2, offset = offset2, start = ip;
                        continue;   /* search a better one */
                    }
                }
                /* let's find an even better one */
                if ((ms->depth == 2) && (ip < ilimit)) {
                    ip++;
                    if ( (dictMode == ZSTD_noDict)
                      &&(offset) && ((offset_1>0) & (MEM_read32(ip) == MEM_read32(ip - offset_1)))) {
                        size_t const mlRep = matchcount(ip+4,iend, ip+4-offset_1, iend) + 4;
                        int const gain2 = (int)(mlRep * 4);
                        int const gain1 = (int)(matchLength*4 - G_highbit32((U32)offset+1) + 1);
                        if ((mlRep >= 4) && (gain2 > gain1))
                            matchLength = mlRep, offset = 0, start = ip;
                    }

                    {
                        size_t offset2 = 999999999;
                        size_t const ml2 = searchMax(ms, ip, iend, &offset2);
                        int const gain2 = (int) (ml2 * 4 - G_highbit32((U32) offset2 + 1));   /* raw approx */
                        int const gain1 = (int) (matchLength * 4 - G_highbit32((U32) offset + 1) + 7);
                        if ((ml2 >= 4) && (gain2 > gain1)) {
                            matchLength = ml2, offset = offset2, start = ip;
                            continue;
                        }
                    }
                }
                break;
            }

/* NOTE:
         * start[-offset+ZSTD_REP_MOVE-1] is undefined behavior.
         * (-offset+ZSTD_REP_MOVE-1) is unsigned, and is added to start, which
         * overflows the pointer, which is undefined behavior.
         */
        /* catch up */
        if (offset) {
            if (1) {
                while ( ((start > anchor) & (start - (offset-ZSTD_REP_MOVE) > prefixLowest))
                        && (start[-1] == (start-(offset-ZSTD_REP_MOVE))[-1]) )  /* only search for offset within prefix */
                { start--; matchLength++; }
            }

            offset_2 = offset_1; offset_1 = (U32)(offset - ZSTD_REP_MOVE);
        }

        /* store sequence */
_storeSequence:
        {
            size_t const litLength = start - anchor;
            printf("litlen:%lu\n",litLength);
            printf("self match len:%lu\n",matchLength);
            printf("offset:%lu\n",offset);
            handlebytes += litLength;
            handlebytes += matchLength;
            storeSeq(seqstore, litLength, anchor, iend, (U32) offset, matchLength - MINMATCH);
            anchor = ip = start + matchLength;
        }

        if (dictMode != ZSTD_noDict) {
            while ( ((ip <= ilimit) & (offset_2>0))
                    && (MEM_read32(ip) == MEM_read32(ip - offset_2)) ) {
                /* store sequence */
                matchLength = matchcount(ip+4,iend, ip+4-offset_2, iend) + 4;
                offset = offset_2; offset_2 = offset_1; offset_1 = (U32)offset; /* swap repcodes */
                handlebytes += matchLength;
                handlebytes += 0;
                storeSeq(seqstore, 0, anchor, iend, 0, matchLength-MINMATCH);
                ip += matchLength;
                anchor = ip;
                continue;   /* faster when present ... (?) */
            }   }

    }

    /* TODO:handle the last bytes*/
    if(anchor<iend)
    {
        memcpy(seqstore->lit,anchor,iend - anchor);
        seqstore->lit += iend - anchor;
        handlebytes   += iend - anchor;
    }
    assert(handlebytes==newSize);
    return 0;

}

static size_t
searchMax (matchState * ms,const BYTE* ip, const BYTE* const iLimit,
           size_t* offsetPtr)
{
    switch(MINMATCH)
    {
        default : /* includes case 3 */
        case 4 : return G_BtFindBestMatch(ms, ip, iLimit, offsetPtr, 4, ZSTD_noDict);
        case 5 : return G_BtFindBestMatch(ms, ip, iLimit, offsetPtr, 5, ZSTD_noDict);
        case 7 :
        case 6 : return G_BtFindBestMatch(ms, ip, iLimit, offsetPtr, 6, ZSTD_noDict);
    }
}





/*-*************************************
*  Binary Tree search
***************************************/

inline size_t
G_BtFindBestMatch( matchState* ms,const BYTE* const ip, const BYTE* const iLimit,size_t* offsetPtr,
                   const U32 mls /* template */,const ZSTD_dictMode_e dictMode)
{
    if (ip < ms-> selfbase + ms->nextToUpdate) return 0;   /* skipped area */
    G_updateDUBT(ms, ip, iLimit, mls);
    return G_DUBT_findBestMatch(ms, ip, iLimit, offsetPtr, mls, dictMode);
}



static void
G_updateDUBT(matchState* ms,
                const BYTE* ip, const BYTE* iend,
                U32 mls)
{
    const G_compressionParameters* const cParams = &ms->cParams;
    U32* const hashTable = ms->self_hashTable;
    U32  const hashLog = cParams->hashLog;

    U32* const bt = ms->self_chainTable;
    U32  const btLog  = cParams->chainLog - 1;
    U32  const btMask = (1 << btLog) - 1;

    const BYTE* const base = ms->selfbase;
    U32 const target = (U32)(ip - base);
    U32 idx = ms->nextToUpdate;

//    if (idx != target)
//        DEBUGLOG(7, "ZSTD_updateDUBT, from %u to %u (dictLimit:%u)",
//                 idx, target, ms->window.dictLimit);
    assert(ip + 8 <= iend);   /* condition for ZSTD_hashPtr */
    (void)iend;

//    assert(idx >= ms->window.dictLimit);   /* condition for valid base+idx */

    for ( ; idx < target ; idx++) {
        size_t const h  = ZSTD_hashPtr(base + idx, hashLog, mls);   /* assumption : ip + 8 <= iend */
        U32    const matchIndex = hashTable[h];

        U32*   const nextCandidatePtr = bt + 2*(idx&btMask);
        U32*   const sortMarkPtr  = nextCandidatePtr + 1;

//        DEBUGLOG(8, "ZSTD_updateDUBT: insert %u", idx);
        hashTable[h] = idx;   /* Update Hash Table */
        *nextCandidatePtr = matchIndex;   /* update BT like a chain */
        *sortMarkPtr = ZSTD_DUBT_UNSORTED_MARK;
    }
    ms->nextToUpdate = target;
}


static size_t
G_DUBT_findBestMatch(matchState* ms,
                        const BYTE* const ip, const BYTE* const iend,
                        size_t* offsetPtr,
                        U32 const mls,
                        const ZSTD_dictMode_e dictMode)
{
    const  G_compressionParameters* const cParams = &ms->cParams;
    U32*   const hashTable = ms->self_hashTable;
    U32    const hashLog = cParams->hashLog;
    size_t const h  = ZSTD_hashPtr(ip, hashLog, mls);
    U32          matchIndex  = hashTable[h];

//    const BYTE* const base = ms->window.base; TODO:window.base is what?  base = src - 1
    const BYTE* const base = ms->selfbase;
    U32    const current = (U32)(ip-base);
//    U32    const windowLow = ZSTD_getLowestMatchIndex(ms, current, cParams->windowLog); //must 1
    U32    const windowLow = 1;             /* below that point, no more valid data */
    U32*   const bt = ms->self_chainTable;
    U32    const btLog  = cParams->chainLog - 1;
    U32    const btMask = (1 << btLog) - 1;
    U32    const btLow = (btMask >= current) ? 0 : current - btMask;   //must 0
    U32    const unsortLimit = MAX(btLow, windowLow);     /*unsorted position must larger than 1*/

    U32*         nextCandidate = bt + 2*(matchIndex&btMask);
    U32*         unsortedMark = bt + 2*(matchIndex&btMask) + 1;
    U32          nbCompares = 1U << cParams->searchLog;  /* now 512 times */
    U32          nbCandidates = nbCompares;
    U32          previousCandidate = 0;

//    DEBUGLOG(7, "ZSTD_DUBT_findBestMatch (%u) ", current);
    assert(ip <= iend-8);   /* required for h calculation */


    /* reach end of unsorted candidates list */
    while ( (matchIndex > unsortLimit)
            && (*unsortedMark == ZSTD_DUBT_UNSORTED_MARK)
            && (nbCandidates > 1) ) {
//        DEBUGLOG(8, "ZSTD_DUBT_findBestMatch: candidate %u is unsorted",
//                 matchIndex);
        *unsortedMark = previousCandidate;  /* the unsortedMark becomes a reversed chain, to move up back to original position */
        previousCandidate = matchIndex;
        matchIndex = *nextCandidate;
        nextCandidate = bt + 2*(matchIndex&btMask);
        unsortedMark = bt + 2*(matchIndex&btMask) + 1;
        nbCandidates --;
    }
    if(current == 337 )
    {
        int b=10;
    }
    /* nullify last candidate if it's still unsorted
     * simplification, detrimental to compression ratio, beneficial for speed */
    if ( (matchIndex > unsortLimit)
         && (*unsortedMark==ZSTD_DUBT_UNSORTED_MARK) ) {
//        DEBUGLOG(7, "ZSTD_DUBT_findBestMatch: nullify last unsorted candidate %u",
//                 matchIndex);
        *nextCandidate = *unsortedMark = 0;
    }

    /* batch sort stacked candidates */
    matchIndex = previousCandidate;
    while (matchIndex) {  /* will end on matchIndex == 0 */    //从小到大,按内容插入排序
        U32* const nextCandidateIdxPtr = bt + 2*(matchIndex&btMask) + 1;
        U32 const nextCandidateIdx = *nextCandidateIdxPtr;
        G_insertDUBT1(ms, matchIndex, iend,
                         nbCandidates, unsortLimit, dictMode);
        matchIndex = nextCandidateIdx;
        nbCandidates++;
    }

    /* find longest match */
    {   size_t commonLengthSmaller = 0, commonLengthLarger = 0;
//        const BYTE* const dictBase = ms->window.dictBase;
//        const U32 dictLimit = ms->window.dictLimit;   // must be 1
//        const BYTE* const dictEnd = dictBase + dictLimit;
//        const BYTE* const prefixStart = base + dictLimit;
        U32* smallerPtr = bt + 2*(current&btMask);
        U32* largerPtr  = bt + 2*(current&btMask) + 1;
        U32 matchEndIdx = current + 8 + 1;
        U32 dummy32;   /* to be nullified at the end */
        size_t bestLength = 0;

        matchIndex  = hashTable[h];
        hashTable[h] = current;   /* Update Hash Table */

        while (nbCompares-- && (matchIndex > windowLow)) {
            U32* const nextPtr = bt + 2*(matchIndex & btMask);
            size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);   /* guaranteed minimum nb of common bytes */
            const BYTE* match;

            if (dictMode != ZSTD_extDict)  {
                match = base + matchIndex;
//                matchLength += ZSTD_count(ip+matchLength, match+matchLength, iend);
                matchLength += matchcount(ip+matchLength, iend,match+matchLength, iend);
            } else {
//                match = dictBase + matchIndex;
//                matchLength += ZSTD_count_2segments(ip+matchLength, match+matchLength, iend, dictEnd, prefixStart);
//                if (matchIndex+matchLength >= dictLimit)
//                    match = base + matchIndex;   /* to prepare for next usage of match[matchLength] */
            }

            if (matchLength > bestLength) {
                if (matchLength > matchEndIdx - matchIndex)
                    matchEndIdx = matchIndex + (U32)matchLength;
                if ( (4*(int)(matchLength-bestLength)) > (int)(G_highbit32(current-matchIndex+1) - G_highbit32((U32)offsetPtr[0]+1)) )
                    bestLength = matchLength, *offsetPtr = ZSTD_REP_MOVE + current - matchIndex;
                if (ip+matchLength == iend) {   /* equal : no way to know if inf or sup */
//                    if (dictMode == ZSTD_dictMatchState) {
//                        nbCompares = 0; /* in addition to avoiding checking any
//                                         * further in this loop, make sure we
//                                         * skip checking in the dictionary. */
//                    }
                    break;   /* drop, to guarantee consistency (miss a little bit of compression) */
                }
            }

            if (match[matchLength] < ip[matchLength]) {
                /* match is smaller than current */
                *smallerPtr = matchIndex;             /* update smaller idx */
                commonLengthSmaller = matchLength;    /* all smaller will now have at least this guaranteed common length */
                if (matchIndex <= btLow) { smallerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
                smallerPtr = nextPtr+1;               /* new "smaller" => larger of match */
                matchIndex = nextPtr[1];              /* new matchIndex larger than previous (closer to current) */
            } else {
                /* match is larger than current */
                *largerPtr = matchIndex;
                commonLengthLarger = matchLength;
                if (matchIndex <= btLow) { largerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
                largerPtr = nextPtr;
                matchIndex = nextPtr[0];
            }   }

        *smallerPtr = *largerPtr = 0;

//        if (dictMode == ZSTD_dictMatchState && nbCompares) {
//            bestLength = ZSTD_DUBT_findBetterDictMatch(
//                    ms, ip, iend,
//                    offsetPtr, bestLength, nbCompares,
//                    mls, dictMode);
//        }

        assert(matchEndIdx > current+8); /* ensure nextToUpdate is increased */
        ms->nextToUpdate = matchEndIdx - 8;   /* skip repetitive patterns */
        if (bestLength >= MINMATCH) {
            U32 const mIndex = current - ((U32)*offsetPtr - ZSTD_REP_MOVE); (void)mIndex;
//            DEBUGLOG(8, "ZSTD_DUBT_findBestMatch(%u) : found match of length %u and offsetCode %u (pos %u)",
//                     current, (U32)bestLength, (U32)*offsetPtr, mIndex);
        }
        return bestLength;
    }
}







/** ZSTD_insertDUBT1() :
 *  sort one already inserted but unsorted position
 *  assumption : current >= btlow == (current - btmask)
 *  doesn't fail */
static void
G_insertDUBT1(matchState* ms,
                 U32 current, const BYTE* inputEnd,
                 U32 nbCompares, U32 btLow,
                 const ZSTD_dictMode_e dictMode)
{
    const G_compressionParameters* const cParams = &ms->cParams;
    U32* const bt = ms->self_chainTable;
    U32  const btLog  = cParams->chainLog - 1;
    U32  const btMask = (1 << btLog) - 1;
    size_t commonLengthSmaller=0, commonLengthLarger=0;
    const BYTE* const base = ms->selfbase;

//    const BYTE* const dictBase = ms->window.dictBase;
//    const U32 dictLimit = ms->window.dictLimit;

//    const BYTE* const ip = (current>=dictLimit) ? base + current : dictBase + current;
//    const BYTE* const iend = (current>=dictLimit) ? inputEnd : dictBase + dictLimit;


    const BYTE* const ip =  base + current;
    const BYTE* const iend =  inputEnd;

//    const BYTE* const dictEnd = dictBase + dictLimit;
//    const BYTE* const prefixStart = base + dictLimit;

    const BYTE* match;
    U32* smallerPtr = bt + 2*(current&btMask);
    U32* largerPtr  = smallerPtr + 1;
    U32 matchIndex = *smallerPtr;   /* this candidate is unsorted : next sorted candidate is reached through *smallerPtr, while *largerPtr contains previous unsorted candidate (which is already saved and can be overwritten) */
    U32 dummy32;   /* to be nullified at the end */
//    U32 const windowValid = ms->window.lowLimit;
    U32 const windowValid = 1;
//    U32 const maxDistance = 1U << cParams->windowLog;
//    U32 const windowLow = (current - windowValid > maxDistance) ? current - maxDistance : windowValid;
    U32 const windowLow = windowValid;

//    DEBUGLOG(8, "ZSTD_insertDUBT1(%u) (dictLimit=%u, lowLimit=%u)",
//             current, dictLimit, windowLow);
    assert(current >= btLow);
    assert(ip < iend);   /* condition for ZSTD_count */

    while (nbCompares-- && (matchIndex > windowLow)) {
        U32* const nextPtr = bt + 2*(matchIndex & btMask);
        size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);   /* guaranteed minimum nb of common bytes */

        if(matchIndex >= current)
        {
            int a =10;
        }

        assert(matchIndex < current);
        /* note : all candidates are now supposed sorted,
         * but it's still possible to have nextPtr[1] == ZSTD_DUBT_UNSORTED_MARK
         * when a real index has the same value as ZSTD_DUBT_UNSORTED_MARK */

        if ( dictMode != ZSTD_extDict)
         {
            const BYTE* const mBase = base;
//            assert( (matchIndex+matchLength >= dictLimit)   /* might be wrong if extDict is incorrectly set to 0 */
//                    || (current < dictLimit) );
            match = mBase + matchIndex;
            matchLength += matchcount(ip+matchLength, iend, match+matchLength, iend);
        }
//        else {
//            match = dictBase + matchIndex;
//            matchLength += ZSTD_count_2segments(ip+matchLength, match+matchLength, iend, dictEnd, prefixStart);
//            if (matchIndex+matchLength >= dictLimit)
//                match = base + matchIndex;   /* preparation for next read of match[matchLength] */
//        }
//
//        DEBUGLOG(8, "ZSTD_insertDUBT1: comparing %u with %u : found %u common bytes ",
//                 current, matchIndex, (U32)matchLength);

        if (ip+matchLength == iend) {   /* equal : no way to know if inf or sup */
            break;   /* drop , to guarantee consistency ; miss a bit of compression, but other solutions can corrupt tree */
        }

        if (match[matchLength] < ip[matchLength]) {  /* necessarily within buffer */
            /* match is smaller than current */
            *smallerPtr = matchIndex;             /* update smaller idx */
            commonLengthSmaller = matchLength;    /* all smaller will now have at least this guaranteed common length */
            if (matchIndex <= btLow) { smallerPtr=&dummy32; break; }   /* beyond tree size, stop searching */
//            DEBUGLOG(8, "ZSTD_insertDUBT1: %u (>btLow=%u) is smaller : next => %u",
//                     matchIndex, btLow, nextPtr[1]);
            smallerPtr = nextPtr+1;               /* new "candidate" => larger than match, which was smaller than target */
            matchIndex = nextPtr[1];              /* new matchIndex, larger than previous and closer to current */
        } else {
            /* match is larger than current */
            *largerPtr = matchIndex;
            commonLengthLarger = matchLength;
            if (matchIndex <= btLow) { largerPtr=&dummy32; break; }   /* beyond tree size, stop searching */
//            DEBUGLOG(8, "ZSTD_insertDUBT1: %u (>btLow=%u) is larger => %u",
//                     matchIndex, btLow, nextPtr[0]);
            largerPtr = nextPtr;
            matchIndex = nextPtr[0];
        }   }

    *smallerPtr = *largerPtr = 0;
}





void storeSeq(seqStore_t* seqStorePtr, size_t litLength, const BYTE* literals, const BYTE* litLimit, U32 offCode, size_t mlBase)
{
    seqStorePtr->sequences[0].offset = offCode  ;  //todo:how to decode?
    assert(literals + litLength <= litLimit);
    assert(seqStorePtr->sequences - seqStorePtr->sequencesStart <= seqStorePtr->maxseqnum);
//    printf("%d\n",mlBase);
//    assert(mlBase <= 65536);
           /**  matchlen type U16 0~65535   **/
    if(mlBase <= 65535)
    {
        seqStorePtr->sequences[0].matchLength = mlBase;
        seqStorePtr->sequences[0].litLength = litLength ;

        memcpy(seqStorePtr->lit,literals,litLength);
        seqStorePtr->lit += litLength;

        seqStorePtr->sequences++;
    }
    else
    {
        int flag = mlBase & 1U;
        mlBase = (mlBase>>1) + MINMATCH;
        if(flag == 1)
        {
            U32 handlebytes = 0;
            storeSeq(seqStorePtr,litLength,literals,litLimit,offCode,(32763 - MINMATCH)<<1 | 1);
            mlBase = mlBase - 32763;
            handlebytes += 32763;
            while(mlBase >= 32767)
            {
                storeSeq(seqStorePtr,0,literals,litLimit,offCode+handlebytes,(32763 - MINMATCH)<<1 | 1);
                mlBase      = mlBase - 32763;
                handlebytes += 32763;
            }
            if(mlBase)
            {
                storeSeq(seqStorePtr,0,literals,litLimit,offCode+handlebytes,(mlBase - MINMATCH)<<1 | 1);
            }

        }
        else if (flag == 0)
        {
            U32 handlebytes = 0;
            storeSeq(seqStorePtr,litLength,literals,litLimit,offCode,(32763 - MINMATCH)<<1 | 0);
            mlBase = mlBase - 32763;
            handlebytes += 32763;
            while(mlBase >= 32767)
            {
                storeSeq(seqStorePtr,0,literals,litLimit,offCode,(32763 - MINMATCH)<<1 | 0);
                mlBase      = mlBase - 32763;
                handlebytes += 32763;
            }
            if(mlBase)
            {
                storeSeq(seqStorePtr,0,literals,litLimit,offCode,(mlBase - MINMATCH)<<1 | 0);
            }
        }

    }

}

int matchcount(const BYTE * ip,const BYTE* iend,const BYTE* basematch,const BYTE* baseend)
{

    int j = 0;

    while (basematch + j + 7 < baseend &&
           ip + j + 7 < iend) {
        if (*(uint64_t *) (basematch+ j) == \
                        *(uint64_t *) (ip + j)) {
            j += 8;
        } else
            break;
    }
    while (basematch+ j < baseend &&
           ip + j < iend) {
        if (basematch[j] == ip[j]) {
            j++;
        } else
            break;
    }

    return j;
}




/*-*************************************
*  Binary Tree search
***************************************/


/**
 * BASE   SEARCH   MAX  FUNCTION
 * */



/**
* THE BASE CHUNK ONLY DO ONCE!
*/

static void
G_updateDUBT_Base(matchState* ms,
                  const BYTE* ip, const BYTE* iend,
                  U32 mls)
{


    const G_compressionParameters* const cParams = &ms->cParams;
    U32* const hashTable = ms->base_hashTable;
    U32  const hashLog = cParams->base_hashLog;

    U32* const bt = ms->base_chainTable;
    U32  const btLog  = cParams->base_chainLog - 1;
    U32  const btMask = (1 << btLog) - 1;

    const BYTE* const base = ms->basebase;
    U32 const target = (U32)(ip - base);

    U32 idx = ms->base_nextToUpdate;


//    if (idx != target)
//        DEBUGLOG(7, "ZSTD_updateDUBT, from %u to %u (dictLimit:%u)",
//                 idx, target, ms->window.dictLimit);
    assert(ip + 8 <= iend);   /* condition for ZSTD_hashPtr */
    (void)iend;

//    assert(idx >= ms->window.dictLimit);   /* condition for valid base+idx */

    for ( ; idx < target ; idx++) {


        size_t const h  = ZSTD_hashPtr(base + idx, hashLog, mls);   /* assumption : ip + 8 <= iend */
        U32    const matchIndex = hashTable[h];

        U32*   const nextCandidatePtr = bt + 2*(idx&btMask);
        U32*   const sortMarkPtr  = nextCandidatePtr + 1;

//        DEBUGLOG(8, "ZSTD_updateDUBT: insert %u", idx);
        hashTable[h] = idx;   /* Update Hash Table */
        *nextCandidatePtr = matchIndex;   /* update BT like a chain */
        *sortMarkPtr = ZSTD_DUBT_UNSORTED_MARK;
    }
    ms->base_nextToUpdate = target;
}








static size_t
searchMax_Base (matchState * ms,const BYTE* ip, const BYTE* const iLimit,
                size_t* offsetPtr)
{
    switch(MINMATCH)
    {
        default : /* includes case 3 */
        case 4 : return G_BtFindBestMatch_Base(ms, ip, iLimit, offsetPtr, 4, ZSTD_noDict);
        case 5 : return G_BtFindBestMatch_Base(ms, ip, iLimit, offsetPtr, 5, ZSTD_noDict);
        case 7 :
        case 6 : return G_BtFindBestMatch_Base(ms, ip, iLimit, offsetPtr, 6, ZSTD_noDict);
    }
}

inline size_t
G_BtFindBestMatch_Base( matchState* ms,const BYTE* const ip, const BYTE* const iLimit,size_t* offsetPtr,
                        const U32 mls /* template */,const ZSTD_dictMode_e dictMode)
{

    return G_DUBT_findBestMatch_Base(ms, ip, iLimit, offsetPtr, mls, dictMode);
}


static size_t
G_DUBT_findBestMatch_Base(matchState* ms,
                          const BYTE* const ip, const BYTE* const iend,
                          size_t* offsetPtr,
                          U32 const mls,
                          const ZSTD_dictMode_e dictMode)
{
    const  G_compressionParameters* const cParams = &ms->cParams;
    const U32* const hashTable = ms->base_hashTable;
    U32    const hashLog       = cParams->base_hashLog;
    size_t const h  = ZSTD_hashPtr(ip, hashLog, mls);
    U32          matchIndex  = hashTable[h];

//    const BYTE* const base = ms->window.base; TODO:window.base is what?  base = src - 1
    const BYTE* const base = ms->selfbase;
    U32    const current   = (U32)(ip-base);
//    U32    const windowLow = ZSTD_getLowestMatchIndex(ms, current, cParams->windowLog); //must 1
    U32    const windowLow = 1;             /* below that point, no more valid data */
    U32*   const bt        = ms->base_chainTable;
    U32    const btLog     = cParams->base_chainLog - 1;
    U32    const btMask    = (1 << btLog) - 1;

//    U32    const btLow     = (btMask >= current) ? 0 : current - btMask;   //must 0
    U32    const btLow       = 0;   //must 0
    U32    const unsortLimit = MAX(btLow, windowLow);     /*unsorted position must larger than 1*/

    U32*         nextCandidate = bt + 2*(matchIndex&btMask);
    U32*         unsortedMark  = bt + 2*(matchIndex&btMask) + 1;
    U32          nbCompares = 1U << cParams->searchLog;  /* now 512 times */
    U32          nbCandidates = nbCompares;
    U32          previousCandidate = 0;

//    DEBUGLOG(7, "ZSTD_DUBT_findBestMatch (%u) ", current);
    assert(ip <= iend-8);   /* required for h calculation */
    if(current == 350 )
    {
        int b=10;
    }

    /* reach end of unsorted candidates list */
    while ( (matchIndex > unsortLimit)
            && (*unsortedMark == ZSTD_DUBT_UNSORTED_MARK)
            && (nbCandidates > 1) ) {
//        DEBUGLOG(8, "ZSTD_DUBT_findBestMatch: candidate %u is unsorted",
//                 matchIndex);
        *unsortedMark     = previousCandidate;  /* the unsortedMark becomes a reversed chain, to move up back to original position */
        previousCandidate = matchIndex;
        matchIndex    = *nextCandidate;
        nextCandidate = bt + 2*(matchIndex&btMask);
        unsortedMark  = bt + 2*(matchIndex&btMask) + 1;
        nbCandidates --;
    }
    if(current == 350 )
    {
        int b=10;
    }
    /* nullify last candidate if it's still unsorted
     * simplification, detrimental to compression ratio, beneficial for speed */
    if ( (matchIndex > unsortLimit)
         && (*unsortedMark==ZSTD_DUBT_UNSORTED_MARK) ) {
//        DEBUGLOG(7, "ZSTD_DUBT_findBestMatch: nullify last unsorted candidate %u",
//                 matchIndex);
        *nextCandidate = *unsortedMark = 0;
    }

    /* batch sort stacked candidates */
    matchIndex = previousCandidate;
    while (matchIndex) {  /* will end on matchIndex == 0 */    //从小到大,按内容插入排序
        U32* const nextCandidateIdxPtr = bt + 2*(matchIndex&btMask) + 1;
        U32 const nextCandidateIdx = *nextCandidateIdxPtr;
        G_insertDUBT1_Base(ms, matchIndex, iend,
                           nbCandidates, unsortLimit, dictMode);
        matchIndex = nextCandidateIdx;
        nbCandidates++;
    }

    /* find longest match */
    {   size_t commonLengthSmaller = 0, commonLengthLarger = 0;
//        const BYTE* const dictBase = ms->window.dictBase;
//        const U32 dictLimit = ms->window.dictLimit;   // must be 1
//        const BYTE* const dictEnd = dictBase + dictLimit;
//        const BYTE* const prefixStart = base + dictLimit;
        U32* smallerPtr = bt + 2*(current&btMask);
        U32* largerPtr  = bt + 2*(current&btMask) + 1;
        U32 matchEndIdx = current + 8 + 1;
        U32 dummy32;   /* to be nullified at the end */
        size_t bestLength = 0;

        matchIndex  = hashTable[h];
//        hashTable[h] = current;   /* don't need to Update Hash Table */

        while (nbCompares-- && (matchIndex > windowLow)) {
            U32* const nextPtr = bt + 2*(matchIndex & btMask);
            size_t matchLength = 0;   /* guaranteed minimum nb of common bytes */
            const BYTE* match;

            if (dictMode != ZSTD_extDict)  {
                match = ms->basebase + matchIndex;
                BYTE* baseend = ms->basebase + ms->basesize+1;
//                matchLength += ZSTD_count(ip+matchLength, match+matchLength, iend);
                matchLength += matchcount(ip+matchLength, iend,match+matchLength, baseend);
            } else {
//                match = dictBase + matchIndex;
//                matchLength += ZSTD_count_2segments(ip+matchLength, match+matchLength, iend, dictEnd, prefixStart);
//                if (matchIndex+matchLength >= dictLimit)
//                    match = base + matchIndex;   /* to prepare for next usage of match[matchLength] */
            }

            if (matchLength > bestLength) {

//                if (matchLength > matchEndIdx - matchIndex)
//                    matchEndIdx = matchIndex + (U32)matchLength;

                if ( (4*(int)(matchLength-bestLength)) > (int)(G_highbit32(matchIndex) - G_highbit32((U32)offsetPtr[0]+1)) )
                    bestLength = matchLength, *offsetPtr = matchIndex;
                if (ip+matchLength == iend) {   /* equal : no way to know if inf or sup */
//                    if (dictMode == ZSTD_dictMatchState) {
//                        nbCompares = 0; /* in addition to avoiding checking any
//                                         * further in this loop, make sure we
//                                         * skip checking in the dictionary. */
//                    }
                    break;   /* drop, to guarantee consistency (miss a little bit of compression) */
                }
            }

            if (match[matchLength] < ip[matchLength]) {
                /* match is smaller than current */
//                *smallerPtr = matchIndex;             /* update smaller idx */
                commonLengthSmaller = matchLength;    /* all smaller will now have at least this guaranteed common length */
//                if (matchIndex <= btLow) { smallerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
//                smallerPtr = nextPtr+1;               /* new "smaller" => larger of match */
                matchIndex = nextPtr[1];              /* new matchIndex larger than previous (closer to current) */
            } else {
                /* match is larger than current */
//                *largerPtr = matchIndex;
                commonLengthLarger = matchLength;
//                if (matchIndex <= btLow) { largerPtr=&dummy32; break; }   /* beyond tree size, stop the search */
//                largerPtr = nextPtr;
                matchIndex = nextPtr[0];
            }   }

        *smallerPtr = *largerPtr = 0;

//        if (dictMode == ZSTD_dictMatchState && nbCompares) {
//            bestLength = ZSTD_DUBT_findBetterDictMatch(
//                    ms, ip, iend,
//                    offsetPtr, bestLength, nbCompares,
//                    mls, dictMode);
//        }

        assert(matchEndIdx > current+8); /* ensure nextToUpdate is increased */
        ms->nextToUpdate = matchEndIdx - 8;   /* skip repetitive patterns */
        if (bestLength >= MINMATCH) {
            U32 const mIndex = current - ((U32)*offsetPtr - ZSTD_REP_MOVE); (void)mIndex;
//            DEBUGLOG(8, "ZSTD_DUBT_findBestMatch(%u) : found match of length %u and offsetCode %u (pos %u)",
//                     current, (U32)bestLength, (U32)*offsetPtr, mIndex);
        }
        return bestLength;
    }
}


static void
G_insertDUBT1_Base(matchState* ms,
                   U32 current, const BYTE* inputEnd,
                   U32 nbCompares, U32 btLow,
                   const ZSTD_dictMode_e dictMode)
{
    const G_compressionParameters* const cParams = &ms->cParams;

    U32* const bt     = ms->base_chainTable;

    U32  const btLog  = cParams->base_chainLog - 1;

    U32  const btMask = (1 << btLog) - 1;

    size_t commonLengthSmaller=0, commonLengthLarger=0;

    const BYTE* const base = ms->basebase;

    const BYTE* const ip =  base + current;

    const BYTE* const iend =  ms->basebase + ms->basesize - 7;

    const BYTE* match;
    const BYTE* baseend = ms->basebase + ms->basesize + 1;

    U32* smallerPtr = bt + 2*(current&btMask);
    U32* largerPtr  = smallerPtr + 1;
    U32 matchIndex = *smallerPtr;   /* this candidate is unsorted : next sorted candidate is reached through *smallerPtr, while *largerPtr contains previous unsorted candidate (which is already saved and can be overwritten) */
    U32 dummy32;   /* to be nullified at the end */



    U32 const windowValid = 1;
    U32 const windowLow = windowValid;

    assert(current >= btLow);
    assert(ip <= iend);   /* condition for ZSTD_count */

    while (nbCompares-- && (matchIndex > windowLow)) {
        U32* const nextPtr = bt + 2*(matchIndex & btMask);
//        size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);   /* guaranteed minimum nb of common bytes */
        size_t matchLength = 0;   /* guaranteed minimum nb of common bytes */
        if(matchIndex >= current)
        {
            int a =10;
        }

        assert(matchIndex < current);
        /* note : all candidates are now supposed sorted,
         * but it's still possible to have nextPtr[1] == ZSTD_DUBT_UNSORTED_MARK
         * when a real index has the same value as ZSTD_DUBT_UNSORTED_MARK */

        if ( dictMode != ZSTD_extDict)
        {
            const BYTE* const mBase = ms->basebase;
//            assert( (matchIndex+matchLength >= dictLimit)   /* might be wrong if extDict is incorrectly set to 0 */
//                    || (current < dictLimit) );
            match = mBase + matchIndex;
            matchLength += matchcount(ip+matchLength, baseend, match+matchLength, baseend);
        }
//        else {
//            match = dictBase + matchIndex;
//            matchLength += ZSTD_count_2segments(ip+matchLength, match+matchLength, iend, dictEnd, prefixStart);
//            if (matchIndex+matchLength >= dictLimit)
//                match = base + matchIndex;   /* preparation for next read of match[matchLength] */
//        }
//
//        DEBUGLOG(8, "ZSTD_insertDUBT1: comparing %u with %u : found %u common bytes ",
//                 current, matchIndex, (U32)matchLength);

        if (ip+matchLength == iend) {   /* equal : no way to know if inf or sup */
            break;   /* drop , to guarantee consistency ; miss a bit of compression, but other solutions can corrupt tree */
        }

        if (match[matchLength] < ip[matchLength]) {  /* necessarily within buffer */
            /* match is smaller than current */
            *smallerPtr = matchIndex;             /* update smaller idx */
            commonLengthSmaller = matchLength;    /* all smaller will now have at least this guaranteed common length */
            if (matchIndex <= btLow) { smallerPtr=&dummy32; break; }   /* beyond tree size, stop searching */
//            DEBUGLOG(8, "ZSTD_insertDUBT1: %u (>btLow=%u) is smaller : next => %u",
//                     matchIndex, btLow, nextPtr[1]);
            smallerPtr = nextPtr+1;               /* new "candidate" => larger than match, which was smaller than target */
            matchIndex = nextPtr[1];              /* new matchIndex, larger than previous and closer to current */
        } else {
            /* match is larger than current */
            *largerPtr = matchIndex;
            commonLengthLarger = matchLength;
            if (matchIndex <= btLow) { largerPtr=&dummy32; break; }   /* beyond tree size, stop searching */
//            DEBUGLOG(8, "ZSTD_insertDUBT1: %u (>btLow=%u) is larger => %u",
//                     matchIndex, btLow, nextPtr[0]);
            largerPtr = nextPtr;
            matchIndex = nextPtr[0];
        }   }

    *smallerPtr = *largerPtr = 0;
}


#include "educational_decoder/zstd_decompress.h"
int Gdelta_decompress(seqStore_t* seqstore, BYTE*deltaBuf, U32 deltaSize,BYTE* newBuf,U32 newSize,BYTE* baseBuf,BYTE* restoreBuf,U32* restoresize)
{



    int    seqnum     = seqstore->sequences - seqstore->sequencesStart;
    BYTE*  restore    = (BYTE*)malloc(newSize*2);
    U32    restorelen = 0;
    U32    inpos = 0;
    seqDef* seq = seqstore->sequencesStart;
    U32    offset1 = 1;
    U32    offset2 = 4;
    U32     offset  = 99999;
    U32 litsumlen  = seqstore->lit - seqstore->litStart;
    U32 littmplen     = 0;
    for(int i=0;i<seqnum;i++)
    {
        U32 litlen    = seq[i].litLength;
        U32 offsetmp  = seq[i].offset ;
        U32 matchlen  = (seq[i].matchLength >> 1) + MINMATCH;
        U32 matchflag = seq[i].matchLength & 1U;

        if(matchflag == 1)   /* BASE MATCH */
        {
            offset     =  offsetmp;

            memcpy(restore + restorelen,seqstore->litStart + littmplen,litlen);
            restorelen += litlen;
            littmplen     += litlen;

            inpos      += litlen;



            memcpy(restore+restorelen,baseBuf + offsetmp - 1,matchlen);
            restorelen += matchlen;

            inpos      += matchlen;

        }
        else if(matchflag == 0)  /* SELF MATCH */
        {
            if(offsetmp != 0)
            {
                offset  = offsetmp - ZSTD_REP_MOVE;
                offset2 = offset1;
                offset1 = offset;
            }
            else if(offsetmp == 0)
            {

                offset = offset1;

            }

            memcpy(restore + restorelen,seqstore->litStart + littmplen,litlen);
            restorelen += litlen;
            littmplen  += litlen;

            inpos      += litlen;

            for(U32 j=0;j<matchlen;j++,inpos++,restorelen++)
            {

                memcpy(restore+restorelen,restore+restorelen-offset,1);
            }
        }
    }

    memcpy(restore + restorelen,seqstore->litStart + littmplen,litsumlen - littmplen);
    restorelen += litsumlen - littmplen;
    assert( restorelen == newSize);
    assert(memcmp(newBuf,restore,newSize) == 0);
//    printf("decode succsess\n");
    free(restore);
    return 0;
}


int Gdelta_decompress1(BYTE*deltaBuf, U32 deltaSize,BYTE* baseBuf,U32 basesize,BYTE* restore,U32* restoresize,u_int32_t inputsize)
{

    sequence_command_t* seq = (sequence_command_t*) malloc(sizeof(sequence_command_t) * (65536 / MINMATCH));
    BYTE*   lit = (BYTE*)   malloc(65536);
    uint32_t litsize = 0;
    uint32_t seqnum  =0;
    gdelta_decompress_litandseq((void*) deltaBuf,deltaSize,(void*) lit,(void*) seq,&litsize,&seqnum,inputsize);


    for(uint32_t i =0;i<seqnum;i++)
    {
        seq[i].match_length -= MINMATCH;
        int flag = seq[i].match_length&1U;

        if(seq[i].offset ==1 && flag==0)
        {
            seq[i].offset -=1;
        }


    }

    for(uint32_t i =0;i<seqnum;i++)
    {
//        printf("lit len[%d]:%d\n",i,seq[i].literal_length);
//        printf("matchlen[%d]:%d\n",i,seq[i].match_length );
//        printf("offset[%d]:%d\n",i,seq[i].offset);
    }

    U32    restorelen = 0;
    U32    inpos = 0;

    U32    offset1 = 1;
    U32    offset2 = 4;
    U32     offset  = 99999;
    U32 litsumlen  = litsize;
    U32 littmplen     = 0;
    for(uint32_t i=0;i<seqnum;i++)
    {
        U32 litlen    = seq[i].literal_length;
        U32 offsetmp  = seq[i].offset;
        U32 matchlen  = (seq[i].match_length >> 1) + MINMATCH;
        U32 matchflag = seq[i].match_length & 1U;

        if(matchflag == 1)   /* BASE MATCH */
        {
            offset     =  offsetmp;

            memcpy(restore + restorelen,lit + littmplen,litlen);
            restorelen += litlen;
            littmplen     += litlen;

            inpos      += litlen;



            memcpy(restore+restorelen,baseBuf + offsetmp - 1,matchlen);
            restorelen += matchlen;

            inpos      += matchlen;

        }
        else if(matchflag == 0)  /* SELF MATCH */
        {
            if(offsetmp != 0)
            {
                offset  = offsetmp - ZSTD_REP_MOVE;
                offset2 = offset1;
                offset1 = offset;
            }
            else if(offsetmp == 0)
            {

                offset = offset1;

            }

            memcpy(restore + restorelen,lit + littmplen,litlen);
            restorelen += litlen;
            littmplen  += litlen;

            inpos      += litlen;

            for(U32 j=0;j<matchlen;j++,inpos++,restorelen++)
            {

                memcpy(restore+restorelen,restore+restorelen-offset,1);
            }
        }
    }

    memcpy(restore + restorelen,lit + littmplen,litsumlen - littmplen);
    restorelen += litsumlen - littmplen;

    *restoresize = restorelen;
    free(lit);
    free(seq);
//    assert( restorelen == newSize);
//    assert(memcmp(newBuf,restore,newSize) == 0);
//    printf("decode succsess\n");
    return 0;
}