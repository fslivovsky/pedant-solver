project(glucose)
# cmake_minimum_required(VERSION 3.4...3.18)

set (CMAKE_CXX_STANDARD 17)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

set(main_simp "glucose/glucose_simp/Main.cc")
set(main_parallel "glucose/glucose_parallel/Main.cc")

file(GLOB GlucoseSources RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" glucose/glucose_mtl/*.cc glucose/glucose_core/*.cc 
								glucose/glucose_simp/*.cc glucose/glucose_utils/*.cc)

list(REMOVE_ITEM GlucoseSources ${main_simp} ${main_parallel})


# add_compile_definitions(INCREMENTAL) # requires at least CMake version 3.12
add_definitions(-DINCREMENTAL)

add_library (glucose_library glucose-ipasir.h glucose.cc ${GlucoseSources})


target_include_directories (glucose_library PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/glucose
  ${ZLIB_INCLUDE_DIR})

target_link_libraries(glucose_library PRIVATE ${ZLIB_LIBRARY})
target_compile_options(glucose_library PRIVATE -fPIC)




