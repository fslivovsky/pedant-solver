# FindUniGen

# UNIX paths are standard, no need to specify them.

find_library(UNIGEN_LIBRARY
	NAMES unigen cryptominisat5 approxmc
	PATHS "/usr/local/lib"
)

find_library(CRYPTOMINISAT_LIBRARY
	NAMES cryptominisat5
	PATHS "/usr/local/lib"
)

find_library(APPROXMC_LIBRARY
	NAMES approxmc
	PATHS "/usr/local/lib"
)


find_path(UNIGEN_INCLUDE_DIR
	NAMES unigen/unigen.h
	PATHS "/usr/local/include/unigen"
)


find_package_handle_standard_args(MLPACK
	REQUIRED_VARS UNIGEN_LIBRARY UNIGEN_INCLUDE_DIR
	CRYPTOMINISAT_LIBRARY APPROXMC_LIBRARY
)

