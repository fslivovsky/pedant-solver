# Find Avy

set(AVY_ROOT "" CACHE PATH "Root of Avy installation.")

if (DEFINED ClauseItpSeq_SOURCE_DIR )
  include("${ClauseItpSeq_SOURCE_DIR}/cmake/PackageOptions.cmake")
else()
  find_path(AVY_SOURCE_DIR NAMES AigUtils.h PATHS ${AVY_ROOT}/src/)
  find_path(AVY_INCLUDE_DIR NAMES avy/AvyAbc.h PATHS ${AVY_ROOT}/include/)
  find_library(ABC_CPP_LIBRARY NAMES AbcCpp PATHS ${AVY_ROOT}/lib/)
  find_library(AVY_DEBUG_LIBRARY NAMES AvyDebug PATHS ${AVY_ROOT}/lib/)
  find_library(CLAUSEITPSEQ_LIBRARY NAMES ClauseItpSeq PATHS ${AVY_ROOT}/lib/)
endif()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(Avy
  REQUIRED_VARS AVY_SOURCE_DIR AVY_INCLUDE_DIR ABC_CPP_LIBRARY AVY_DEBUG_LIBRARY CLAUSEITPSEQ_LIBRARY)

mark_as_advanced(AVY_SOURCE_DIR AVY_INCLUDE_DIR ABC_CPP_LIBRARY AVY_DEBUG_LIBRARY CLAUSEITPSEQ_LIBRARY)
