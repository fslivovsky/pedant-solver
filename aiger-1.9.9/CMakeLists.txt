project(aiger LANGUAGES C)
cmake_minimum_required(VERSION 3.4...3.18)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR ) 
  message (FATAL_ERROR
    "In-source builds are not allowed. Please clean your source tree and try again.")  
endif()

add_compile_options (-O3 -DNDEBUG)

add_library(aiger_library aiger.h aiger.c)

add_executable(aigtoaig aigtoaig.c)
target_link_libraries(aigtoaig PRIVATE aiger_library)

#add_executable(aigsplit aigsplit.c)
#target_link_libraries(aigsplit PRIVATE aiger_library)

#add_executable(aigtocnf aigtocnf.c)
#target_link_libraries(aigtocnf PRIVATE aiger_library)
