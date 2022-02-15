//
// Created by haoliang on 2020/3/14.
//

#ifndef GDELTA_COMPRESS_H
#define GDELTA_COMPRESS_H

#define MBSIZE 1024*1024
#include "mem.h"
#include "zstdsrc.h"
#include "zstd_internal.h"
typedef uint32_t UINT32;
typedef uint32_t U32;








typedef enum { ZSTD_noDict = 0, ZSTD_extDict = 1, ZSTD_dictMatchState = 2 } ZSTD_dictMode_e;
typedef enum { SELF_MATCH= 0, BASE_MATCH = 1 , NO_MATCH = 2 } G_MATCHFLAG;

typedef struct
{
    unsigned windowLog;       /**< largest match distance : larger == more compression, more memory needed during decompression */
    unsigned chainLog;        /**< fully searched segment : larger == more compression, slower, more memory (useless for fast) */
    unsigned hashLog;         /**< dispatch table : larger == faster, more memory */
    unsigned searchLog;       /**< nb of searches : larger == more compression, slower */
//    unsigned minMatch;        /**< match length searched : larger == faster decompression, sometimes less compression */
//    unsigned targetLength;    /**< acceptable match size for optimal parser (only) : larger == more compression, slower */
    unsigned base_chainLog;        /**< fully searched segment : larger == more compression, slower, more memory (useless for fast) */
    unsigned base_hashLog;         /**< dispatch table : larger == faster, more memory */
}G_compressionParameters;

typedef struct matchState matchState;

struct matchState {

    UINT32   nextToUpdate;       /* index from which to continue table update */
    UINT32   base_nextToUpdate;
    U32*  self_hashTable;
    U32*  self_chainTable;
    BYTE* selfbase;  /* selfbase = src - 1*/


    U32*    base_hashTable;
    U32*    base_chainTable;
    BYTE*   basebase; /*basebase = src -1*/
    U32     basesize;


    UINT32 depth;
    G_compressionParameters cParams;
    ZSTD_dictMode_e mode;
    int endmatch;
    U32 endmatchlen;
    U32 endoffset;


};


typedef struct {
    U32 literal_length;
    U32 match_length;
    U32 offset;
} sequence_command_t;

//typedef struct seqDef_s1 {
//    U32    offset;
//    U16  litLength;
//    U16  matchLength;
//} seqDefine;

//typedef struct {
//    seqDefine* sequencesStart;
//    seqDefine* sequences;
//    BYTE*  litStart;
//    BYTE*  lit;
//    BYTE*  signal;// 0 or 1 to represent self match or base match!!!
//    BYTE*  signalStart;
////    BYTE* llCode;
////    BYTE* mlCode;
////    BYTE* ofCode;
//    size_t maxNbSeq;
//    size_t maxNbLit;
////    UINT32   longLengthID;   /* 0 == no longLength; 1 == Lit.longLength; 2 == Match.longLength; */
////    UINT32   longLengthPos;
//} seqStore_t;




int gdelta_Encode_Helper( uint8_t* newBuf, U32 newSize,
                          matchState* ms,seqStore_t* seqstore);

int gdelta_Encode_Helper1( uint8_t* newBuf, U32 newSize,
                          matchState* ms,seqStore_t* seqstore);

int gdelta_Encode_Helper2( uint8_t* newBuf, U32 newSize,
                           matchState* ms,seqStore_t* seqstore);

int matchcount(const BYTE * ip,const BYTE* iend,const BYTE* basematch,const BYTE* baseend);

void storeSeq(seqStore_t* seqStorePtr, size_t litLength, const BYTE* literals, const BYTE* litLimit, U32 offCode, size_t mlBase);

inline size_t
G_BtFindBestMatch( matchState* ms,const BYTE* const ip, const BYTE* const iLimit,size_t* offsetPtr,
const U32 mls /* template */,const ZSTD_dictMode_e dictMode);


inline size_t
G_BtFindBestMatch_Base( matchState* ms,const BYTE* const ip, const BYTE* const iLimit,size_t* offsetPtr,
                   const U32 mls /* template */,const ZSTD_dictMode_e dictMode);


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

void InitMSandSeq(matchState* ms, uint8_t* src, U32 srcsize,seqStore_t* seqstore);

void DestroyMSandSeq(matchState* ms,seqStore_t* seq);

void SaveCompress(seqStore_t* seqstore,uint8_t* deltabuf,U32* deltasize);

int gdelta_Encode( uint8_t* newBuf, U32 newSize,
                   uint8_t* baseBuf, U32 baseSize,
                   uint8_t* dataBuf, uint8* seqbuf,
                   U32* datasize,uint32_t* seqsize);


extern "C" {
int fseenins(void* src,int nbseq,void* dst);
}


int gdelta_Encode_Helper( uint8_t* newBuf, U32 newSize,
                          matchState* ms,seqStore_t* seqstore);

int Gdelta_decompress(seqStore_t* seqstore,BYTE*deltaBuf,U32 deltaSize,BYTE* newBuf,U32 newSize,BYTE* baseBuf,BYTE* restoreBuf,U32* restoresize);

int Gdelta_decompress1(BYTE*deltaBuf, U32 deltaSize,BYTE* baseBuf,U32 basesize,BYTE* restore,U32* restoresize,U32 inputsize);
#endif //GDELTA_COMPRESS_H


