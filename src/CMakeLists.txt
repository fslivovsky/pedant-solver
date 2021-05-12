
if (USE_ML)
	add_compile_definitions(USE_MACHINE_LEARNING)
endif ()

add_library(simplevaliditychecker simplevaliditychecker.h simplevaliditychecker.cc)
target_link_libraries(simplevaliditychecker PRIVATE cadical_library)

add_library(unatechecker unatechecker.h unatechecker.cc)
target_link_libraries(unatechecker PRIVATE cadical_library)

if (USE_ML)
	add_library(learner learning.h learning.cc)
	target_include_directories(learner PRIVATE ${MLPACK_INCLUDE_DIRS})
	target_link_libraries(learner PRIVATE ${MLPACK_LIBRARIES})
endif ()

add_library(preprocessor preprocessor.h preprocessor.cc)

add_library(modellogger modellogger.h modellogger.cc buildAIGER.h buildAIGER.cc)
target_link_libraries(modellogger PRIVATE aiger_library)

add_library(skolemcontainer skolemcontainer.h skolemcontainer.cc)
target_link_libraries(skolemcontainer PRIVATE cadical_library modellogger)

add_library(ITPsolver ITPsolver.h ITPsolver.cc)
target_link_libraries(ITPsolver PRIVATE ${INTERPOLATING_SOLVER_LIBRARY})

add_library(definabilitychecker definabilitychecker.h definabilitychecker.cc)
target_link_libraries(definabilitychecker PRIVATE ITPsolver cadical_library)

add_library(parser dqdimacs_parser.h dqdimacs_parser.cc)

add_library(solver solver.h solver.cc)
if (USE_ML)
	target_link_libraries(solver PRIVATE definabilitychecker simplevaliditychecker skolemcontainer learner unatechecker interrupt)
else ()
	target_link_libraries(solver PRIVATE definabilitychecker simplevaliditychecker skolemcontainer unatechecker interrupt)
endif()

add_executable(pedant pedant.cc)
target_link_libraries(pedant PRIVATE solver parser preprocessor interrupt)

add_executable(dependencychecker AIGERDependencyChecker.h AIGERDependencyChecker.cc)
target_link_libraries(dependencychecker PRIVATE aiger_library)

add_executable(aig2cnf AigerCnfConverter.h AigerCnfConverter.cc)
target_link_libraries(aig2cnf PRIVATE aiger_library)
