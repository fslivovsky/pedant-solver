cmake_minimum_required(VERSION 3.4...3.18)
project(pedant LANGUAGES CXX)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release")
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
add_compile_options (-fpic)

include_directories(include)
include_directories(interrupt)

add_subdirectory(interrupt)
add_subdirectory(cadical_ipasir)
add_subdirectory(abc)
add_subdirectory(minisat)
add_subdirectory(avy)
add_subdirectory(interpolatingsolver)
add_subdirectory(aiger-1.9.9)

find_package(Abc REQUIRED)
if (ABC_FOUND)
  include_directories(${ABC_INCLUDE_DIR})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ABC_CXXFLAGS}")
  add_definitions (-DABC_NAMESPACE=${ABC_NAMESPACE})
else()
  set(ABC_LIBRARY "")
endif ()

find_package(Minisat REQUIRED)
if (MINISAT_FOUND)
  include_directories(${MINISAT_INCLUDE_DIR})
else()
  set (MINISAT_LIBRARY "")
endif()

find_package(Avy REQUIRED)
if (AVY_FOUND)
  include_directories(${AVY_SOURCE_DIR} ${AVY_INCLUDE_DIR})
endif ()

find_package(InterpolatingSolver REQUIRED)
if (InterpolatingSolver_FOUND)
  include_directories(${INTERPOLATING_SOLVER_INCLUDE_DIR})
endif()

option(USE_ML "Use machine learning - requires MLPack " ON)
if (USE_ML)
  find_package(MLPACK REQUIRED)
  add_compile_options (-fopenmp)
  if ( ${MLPACK_VERSION_STRING} VERSION_LESS 3.2 )
	message(SEND_ERROR "MLPack version ${MLPACK_VERSION_STRING} is provided but at least version 3.2 is required.")
  endif ()
endif()


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(cadical_ipasir)
include_directories(aiger-1.9.9)
add_subdirectory(src)
