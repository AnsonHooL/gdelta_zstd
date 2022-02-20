//
// Created by haoliang on 2020/3/14.
//

#ifndef G_ZETDSRCH
#define G_ZETDSRCH

#include <cstdint>
#include <assert.h>
#include <cstddef>
#include "mem.h"
typedef uint8_t BYTE;
typedef uint8_t uint8;
typedef uint32_t U32;

/*-*************************************
*  Constants
***************************************/
#define kSearchStrength      8
#define HASH_READ_SIZE       8
#define ZSTD_DUBT_UNSORTED_MARK 1   /* For btlazy2 strategy, index ZSTD_DUBT_UNSORTED_MARK==1 means "unsorted".
                                       It could be confused for a real successor at index "1", if sorted as larger than its predecessor.
                                       It's not a big deal though : candidate will just be sorted again.
                                       Additionally, candidate position 1 will be lost.
                                       But candidate 1 cannot hide a large tree of candidates, so it's a minimal loss.
                                       The benefit is that ZSTD_DUBT_UNSORTED_MARK cannot be mishandled after table re-use with a different strategy.
                                       This constant is required by ZSTD_compressBlock_btlazy2() and ZSTD_reduceTable_internal() */

#define ZSTD_REP_NUM      3                 /* number of repcodes */
#define ZSTD_REP_MOVE     (ZSTD_REP_NUM-1)


#define MINMATCH 3 // TODO:4 or 3 for compression??

/*-*************************************
*  shared macros
***************************************/
#undef MIN
#undef MAX
#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))




#if defined(__GNUC__)
#  define MEM_STATIC static __inline __attribute__((unused))
#elif defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#  define MEM_STATIC static inline
#elif defined(_MSC_VER)
#  define MEM_STATIC static __inline
#else
#  define MEM_STATIC static  /* this version may generate warnings for unused static functions; disable the relevant warning */
#endif

MEM_STATIC U32 G_highbit32(U32 val)   /* compress, dictBuilder, decodeCorpus */
{
    assert(val != 0);
    {
#   if defined(_MSC_VER)   /* Visual */
        unsigned long r=0;
        _BitScanReverse(&r, val);
        return (unsigned)r;
#   elif defined(__GNUC__) && (__GNUC__ >= 3)   /* GCC Intrinsic */
        return __builtin_clz (val) ^ 31;
#   elif defined(__ICCARM__)    /* IAR Intrinsic */
        return 31 - __CLZ(val);
#   else   /* Software version */
        static const U32 DeBruijnClz[32] = { 0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30, 8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31 };
        U32 v = val;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        return DeBruijnClz[(v * 0x07C4ACDDU) >> 27];
#   endif
    }
}




/*-*************************************
 *  Hashes
 ***************************************/
static const U32 prime3bytes = 506832829U;
static U32    ZSTD_hash3(U32 u, U32 h) { return ((u << (32-24)) * prime3bytes)  >> (32-h) ; }
MEM_STATIC size_t ZSTD_hash3Ptr(const void* ptr, U32 h) { return ZSTD_hash3(MEM_readLE32(ptr), h); } /* only in zstd_opt.h */

static const U32 prime4bytes = 2654435761U;
static U32    ZSTD_hash4(U32 u, U32 h) { return (u * prime4bytes) >> (32-h) ; }
static size_t ZSTD_hash4Ptr(const void* ptr, U32 h) { return ZSTD_hash4(MEM_read32(ptr), h); }

static const U64 prime5bytes = 889523592379ULL;
static size_t ZSTD_hash5(U64 u, U32 h) { return (size_t)(((u  << (64-40)) * prime5bytes) >> (64-h)) ; }
static size_t ZSTD_hash5Ptr(const void* p, U32 h) { return ZSTD_hash5(MEM_readLE64(p), h); }

static const U64 prime6bytes = 227718039650203ULL;
static size_t ZSTD_hash6(U64 u, U32 h) { return (size_t)(((u  << (64-48)) * prime6bytes) >> (64-h)) ; }
static size_t ZSTD_hash6Ptr(const void* p, U32 h) { return ZSTD_hash6(MEM_readLE64(p), h); }

static const U64 prime7bytes = 58295818150454627ULL;
static size_t ZSTD_hash7(U64 u, U32 h) { return (size_t)(((u  << (64-56)) * prime7bytes) >> (64-h)) ; }
static size_t ZSTD_hash7Ptr(const void* p, U32 h) { return ZSTD_hash7(MEM_readLE64(p), h); }

static const U64 prime8bytes = 0xCF1BBCDCB7A56463ULL;
static size_t ZSTD_hash8(U64 u, U32 h) { return (size_t)(((u) * prime8bytes) >> (64-h)) ; }
static size_t ZSTD_hash8Ptr(const void* p, U32 h) { return ZSTD_hash8(MEM_readLE64(p), h); }

MEM_STATIC size_t ZSTD_hashPtr(const void* p, U32 hBits, U32 mls)
{
    switch(mls)
    {
        default:
        case 4: return ZSTD_hash4Ptr(p, hBits);
        case 5: return ZSTD_hash5Ptr(p, hBits);
        case 6: return ZSTD_hash6Ptr(p, hBits);
        case 7: return ZSTD_hash7Ptr(p, hBits);
        case 8: return ZSTD_hash8Ptr(p, hBits);
    }
}








#endif //G_ZETDSRCH
