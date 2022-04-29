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

option(STATIC_BUILD "Build a static binary." OFF)
if(STATIC_BUILD)
  # If machine learning shall be enabled you have to provide a static build of MLPack
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  set(BUILD_SHARED_LIBS OFF)
  set(CMAKE_EXE_LINKER_FLAGS "-static")
  set(Boost_USE_STATIC_LIBS        ON)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
add_compile_options (-fpic)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
 #add_compile_options (-Og)
endif() 

include_directories(include)
include_directories(interrupt)

add_subdirectory(interrupt)
add_subdirectory(cadical_ipasir)
add_subdirectory(abc)
add_subdirectory(minisat)
add_subdirectory(avy)
add_subdirectory(interpolatingsolver)
add_subdirectory(glucose)
add_subdirectory(aiger-1.9.9)
add_subdirectory(docopt)


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

option(USE_ML "Use machine learning - requires MLPack " OFF)
if (USE_ML)
  find_package(MLPACK)
  if (MLPACK_FOUND)
  	if ( ${MLPACK_VERSION_STRING} VERSION_LESS 3.2 )
		message(WARNING "MLPack version ${MLPACK_VERSION_STRING} is provided but at least version 3.2 is required. Proceeding with USE_ML=OFF.")
		set (USE_ML OFF)
	else ()
		add_compile_options (-fopenmp)
  	endif ()	
  else ()
    message( WARNING "To use the option USE_ML, MLPack needs to be installed. Proceeding with USE_ML=OFF.")
  	set (USE_ML OFF)
  endif ()
  if (NOT STATIC_BUILD)
	  # Look for Armadillo headers.
	  find_package(Armadillo)
	  if (ARMADILLO_FOUND)
		message(STATUS "Armadillo headers found: ${ARMADILLO_INCLUDE_DIRS}")
	  else ()
		message( WARNING "To use the option USE_ML, Armadillo needs to be installed. Proceeding with USE_ML=OFF.")
		set (USE_ML OFF)
	  endif()
  endif()
endif()

option(SAMP "Use cmsgen for sampling " OFF)
if (NOT USE_ML)
  set (SAMP OFF)
endif ()

find_package (Boost 1.46.1 COMPONENTS graph REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include_directories(cadical_ipasir)
include_directories(glucose)
include_directories(glucose/glucose)

include_directories(aiger-1.9.9)
include_directories(docopt)

option(BUILD_PEDANT_EXECUTABLE "Build Pedant executable (switch off to only build library)." ON)

add_subdirectory(src)

option(BUILT_CERT_TOOLS "Build tools required for checking AIGER certificates" ON)
if (BUILT_CERT_TOOLS)
	add_subdirectory(certification/AIG2CNF)
	add_subdirectory(certification/AIG_Dependency_Checker)
endif()
