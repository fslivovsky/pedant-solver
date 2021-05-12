project(cadical_bindings)

FILE(GLOB CadicalSources cadical/src/*.cpp cadical/src/*.hpp)

add_library (cadical_library cadical.h cadical.cc ${CadicalSources})
target_compile_options(cadical_library PRIVATE -fPIC)
target_link_libraries(cadical_library interrupt)
