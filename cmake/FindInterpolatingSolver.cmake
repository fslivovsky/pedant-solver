# Find Avy

set(INTERPOLATING_SOLVER_ROOT "" CACHE PATH "Root of ITP MiniSAT Directory.")

if (DEFINED InterpolatingSolver_SOURCE_DIR )
  include("${InterpolatingSolver_SOURCE_DIR}/cmake/PackageOptions.cmake")
else()
  find_path(INTERPOLATING_SOLVER_INCLUDE_DIR NAMES InterpolatingSolver.h PATHS ${INTERPOLATING_SOLVER_ROOT}/src/)
  find_library(INTERPOLATING_SOLVER_LIBRARY NAMES interpolating_minisat PATHS ${INTERPOLATING_SOLVER}/src/)
endif()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(InterpolatingSolver INTERPOLATING_SOLVER_INCLUDE_DIR INTERPOLATING_SOLVER_LIBRARY)
mark_as_advanced(INTERPOLATING_SOLVER_INCLUDE_DIR INTERPOLATING_SOLVER_LIBRARY)
