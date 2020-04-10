//
// Created by haoliang on 2020/2/8.
//
#include <stdio.h>     // printf
#include <stdlib.h>    // free
#include <string.h>    // strlen, strcat, memset
#include <zstd.h>      // presumes zstd library is installed
//#include <zstd_compress.c>
#include <zstd_internal.h>
#include <zstd_compress_internal.h>
#include <zstd_compress_sequences.h>
#include <zstd_compress_literals.h>
#include <cpu.h>
void ZSTD_initCCtx(ZSTD_CCtx* cctx, ZSTD_customMem memManager)
{
    assert(cctx != NULL);
    memset(cctx, 0, sizeof(*cctx));
    cctx->customMem = memManager;
    cctx->bmi2 = ZSTD_cpuid_bmi2(ZSTD_cpuid());
    {   size_t const err = ZSTD_CCtx_reset(cctx, ZSTD_reset_parameters);
        assert(!ZSTD_isError(err));
        (void)err;
    }
}

size_t HIST_countFast_wksp(unsigned* count, unsigned* maxSymbolValuePtr,
                           const void* source, size_t sourceSize,
                           void* workSpace, size_t workSpaceSize);
symbolEncodingType_e
ZSTD_selectEncodingType(
        FSE_repeat* repeatMode, unsigned const* count, unsigned const max,
        size_t const mostFrequent, size_t nbSeq, unsigned const FSELog,
        FSE_CTable const* prevCTable,
        short const* defaultNorm, U32 defaultNormLog,
        ZSTD_defaultPolicy_e const isDefaultAllowed,
        ZSTD_strategy const strategy);






