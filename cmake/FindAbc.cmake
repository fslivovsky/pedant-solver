# Find Abc

set(ABC_ROOT "" CACHE PATH "Root of ABC compiled source tree.")

set(ABC_NAMESPACE "abc" CACHE STRING "Abc namespace to use.")

if (DEFINED ABC_SOURCE_DIR)
  include ("${ABC_SOURCE_DIR}/cmake/PackageOptions.cmake")
  set (ABC_LIBRARY abc-static-lib)
else()
  find_program(ABC_ARCH_FLAGS NAMES arch_flags PATHS ${ABC_ROOT})
  if (ABC_ARCH_FLAGS)
    execute_process (COMMAND ${ABC_ARCH_FLAGS}
      OUTPUT_VARIABLE ABC_CXXFLAGS
      OUTPUT_STRIP_TRAILING_WHITESPACE )

    set(ABC_CXXFLAGS "${ABC_CXXFLAGS} -DABC_NAMESPACE=abc")

    message (STATUS "ABC arch flags are: ${ABC_CXXFLAGS}")
  endif()

  find_path(ABC_INCLUDE_DIR NAMES base/abc/abc.h PATHS ${ABC_ROOT}/src)
  find_library(ABC_LIBRARY NAMES abc PATHS ${ABC_ROOT})
endif()


include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(Abc
  REQUIRED_VARS ABC_LIBRARY ABC_INCLUDE_DIR)

mark_as_advanced(ABC_LIBRARY ABC_INCLUDE_DIR ABC_CXXFLAGS ABC_ARCH_FLAGS)