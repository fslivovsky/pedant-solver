project(aig2cng)

add_executable(aig2cnf AigerCnfConverter.h AigerCnfConverter.cc)
target_link_libraries(aig2cnf PRIVATE aiger_library)
