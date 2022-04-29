#include "solverwrapper.h"

#include <iostream>

#include <csignal> 

#include "dqdimacs.h"
#include "inputformula.h"
#include "solvertypes.h"
#include "solver.h"
#include "logging.h"
#include "configuration.h"
#include "preprocessor.h"
#include "interrupt.h"

namespace pedant {

int callSolver(pedant::DQDIMACS& dqdimacs) {
  Configuration config;

  signal(SIGABRT, InterruptHandler::interrupt);
  signal(SIGTERM, InterruptHandler::interrupt);
  signal(SIGINT, InterruptHandler::interrupt);

  config.ignore_innermost_existentials = true;

  Preprocessor preprocessor(dqdimacs, config);
  InputFormula formula = preprocessor.preprocess();
  
  auto solver = Solver(formula, config);
  int status = solver.solve();
  if (config.verbosity > 0) {
    solver.printStatistics();
  }
  if (status == 10) {
    std::cout << "SATISFIABLE" << std::endl;
  } else if (status == 20) {
    std::cout << "UNSATISFIABLE" << std::endl;
  } else {
    std::cout << "UNKNOWN" << std::endl;
  }
  return status;
}

}
