#ifndef PEDANT_CADICAL_H_
#define PEDANT_CADICAL_H_

#include <stdlib.h> 

#include <vector>
#include <algorithm>

#include "cadical/src/cadical.hpp"
#include "solvertypes.h"
#include "interrupt.h"

namespace pedant {

class CadicalSolver {
 public:
  CadicalSolver();
  ~CadicalSolver();
  void appendFormula(std::vector<Clause>& formula);
  void addClause(Clause& clause);
  void assume(std::vector<int>& assumptions);
  int solve(std::vector<int>& assumptions);
  int solve();
  int solve(int conflict_limit);
  std::vector<int> getFailed(std::vector<int>& assumptions);
  std::vector<int> getValues(std::vector<int>& variables);
  std::vector<int> getModel();

 private:
  void assumeAll(std::vector<int>& assumptions);
  int val(int variable);

  CaDiCaL::Solver solver;

  class CadicalTerminator: public CaDiCaL::Terminator {
   public:
    virtual bool terminate();
  };

  static CadicalTerminator terminator;
};

inline void CadicalSolver::appendFormula(std::vector<Clause>& formula) {
  for (auto& clause: formula) {
    addClause(clause);
  }
}

inline void CadicalSolver::addClause(Clause& clause) {
  for (auto& l: clause) {
    solver.add(l);
  }
  solver.add(0);
}

inline void CadicalSolver::assume(std::vector<int>& assumptions) {
  for (auto& l: assumptions) {
    solver.assume(l);
  }
}

inline void CadicalSolver::assumeAll(std::vector<int>& assumptions) {
  solver.reset_assumptions();
  assume(assumptions);
}

inline int CadicalSolver::solve(std::vector<int>& assumptions) {
  assumeAll(assumptions);
  return solver.solve();
}

inline int CadicalSolver::solve() {
  int result = solver.solve();
  if (InterruptHandler::interrupted(nullptr)) {
    throw InterruptedException();
  }
  return result;
}

inline int CadicalSolver::solve(int conflict_limit) {
  solver.limit("conflicts", conflict_limit);
  return solve();
}

inline std::vector<int> CadicalSolver::getFailed(std::vector<int>& assumptions) {
  std::vector<int> failed_literals;
  for (auto& l: assumptions) {
    if (solver.failed(l)) {
      failed_literals.push_back(l);
    }
  }
  return failed_literals;
}

inline std::vector<int> CadicalSolver::getValues(std::vector<int>& variables) {
  std::vector<int> assignment;
  for (auto& v: variables) {
    assignment.push_back(val(v));
  }
  return assignment;
}

inline std::vector<int> CadicalSolver::getModel() {
  std::vector<int> assignment;
  for (int v = 1; v <= solver.vars(); v++) {
    assignment.push_back(val(v));
  }
  return assignment;
}

inline int CadicalSolver::val(int variable) {
  auto l = solver.val(variable);
  auto v = abs(l);
  if (v == variable) {
    return l;
  } else {
    return variable;
  }
}

}

#endif // PEDANT_CADICAL_H_