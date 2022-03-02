#ifndef PEDANT_SATSOLVER_H_
#define PEDANT_SATSOLVER_H_

#include <vector>

#include "solvertypes.h"


namespace pedant {

class SatSolver {

 public:
  // virtual ~SatSolver();
  virtual void appendFormula(const std::vector<Clause>& formula) = 0;
  virtual void addClause(const Clause& clause) = 0;
  virtual void assume(const std::vector<int>& assm) = 0;
  virtual int solve(const std::vector<int>& assumptions) = 0;
  virtual int solve() = 0;
  virtual int solve(int conflict_limit) = 0;
  virtual std::vector<int> getFailed(const std::vector<int>& assumptions) = 0;
  virtual std::vector<int> getValues(const std::vector<int>& variables) = 0;
  virtual std::vector<int> getModel() = 0;
  virtual int val(int variable) = 0;
};
  
} 





#endif