int fseenins(void* src,int nbseq,void* dst)
{

    ZSTD_CCtx* zc = (ZSTD_CCtx*)malloc(sizeof(ZSTD_CCtx));

    int dstCapacity = 10240;

    ZSTD_initCCtx(zc, ZSTD_defaultCMem);

    zc->seqStore.lit=NULL;
    zc->entropyWorkspace = (U32*)malloc( HUF_WORKSPACE_SIZE);

    seqDef*buf = (seqDef*)src;
    zc->seqStore.sequencesStart= buf;
    zc->seqStore.sequences = zc->seqStore.sequencesStart;


    for(int i=0;i<nbseq;i++)
    {

        zc->seqStore.sequencesStart[i].litLength   = buf[i].litLength;
        zc->seqStore.sequencesStart[i].matchLength = buf[i].matchLength;
        zc->seqStore.sequencesStart[i].offset      = buf[i].offset;
        zc->seqStore.sequences++;
    }

    zc->blockState.prevCBlock=(ZSTD_compressedBlockState_t* )malloc(sizeof(ZSTD_compressedBlockState_t));
    zc->appliedParams.cParams.windowLog=10;
    zc->appliedParams.cParams.strategy = ZSTD_btlazy2;

//    ZSTD_compressSequences_internal(&zc->seqStore,
//            &zc->blockState.prevCBlock->entropy,
//            &zc->blockState.nextCBlock->entropy,&zc->appliedParams,
//            dst, dstCapacity,
//            zc->entropyWorkspace, HUF_WORKSPACE_SIZE ,
//                                    1);


    seqStore_t*seqStorePtr = &zc->seqStore;
    seqStorePtr->ofCode = (BYTE*)malloc(10000);
    seqStorePtr->llCode = (BYTE*)malloc(10000);
    seqStorePtr->mlCode = (BYTE*)malloc(10000);

    ZSTD_entropyCTables_t* prevEntropy = (ZSTD_entropyCTables_t*)malloc(sizeof(ZSTD_entropyCTables_t));

    ZSTD_entropyCTables_t* nextEntropy = (ZSTD_entropyCTables_t*)malloc(sizeof(ZSTD_entropyCTables_t));

    const int longOffsets = zc->appliedParams.cParams.windowLog> STREAM_ACCUMULATOR_MIN;
    ZSTD_strategy const strategy = zc->appliedParams.cParams.strategy;
    unsigned count[MaxSeq+1];
//    memset(count,0, sizeof(unsigned)*(MaxSeq+1));
    FSE_CTable* CTable_LitLength = nextEntropy->fse.litlengthCTable;
    FSE_CTable* CTable_OffsetBits = nextEntropy->fse.offcodeCTable;
    FSE_CTable* CTable_MatchLength = nextEntropy->fse.matchlengthCTable;


//    {
//        for(int i=0;i<363;i++)
//        {
//            printf("%d CT ML:%d\n",i,CTable_MatchLength[i]);
//
//        }
//        for(int i=0;i<193;i++)
//        {
//            printf("%d CT OF:%d\n",i,CTable_OffsetBits[i]);
//
//        }
//        for(int i=0;i<329;i++)
//        {
//            printf("%d CT LL:%d\n",i,CTable_LitLength[i]);
//
//        }
//
//    }

    U32 LLtype, Offtype, MLtype;   /* compressed, raw or rle */
    const seqDef* const sequences = seqStorePtr->sequencesStart;
    const BYTE* const ofCodeTable = seqStorePtr->ofCode;
    const BYTE* const llCodeTable = seqStorePtr->llCode;
    const BYTE* const mlCodeTable = seqStorePtr->mlCode;
    BYTE* const ostart = (BYTE*)dst;
    BYTE* const oend = ostart + dstCapacity;
    BYTE* op = ostart;
    size_t const nbSeq = (size_t)(seqStorePtr->sequences - seqStorePtr->sequencesStart);
    BYTE* seqHead;
    BYTE* lastNCount = NULL;
    void* entropyWorkspace= zc->entropyWorkspace;
    size_t entropyWkspSize=HUF_WORKSPACE_SIZE;
    int bmi2 =1;

    /* Compress literals */
    {

        seqStorePtr->litStart = (BYTE*)malloc(100000);
        seqStorePtr->lit = seqStorePtr->litStart;
        FILE* f1 = fopen("/home/thl/test/lit","r");
        fread(seqStorePtr->litStart,1,2983,f1);
        fclose(f1);
        seqStorePtr->lit += 2983;


        const BYTE* const literals = seqStorePtr->litStart;
        size_t const litSize = (size_t)(seqStorePtr->lit - literals);

        struct timeval TvStart,TvEND;


        size_t const cSize = ZSTD_compressLiterals(
                &prevEntropy->huf, &nextEntropy->huf,
                strategy,
                0,
                op, dstCapacity,
                literals, litSize,
                entropyWorkspace, entropyWkspSize,
                bmi2);
        for(int i=0;i<entropyWkspSize;i++)
        {
            printf("%d enttopy:%d\n",i,((BYTE*)entropyWorkspace)[i]);
        }


        double tEncode_e = (TvEND.tv_sec - TvStart.tv_sec) * 1000000 + (TvEND.tv_usec - TvStart.tv_usec) ; // ms
//        printf("Huffman time:%.3fus\n",tEncode_e);
//        printf("Huffman size %d--->%d\n",litSize,cSize);


//        //fse compress
//        uint8_t dst1[1024000];
//        gettimeofday(&TvStart, NULL);
//        size_t fsesize= FSE_compress(dst1,1024000,literals,litSize);
//        gettimeofday(&TvEND, NULL);
//        tEncode_e = (TvEND.tv_sec - TvStart.tv_sec) * 1000000 + (TvEND.tv_usec - TvStart.tv_usec) ; // ms



        FORWARD_IF_ERROR(cSize);
        assert(cSize <= dstCapacity);
        op += cSize;
    }






    seqHead = op++;
    ZSTD_seqToCodes(seqStorePtr);
    /* build CTable for Literal Lengths */
    {   unsigned max = MaxLL;
        size_t const mostFrequent = HIST_countFast_wksp(count, &max, llCodeTable, nbSeq, entropyWorkspace, entropyWkspSize);   /* can't fail */
        DEBUGLOG(5, "Building LL table");
        nextEntropy->fse.litlength_repeatMode = prevEntropy->fse.litlength_repeatMode;
        LLtype = ZSTD_selectEncodingType(&nextEntropy->fse.litlength_repeatMode,
                                         count, max, mostFrequent, nbSeq,
                                         LLFSELog, prevEntropy->fse.litlengthCTable,
                                         LL_defaultNorm, LL_defaultNormLog,
                                         ZSTD_defaultAllowed, strategy);
        assert(set_basic < set_compressed && set_rle < set_compressed);
        assert(!(LLtype < set_compressed && nextEntropy->fse.litlength_repeatMode != FSE_repeat_none)); /* We don't copy tables */

        {   size_t const countSize = ZSTD_buildCTable(
                    op, (size_t)(oend - op),
                    CTable_LitLength, LLFSELog, (symbolEncodingType_e)LLtype,
                    count, max, llCodeTable, nbSeq,
                    LL_defaultNorm, LL_defaultNormLog, MaxLL,
                    prevEntropy->fse.litlengthCTable,
                    sizeof(prevEntropy->fse.litlengthCTable),
                    entropyWorkspace, entropyWkspSize);
            FORWARD_IF_ERROR(countSize);
            if (LLtype == set_compressed)
                lastNCount = op;
            op += countSize;
//            printf("cout:%d\n",countSize);
            assert(op <= oend);
        }   }
    /* build CTable for Offsets */
    {   unsigned max = MaxOff;
        size_t const mostFrequent = HIST_countFast_wksp(
                count, &max, ofCodeTable, nbSeq, entropyWorkspace, entropyWkspSize);  /* can't fail */
        /* We can only use the basic table if max <= DefaultMaxOff, otherwise the offsets are too large */
        ZSTD_defaultPolicy_e const defaultPolicy = (max <= DefaultMaxOff) ? ZSTD_defaultAllowed : ZSTD_defaultDisallowed;
        DEBUGLOG(5, "Building OF table");
        nextEntropy->fse.offcode_repeatMode = prevEntropy->fse.offcode_repeatMode;
        Offtype = ZSTD_selectEncodingType(&nextEntropy->fse.offcode_repeatMode,
                                          count, max, mostFrequent, nbSeq,
                                          OffFSELog, prevEntropy->fse.offcodeCTable,
                                          OF_defaultNorm, OF_defaultNormLog,
                                          defaultPolicy, strategy);
        assert(!(Offtype < set_compressed && nextEntropy->fse.offcode_repeatMode != FSE_repeat_none)); /* We don't copy tables */
        {   size_t const countSize = ZSTD_buildCTable(
                    op, (size_t)(oend - op),
                    CTable_OffsetBits, OffFSELog, (symbolEncodingType_e)Offtype,
                    count, max, ofCodeTable, nbSeq,
                    OF_defaultNorm, OF_defaultNormLog, DefaultMaxOff,
                    prevEntropy->fse.offcodeCTable,
                    sizeof(prevEntropy->fse.offcodeCTable),
                    entropyWorkspace, entropyWkspSize);
            FORWARD_IF_ERROR(countSize);
            if (Offtype == set_compressed)
                lastNCount = op;
            op += countSize;
//            printf("cout:%d\n",countSize);
            assert(op <= oend);
        }   }
    /* build CTable for MatchLengths */
    {   unsigned max = MaxML;
        size_t const mostFrequent = HIST_countFast_wksp(
                count, &max, mlCodeTable, nbSeq, entropyWorkspace, entropyWkspSize);   /* can't fail */
        DEBUGLOG(5, "Building ML table (remaining space : %i)", (int)(oend-op));
        nextEntropy->fse.matchlength_repeatMode = prevEntropy->fse.matchlength_repeatMode;
        MLtype = ZSTD_selectEncodingType(&nextEntropy->fse.matchlength_repeatMode,
                                         count, max, mostFrequent, nbSeq,
                                         MLFSELog, prevEntropy->fse.matchlengthCTable,
                                         ML_defaultNorm, ML_defaultNormLog,
                                         ZSTD_defaultAllowed, strategy);
        assert(!(MLtype < set_compressed && nextEntropy->fse.matchlength_repeatMode != FSE_repeat_none)); /* We don't copy tables */
        {   size_t const countSize = ZSTD_buildCTable(
                    op, (size_t)(oend - op),
                    CTable_MatchLength, MLFSELog, (symbolEncodingType_e)MLtype,
                    count, max, mlCodeTable, nbSeq,
                    ML_defaultNorm, ML_defaultNormLog, MaxML,
                    prevEntropy->fse.matchlengthCTable,
                    sizeof(prevEntropy->fse.matchlengthCTable),
                    entropyWorkspace, entropyWkspSize);
            FORWARD_IF_ERROR(countSize);
            if (MLtype == set_compressed)
                lastNCount = op;
            op += countSize;
//            printf("cout:%d\n",countSize);
            assert(op <= oend);
        }   }

    *seqHead = (BYTE)((LLtype<<6) + (Offtype<<4) + (MLtype<<2));
    for(int i=0;i<nbseq;i++)
    {
        printf("%d ML:%d\n",i,mlCodeTable[i]);
        printf("%d LL:%d\n",i,llCodeTable[i]);
        printf("%d OF:%d\n",i,ofCodeTable[i]);
    }


//    {
//        for(int i=0;i<363;i++)
//        {
//            printf("%d CT ML:%d\n",i,CTable_MatchLength[i]);
//
//        }
//        for(int i=0;i<193;i++)
//        {
//            printf("%d CT OF:%d\n",i,CTable_OffsetBits[i]);
//
//        }
//        for(int i=0;i<329;i++)
//        {
//            printf("%d CT LL:%d\n",i,CTable_LitLength[i]);
//
//        }
//
//    }

    {
        struct timeval TvStart, TvEND;


        size_t const bitstreamSize = ZSTD_encodeSequences(
                op, (size_t) (oend - op),
                CTable_MatchLength, mlCodeTable,
                CTable_OffsetBits, ofCodeTable,
                CTable_LitLength, llCodeTable,
                sequences, nbSeq,
                longOffsets, bmi2); //压缩指令？

//        gettimeofday(&TvEND, NULL);
        double tEncode_e = (TvEND.tv_sec - TvStart.tv_sec) * 1000000 + (TvEND.tv_usec - TvStart.tv_usec); // ms
//        printf("FSE time:%.3fus\n", tEncode_e);
//        for(int i=0;i<bitstreamSize;i++)
//        {
//            printf("%d:%d\n",i+1,op[i]);
//        }


        printf("bitstream size :%d\n", bitstreamSize);
        op += bitstreamSize;
    }

//    printf("all csize:%d\n",op-ostart);
    free(seqStorePtr->ofCode);
    free(seqStorePtr->llCode);
    free(seqStorePtr->mlCode);
    free(zc->blockState.prevCBlock);
    free(zc->entropyWorkspace);
    free(zc);
    free(nextEntropy);
    free(prevEntropy);
    return op-ostart;
}


