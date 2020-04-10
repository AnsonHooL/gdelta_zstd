//
// Created by haoliang on 2020/3/25.
//

#ifndef GDELTA_TESTZSTD_H
#define GDELTA_TESTZSTD_H

#if defined (__cplusplus)
extern "C" {
#endif

#include <zstd.h>
#include <zstd_internal.h>

size_t compressLitandSeq(BYTE* dst, size_t dstcapacity, seqStore_t* seqStorePtr,size_t newSize);





#if defined (__cplusplus)
}
#endif

#endif //GDELTA_TESTZSTD_H
