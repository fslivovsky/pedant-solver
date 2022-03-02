#ifndef PEDANT_GLUCOSE_IPASIR_H_
#define PEDANT_GLUCOSE_IPASIR_H_


#include <vector>
#include <algorithm>

#include "glucose_core/Solver.h"
#include "glucose_core/SolverTypes.h"

#include "solvertypes.h"
#include "satsolver.h"


namespace pedant {

class GlucoseSolver : public SatSolver {

 public:
  GlucoseSolver();
  void appendFormula(const std::vector<Clause>& formula);
  void addClause(const Clause& clause);
  void assume(const std::vector<int>& assm);
  int solve(const std::vector<int>& assumptions);
  int solve();
  int solve(int conflict_limit);
  std::vector<int> getFailed(const std::vector<int>& assumptions);
  std::vector<int> getValues(const std::vector<int>& variables);
  std::vector<int> getModel();
  int val(int var);

 private:
  Glucose::Solver solver;
  std::vector<int> assumptions;
  int max_var=-1;

  Glucose::Lit MakeGlucoseLiteral(int literal);
  Glucose::Lit MakeGlucoseVariable(int variable);

};


inline GlucoseSolver::GlucoseSolver() {
  //Compiling glucose with the option INCREMENTAL causes errors when the original source code is used -> In the Glucose source code there is a typo.
  solver.setIncrementalMode();
}


inline Glucose::Lit GlucoseSolver::MakeGlucoseLiteral(int literal) {
  return Glucose::mkLit(abs(literal),literal<0);
}

inline Glucose::Lit GlucoseSolver::MakeGlucoseVariable(int variable) {
  return Glucose::mkLit(variable,false);
}


inline void GlucoseSolver::appendFormula(const std::vector<Clause>& formula) {
  for (const Clause& clause:formula) {
    addClause(clause);
  }  
}

inline void GlucoseSolver::assume(const std::vector<int>& assms) {
  assumptions.insert(assumptions.end(),assms.begin(),assms.end());
}


}

#endif // PEDANT_GLUCOSE_H_