typedef struct seqDef_s1 {
    u_int32_t offset;
    u_int16_t litLength;
    u_int16_t matchLength;
} seqDef1;


/* ZSTD_compressSequences_internal():
 * actually compresses both literals and sequences */
MEM_STATIC size_t
ZSTD_compressSequences_internal(seqStore_t* seqStorePtr,
                                const ZSTD_entropyCTables_t* prevEntropy,
                                ZSTD_entropyCTables_t* nextEntropy,
                                const ZSTD_CCtx_params* cctxParams,
                                void* dst, size_t dstCapacity,
                                void* entropyWorkspace, size_t entropyWkspSize,
                                const int bmi2)
{
    const int longOffsets = cctxParams->cParams.windowLog > STREAM_ACCUMULATOR_MIN;
    ZSTD_strategy const strategy = cctxParams->cParams.strategy;
    unsigned count[MaxSeq+1] ={0};

//    prevEntropy = (ZSTD_entropyCTables_t*)malloc(sizeof(ZSTD_entropyCTables_t));
//    nextEntropy = (ZSTD_entropyCTables_t*)malloc(sizeof(ZSTD_entropyCTables_t));

    FSE_CTable* CTable_LitLength = nextEntropy->fse.litlengthCTable;
    FSE_CTable* CTable_OffsetBits = nextEntropy->fse.offcodeCTable;
    FSE_CTable* CTable_MatchLength = nextEntropy->fse.matchlengthCTable;
    U32 LLtype, Offtype, MLtype;   /* compressed, raw or rle */
    const seqDef* const sequences = seqStorePtr->sequencesStart;
    const BYTE* const ofCodeTable = seqStorePtr->ofCode;
    const BYTE* const llCodeTable = seqStorePtr->llCode;
    const BYTE* const mlCodeTable = seqStorePtr->mlCode;

//    entropyWorkspace = malloc(entropyWkspSize);



    BYTE* const ostart = (BYTE*)dst;
    BYTE* const oend = ostart + dstCapacity;
    BYTE* op = ostart;
    size_t const nbSeq = (size_t)(seqStorePtr->sequences - seqStorePtr->sequencesStart);
    BYTE* seqHead;
    BYTE* lastNCount = NULL;

    DEBUGLOG(5, "ZSTD_compressSequences_internal (nbSeq=%zu)", nbSeq);
    ZSTD_STATIC_ASSERT(HUF_WORKSPACE_SIZE >= (1<<MAX(MLFSELog,LLFSELog)));


    /* Compress literals */
    {   const BYTE* const literals = seqStorePtr->litStart;
        size_t const litSize = (size_t)(seqStorePtr->lit - literals);
//        {
//            FILE* f1 = fopen("/home/thl/test/lit","w");
//            fwrite(literals,1,litSize,f1);
//            fclose(f1);
//        }
        size_t const cSize = ZSTD_compressLiterals(
                &prevEntropy->huf, &nextEntropy->huf,
                cctxParams->cParams.strategy,
                0,
                op, dstCapacity,
                literals, litSize,
                entropyWorkspace, entropyWkspSize,
                bmi2);
//        printf("Huf size:%d --> %d \n",litSize,cSize);
        FORWARD_IF_ERROR(cSize);
        assert(cSize <= dstCapacity);
        op += cSize;
    }

    /* Sequences Header */
    RETURN_ERROR_IF((oend-op) < 3 /*max nbSeq Size*/ + 1 /*seqHead*/,
                    dstSize_tooSmall);
    if (nbSeq < 128) {
        *op++ = (BYTE)nbSeq;
    } else if (nbSeq < LONGNBSEQ) {
        op[0] = (BYTE)((nbSeq>>8) + 0x80);
        op[1] = (BYTE)nbSeq;
        op+=2;
    } else {
        op[0]=0xFF;
        MEM_writeLE16(op+1, (U16)(nbSeq - LONGNBSEQ));
        op+=3;
    }
    assert(op <= oend);
    if (nbSeq==0) {
        /* Copy the old tables over as if we repeated them */
        memcpy(&nextEntropy->fse, &prevEntropy->fse, sizeof(prevEntropy->fse));
        return (size_t)(op - ostart);
    }

    /* seqHead : flags for FSE encoding type */
    seqHead = op++;
    assert(op <= oend);

    /* convert length/distances into codes */
    ZSTD_seqToCodes(seqStorePtr);
    /* build CTable for Literal Lengths */
    {   unsigned max = MaxLL;
        size_t const mostFrequent = HIST_countFast_wksp(count, &max, llCodeTable, nbSeq, entropyWorkspace, entropyWkspSize);   /* can't fail */
        DEBUGLOG(5, "Building LL table");
        nextEntropy->fse.litlength_repeatMode = prevEntropy->fse.litlength_repeatMode;
        LLtype = ZSTD_selectEncodingType(&nextEntropy->fse.litlength_repeatMode,
                                         count, max, mostFrequent, nbSeq,
                                         LLFSELog, prevEntropy->fse.litlengthCTable,
                                         LL_defaultNorm, LL_defaultNormLog,
                                         ZSTD_defaultAllowed, strategy);
        assert(set_basic < set_compressed && set_rle < set_compressed);
        assert(!(LLtype < set_compressed && nextEntropy->fse.litlength_repeatMode != FSE_repeat_none)); /* We don't copy tables */
        {   size_t const countSize = ZSTD_buildCTable(
                    op, (size_t)(oend - op),
                    CTable_LitLength, LLFSELog, (symbolEncodingType_e)LLtype,
                    count, max, llCodeTable, nbSeq,
                    LL_defaultNorm, LL_defaultNormLog, MaxLL,
                    prevEntropy->fse.litlengthCTable,
                    sizeof(prevEntropy->fse.litlengthCTable),
                    entropyWorkspace, entropyWkspSize);
            FORWARD_IF_ERROR(countSize);
            if (LLtype == set_compressed)
                lastNCount = op;
            op += countSize;
            assert(op <= oend);
        }   }
    /* build CTable for Offsets */
    {   unsigned max = MaxOff;
        size_t const mostFrequent = HIST_countFast_wksp(
                count, &max, ofCodeTable, nbSeq, entropyWorkspace, entropyWkspSize);  /* can't fail */
        /* We can only use the basic table if max <= DefaultMaxOff, otherwise the offsets are too large */
        ZSTD_defaultPolicy_e const defaultPolicy = (max <= DefaultMaxOff) ? ZSTD_defaultAllowed : ZSTD_defaultDisallowed;
        DEBUGLOG(5, "Building OF table");
        nextEntropy->fse.offcode_repeatMode = prevEntropy->fse.offcode_repeatMode;
        Offtype = ZSTD_selectEncodingType(&nextEntropy->fse.offcode_repeatMode,
                                          count, max, mostFrequent, nbSeq,
                                          OffFSELog, prevEntropy->fse.offcodeCTable,
                                          OF_defaultNorm, OF_defaultNormLog,
                                          defaultPolicy, strategy);
        assert(!(Offtype < set_compressed && nextEntropy->fse.offcode_repeatMode != FSE_repeat_none)); /* We don't copy tables */
        {   size_t const countSize = ZSTD_buildCTable(
                    op, (size_t)(oend - op),
                    CTable_OffsetBits, OffFSELog, (symbolEncodingType_e)Offtype,
                    count, max, ofCodeTable, nbSeq,
                    OF_defaultNorm, OF_defaultNormLog, DefaultMaxOff,
                    prevEntropy->fse.offcodeCTable,
                    sizeof(prevEntropy->fse.offcodeCTable),
                    entropyWorkspace, entropyWkspSize);
            FORWARD_IF_ERROR(countSize);
            if (Offtype == set_compressed)
                lastNCount = op;
            op += countSize;
            assert(op <= oend);
        }   }
    /* build CTable for MatchLengths */
    {   unsigned max = MaxML;
        size_t const mostFrequent = HIST_countFast_wksp(
                count, &max, mlCodeTable, nbSeq, entropyWorkspace, entropyWkspSize);   /* can't fail */
        DEBUGLOG(5, "Building ML table (remaining space : %i)", (int)(oend-op));
        nextEntropy->fse.matchlength_repeatMode = prevEntropy->fse.matchlength_repeatMode;
        MLtype = ZSTD_selectEncodingType(&nextEntropy->fse.matchlength_repeatMode,
                                         count, max, mostFrequent, nbSeq,
                                         MLFSELog, prevEntropy->fse.matchlengthCTable,
                                         ML_defaultNorm, ML_defaultNormLog,
                                         ZSTD_defaultAllowed, strategy);
        assert(!(MLtype < set_compressed && nextEntropy->fse.matchlength_repeatMode != FSE_repeat_none)); /* We don't copy tables */
        {   size_t const countSize = ZSTD_buildCTable(
                    op, (size_t)(oend - op),
                    CTable_MatchLength, MLFSELog, (symbolEncodingType_e)MLtype,
                    count, max, mlCodeTable, nbSeq,
                    ML_defaultNorm, ML_defaultNormLog, MaxML,
                    prevEntropy->fse.matchlengthCTable,
                    sizeof(prevEntropy->fse.matchlengthCTable),
                    entropyWorkspace, entropyWkspSize);
            FORWARD_IF_ERROR(countSize);
            if (MLtype == set_compressed)
                lastNCount = op;
            op += countSize;
            assert(op <= oend);
        }   }

    *seqHead = (BYTE)((LLtype<<6) + (Offtype<<4) + (MLtype<<2));


//    {
//        FILE* f1 = fopen("/home/thl/test/seq","w");
//        fwrite(sequences,1,(seqStorePtr->sequences-seqStorePtr->sequencesStart)* sizeof(seqDef),f1);
//        fclose(f1);
//    }


    {   size_t const bitstreamSize = ZSTD_encodeSequences(
                op, (size_t)(oend - op),
                CTable_MatchLength, mlCodeTable,
                CTable_OffsetBits, ofCodeTable,
                CTable_LitLength, llCodeTable,
                sequences, nbSeq,
                longOffsets, bmi2);
        FORWARD_IF_ERROR(bitstreamSize);
//        printf("fse num:%d\n",nbSeq);
//        printf("fsesize:%d\n",bitstreamSize);
//        for(int i=0;i<bitstreamSize;i++)
//        {
//            printf("%d op[i]:%d\n",i,op[i]);
//        }
        op += bitstreamSize;
        assert(op <= oend);
        /* zstd versions <= 1.3.4 mistakenly report corruption when
         * FSE_readNCount() receives a buffer < 4 bytes.
         * Fixed by https://github.com/facebook/zstd/pull/1146.
         * This can happen when the last set_compressed table present is 2
         * bytes and the bitstream is only one byte.
         * In this exceedingly rare case, we will simply emit an uncompressed
         * block, since it isn't worth optimizing.
         */
        if (lastNCount && (op - lastNCount) < 4) {
            /* NCountSize >= 2 && bitstreamSize > 0 ==> lastCountSize == 3 */
            assert(op - lastNCount == 3);
            DEBUGLOG(5, "Avoiding bug in zstd decoder in versions <= 1.3.4 by "
                        "emitting an uncompressed block.");
            return 0;
        }
    }

    DEBUGLOG(5, "compressed block size : %u", (unsigned)(op - ostart));
    for(int i=0;i<op - ostart;i++)
    {
//        printf("%d op[i]:%d\n",i,ostart[i]);
    }
    return (size_t)(op - ostart);
}



