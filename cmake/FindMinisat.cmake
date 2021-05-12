# Find Minisat

set(MINISAT_ROOT "" CACHE PATH "Root of Minisat installation.")

if (DEFINED MiniSAT_SOURCE_DIR)
  include("${MiniSAT_SOURCE_DIR}/cmake/PackageOptions.cmake")
else()
  find_path(MINISAT_INCLUDE_DIR NAMES core/Solver.h PATHS ${MINISAT_ROOT}/include/minisat)
  find_library(MINISAT_LIBRARY  NAMES minisat  PATHS ${MINISAT_ROOT}/lib)
endif()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(Minisat
  REQUIRED_VARS MINISAT_LIBRARY MINISAT_INCLUDE_DIR)

mark_as_advanced(MINISAT_LIBRARY MINISAT_INCLUDE_DIR)