cmake_minimum_required(VERSION 3.10)
project(utils CXX)

pybind11_add_module(utils_cc SHARED utils_bindings.cc)

set_target_properties(utils_cc PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET utils_cc PROPERTY CXX_STANDARD 17)
