cmake_minimum_required(VERSION 3.10)
project(zstd_dev)

#set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 ")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 ")

include_directories(lib)
include_directories(lib/common)
include_directories(lib/compress)
include_directories(doc/educational_decoder)

aux_source_directory(lib/common COMMON_SRCS)
aux_source_directory(lib/compress COMPRESS_SRCS)
aux_source_directory(lib/dictBuilder DICT_SRCS)
aux_source_directory(lib/decompress DECOMPRESS_SRCS)


add_executable(compress  examples/simple_compression.c  ${COMPRESS_SRCS}    ${COMMON_SRCS}   ${DICT_SRCS} )
add_executable(stream examples/streaming_compression.c  ${COMPRESS_SRCS}    ${COMMON_SRCS}   ${DICT_SRCS} )
add_executable(dictionary examples/dictionary_compression.c  ${COMPRESS_SRCS}    ${COMMON_SRCS}   ${DICT_SRCS} )
add_executable(decompress examples/simple_decompression.c  ${DECOMPRESS_SRCS}    ${COMMON_SRCS}   ${DICT_SRCS}  ${COMPRESS_SRCS}  )
add_executable(edudecode doc/educational_decoder/harness.c doc/educational_decoder/zstd_decompress.c)
LINK_DIRECTORIES(.)

add_executable(testfse  testzstd.c  ${COMPRESS_SRCS}    ${COMMON_SRCS}    )




#add_executable(testfse  testzstd.c  ${COMPRESS_SRCS}    ${COMMON_SRCS}  )

#add_library(testfse  STATIC  testzstd.c  ${COMPRESS_SRCS}    ${COMMON_SRCS}   ${DICT_SRCS}  ${DECOMPRESS_SRCS}  )


add_executable(testans test.c)
TARGET_LINK_LIBRARIES(testans -ltestfse )