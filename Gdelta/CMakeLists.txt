cmake_minimum_required(VERSION 3.10)
project(Gdelta)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0   ")
set(CMAKE_EXPORT_COMPILECOMMANDS ON)

include_directories(.)
include_directories(zstd-dev)



include_directories(zstd-dev/lib)
include_directories(zstd-dev/lib/common)
include_directories(zstd-dev/lib/compress)
include_directories(zstd-dev/doc/educational_decoder)
include_directories(educational_decoder)


aux_source_directory(zstd-dev/lib/common COMMON_SRCS)
aux_source_directory(zstd-dev/lib/compress COMPRESS_SRCS)
aux_source_directory(zstd-dev/lib/dictBuilder DICT_SRCS)
aux_source_directory(zstd-dev/lib/decompress DECOMPRESS_SRCS)


add_executable(Gdelta main.cpp compress.cpp zstdsrc.cpp  zstd-dev/testzstd.c  ${COMPRESS_SRCS}    ${COMMON_SRCS}  educational_decoder/zstd_decompress.c)
add_executable(benchmark benchmark.cpp compress.cpp zstdsrc.cpp  zstd-dev/testzstd.c  ${COMPRESS_SRCS}    ${COMMON_SRCS}  educational_decoder/zstd_decompress.c)
add_executable(testzstd  zstd-dev/testzstd.c main.cpp compress.cpp educational_decoder/zstd_decompress.c ${COMPRESS_SRCS}    ${COMMON_SRCS})
add_executable(edudecode educational_decoder/harness.c educational_decoder/zstd_decompress.c)

add_library(gdelta STATIC compress.cpp zstdsrc.cpp  zstd-dev/testzstd.c  ${COMPRESS_SRCS}    ${COMMON_SRCS}  educational_decoder/zstd_decompress.c)