size_t compressLitandSeq(BYTE* dst, size_t dstcapacity, seqStore_t* seqStorePtr,size_t newSize)
{


    size_t Maxseq = newSize/4;

    seqStorePtr->llCode =(BYTE*)malloc(Maxseq * sizeof(U32));
    memset(seqStorePtr->llCode,0,Maxseq * sizeof(U32));
    seqStorePtr->mlCode =(BYTE*)malloc(Maxseq * sizeof(U32));
    memset(seqStorePtr->llCode,0,Maxseq * sizeof(U32));
    seqStorePtr->ofCode =(BYTE*)malloc(Maxseq * sizeof(U32));
    memset(seqStorePtr->llCode,0,Maxseq * sizeof(U32));

    seqStorePtr->maxNbSeq = Maxseq;
    seqStorePtr->maxNbLit = newSize;
    ZSTD_entropyCTables_t* prevEntropy = (ZSTD_entropyCTables_t*)malloc(sizeof(ZSTD_entropyCTables_t));
    ZSTD_entropyCTables_t* nextEntropy = (ZSTD_entropyCTables_t*)malloc(sizeof(ZSTD_entropyCTables_t));


    prevEntropy->huf.repeatMode = HUF_repeat_none;
    nextEntropy->huf.repeatMode = HUF_repeat_none;



    prevEntropy->fse.matchlength_repeatMode = FSE_repeat_none;
    prevEntropy->fse.offcode_repeatMode     =FSE_repeat_none;
    prevEntropy->fse.litlength_repeatMode   =FSE_repeat_none;

    nextEntropy->fse.matchlength_repeatMode = FSE_repeat_none;
    nextEntropy->fse.offcode_repeatMode     =FSE_repeat_none;
    nextEntropy->fse.litlength_repeatMode   =FSE_repeat_none;

    ZSTD_CCtx_params* cctxParams = (ZSTD_CCtx_params*)malloc(sizeof(ZSTD_CCtx_params));
    cctxParams->cParams.minMatch = 4;
    cctxParams->cParams.windowLog =14;
    cctxParams->cParams.chainLog =15;
    cctxParams->cParams.searchLog = 9;
    cctxParams->cParams.strategy = ZSTD_btlazy2;
    cctxParams->cParams.targetLength =8;
    cctxParams->cParams.hashLog =14;
    cctxParams->fParams.contentSizeFlag = 1;
    cctxParams->compressionLevel = 3;

    size_t entropyWkspSize = HUF_WORKSPACE_SIZE;
    void* entropyWorkspace = malloc(entropyWkspSize * sizeof(U32));
    int bim2 = 1;

    size_t csize = ZSTD_compressSequences_internal(seqStorePtr,prevEntropy,nextEntropy
            ,cctxParams,dst,dstcapacity,entropyWorkspace,entropyWkspSize,bim2);

//    {
//        FILE* F1 = fopen("/home/thl/data/lit","w");
//        fwrite(seqStorePtr->litStart,1,seqStorePtr->lit - seqStorePtr->litStart,F1);
//        fclose(F1);
//        FILE* F2 = fopen("/home/thl/data/seq","w");
//        fwrite(seqStorePtr->sequencesStart,1,(seqStorePtr->sequences - seqStorePtr->sequencesStart)*sizeof(seqDef),F1);
//        fclose(F2);
//
//    }
    free(entropyWorkspace);
    free(seqStorePtr->llCode);
    free(seqStorePtr->ofCode);
    free(seqStorePtr->mlCode);
    free(prevEntropy);
    free(nextEntropy);
    free(cctxParams);

    return csize;
}

