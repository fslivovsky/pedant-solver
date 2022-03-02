#ifndef PEDANT_SOLVERCONFIG_H_
#define PEDANT_SOLVERCONFIG_H_

#include <memory>
#include <exception>

#include "satsolver.h"
#include "cadical.h"
#include "glucose-ipasir.h"
//#include "mergesat.h"


#include "../src/configuration.h"


namespace pedant {

class SolverNotSupportedException : public std::exception {
  const char * what () const throw () {
      return "Solver not supported!";
   }
};

std::shared_ptr<SatSolver> giveSolverInstance(SatSolverType solver_type);

inline std::shared_ptr<SatSolver> giveSolverInstance(SatSolverType solver_type) {
  switch (solver_type)
  {
  case Cadical:
    return std::make_shared<CadicalSolver>();
  case Glucose:
    return std::make_shared<GlucoseSolver>();
  //case MergeSat:
   //return std::make_unique<MergesatSolver>();
  default:
    throw SolverNotSupportedException();
  }
};

}
#endif
