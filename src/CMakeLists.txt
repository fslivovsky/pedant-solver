if (USE_ML)
  add_definitions(-DUSE_MACHINE_LEARNING)
endif ()



add_library(simplevaliditychecker simplevaliditychecker.h simplevaliditychecker.cc)
target_link_libraries(simplevaliditychecker PUBLIC skolemcontainer consistencychecker cadical_library glucose_library)

add_library(unatechecker unatechecker.h unatechecker.cc)
target_link_libraries(unatechecker PRIVATE cadical_library glucose_library)


add_library(preprocessor preprocessor.h preprocessor.cc dependencyextractor.h dependencyextractor.cc)


add_library(modellogger modellogger.h modellogger.cc buildAIGER.h buildAIGER.cc)

add_library(dependencycontainer dependencycontainer.h dependencycontainer.cc)

add_library(consistencychecker consistencychecker.h consistencychecker.cc)
target_link_libraries(consistencychecker PUBLIC cadical_library supporttracker dependencycontainer)

add_library(defaultcontainer defaultvaluecontainer.h defaultvaluecontainer.cc)
target_link_libraries(defaultcontainer PUBLIC dependencycontainer)
if (USE_ML)
	add_library(hoeffdingdefaulttrees hoeffdingDefaultTree.h hoeffdingDefaultTree.cc)
	target_include_directories(hoeffdingdefaulttrees PUBLIC ${MLPACK_INCLUDE_DIRS} ${ARMADILLO_INCLUDE_DIRS})
	target_link_libraries(hoeffdingdefaulttrees PRIVATE ${MLPACK_LIBRARIES} dependencycontainer)
	target_link_libraries(defaultcontainer PUBLIC hoeffdingdefaulttrees)
endif ()
target_link_libraries(modellogger PRIVATE aiger_library defaultcontainer)

add_library(skolemcontainer skolemcontainer.h skolemcontainer.cc)
target_link_libraries(skolemcontainer PUBLIC cadical_library glucose_library modellogger consistencychecker defaultcontainer dependencycontainer)

add_library(ITPsolver ITPsolver.h ITPsolver.cc)
target_link_libraries(ITPsolver PRIVATE ${INTERPOLATING_SOLVER_LIBRARY})

add_library(definabilitychecker definabilitychecker.h definabilitychecker.cc)
target_link_libraries(definabilitychecker PRIVATE ITPsolver cadical_library glucose_library)

add_library(supporttracker supporttracker.h supporttracker.cc)
target_link_libraries(supporttracker PRIVATE cadical_library glucose_library graphSeparator dependencycontainer)

add_library(graphSeparator graphSeparator.h graphSeparator.cc)
target_include_directories(graphSeparator PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(graphSeparator PRIVATE ${Boost_LIBRARIES})

add_library(dqdimacs dqdimacs.h dqdimacs.cc)
add_library(parser dqdimacsparser.h dqdimacsparser.cc)

add_library(solver solver.h solver.cc)
target_link_libraries(solver PUBLIC definabilitychecker simplevaliditychecker skolemcontainer unatechecker interrupt)

if (BUILD_PEDANT_EXECUTABLE)
	add_executable(pedant pedant.cc)
	target_link_libraries(pedant PUBLIC solver parser preprocessor interrupt docopt_s dqdimacs)
endif()

add_library(solverwrapper solverwrapper.h solverwrapper.cc)
target_link_libraries(solverwrapper PUBLIC solver preprocessor interrupt docopt_s dqdimacs)