int ain()
{

    seqStore_t* seqStorePtr = malloc(sizeof(seqStore_t));
    seqStorePtr->litStart =(BYTE*)malloc(108*1024);
    seqStorePtr->lit = seqStorePtr->litStart;

    FILE* f1 = fopen("/home/thl/test/lit","r");
    fread(seqStorePtr->litStart,1,4389,f1);
    fclose(f1);
    seqStorePtr->lit += 4389;

    seqStorePtr->sequencesStart=(seqDef*)malloc(10000* sizeof(seqDef));
    seqStorePtr->sequences = seqStorePtr->sequencesStart;
    FILE* f2 = fopen("/home/thl/test/seq","r");
    int seqsize =3288;
    fread(seqStorePtr->sequencesStart,1,seqsize,f2);
    fclose(f2);
    for(int i =0;i<(3288/8);i++)
    {
        seqStorePtr->sequences++;
    }

    seqStorePtr->llCode =(BYTE*)malloc(100000);
    memset(seqStorePtr->llCode,0,100000);
    seqStorePtr->mlCode =(BYTE*)malloc(100000);
    memset(seqStorePtr->llCode,0,100000);
    seqStorePtr->ofCode =(BYTE*)malloc(100000);
    memset(seqStorePtr->llCode,0,100000);
    seqStorePtr->maxNbSeq = 3045;
    seqStorePtr->maxNbLit = 12180;
    ZSTD_entropyCTables_t* prevEntropy = (ZSTD_entropyCTables_t*)malloc(sizeof(ZSTD_entropyCTables_t));
    ZSTD_entropyCTables_t* nextEntropy = (ZSTD_entropyCTables_t*)malloc(sizeof(ZSTD_entropyCTables_t));

    ZSTD_CCtx_params* cctxParams = (ZSTD_CCtx_params*)malloc(sizeof(ZSTD_CCtx_params));
    cctxParams->cParams.minMatch = 4;
    cctxParams->cParams.windowLog =14;
    cctxParams->cParams.chainLog =15;
    cctxParams->cParams.searchLog = 9;
    cctxParams->cParams.strategy = ZSTD_btlazy2;
    cctxParams->cParams.targetLength =8;
    cctxParams->cParams.hashLog =14;
    cctxParams->fParams.contentSizeFlag = 1;
    cctxParams->compressionLevel = 3;

    BYTE*  dst = malloc(100000);
    size_t dstcapacity = 12275;


    size_t entropyWkspSize = HUF_WORKSPACE_SIZE;
    void* entropyWorkspace = malloc(entropyWkspSize * sizeof(U32));
    int bim2 = 1;

    size_t  csize = compressLitandSeq(dst,dstcapacity,seqStorePtr,12275);


//    size_t csize = ZSTD_compressSequences_internal(seqStorePtr,prevEntropy,nextEntropy
//            ,cctxParams,dst,dstcapacity,entropyWorkspace,entropyWkspSize,bim2);

//    void* dst =malloc(1024*1024*1000);
//    seqDef* src = (seqDef*)malloc(1024*1024*1000);
//    FILE* f1 = fopen("/home/thl/test/seq","r");
//    int seqsize =5856;
//    fread(src,1,seqsize,f1);
//    fclose(f1);
//    int seqnum = seqsize/8;
//    int i = fseenins((void*)(src),seqnum,dst);
//    printf("%d\n",i);

    return 0;
}

//#define  NUM 10000
//int man()
//{
//
//    uint32_t* num = (uint32_t*)malloc(3403440);
//
//    void* dst =malloc(1024*1024*1000);
//    seqDef* src = (seqDef*)malloc(1024*1024*1000);
//    FILE* f1 = fopen("/home/thl/test/seq","r");
//    fread(src,1,10016,f1);
//    fclose(f1);
//
////    FILE*f2 = fopen("/home/thl/test/num","r");
////    fread(num,1,3403440,f2);
////    fclose(f2);
//
//    int nbseq =10016/8;
//    int size=0;
//    int seqnum;
//    uint32_t readlen=0;
//    int count = 0;
//    while(nbseq > NUM)
//    {
//        seqnum = NUM;
//        count++;
//        int i = fseenins((void*)(src+readlen),seqnum,dst);
//        readlen +=seqnum;
//        nbseq -= seqnum;
//        size +=i;
//    }
//
//    int i = fseenins((void*)(src+readlen),nbseq,dst);
//    size +=i;
//
//    printf("%d\n",size);
//
////    for(int i=0;i<size;i++)
////    {
////        printf("%c\n",((char*)dst)[i]);
////    }
//    return 0;
//}


