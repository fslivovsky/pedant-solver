#ifndef PEDANT_DEFINABILITYCHECKER_H_
#define PEDANT_DEFINABILITYCHECKER_H_

#include <vector>
#include <memory>
#include <unordered_map>

#include "solvertypes.h"
#include "cadical.h"
#include "ITPsolver.h"

namespace pedant {

class DefinabilityChecker {

 public:
  DefinabilityChecker(std::vector<int>& universal_variables, std::unordered_map<int, std::vector<int>>& dependency_map, std::vector<Clause>& matrix, int& last_used_variable, bool compress=false);
  std::tuple<bool, std::vector<Clause>, std::vector<std::tuple<std::vector<int>,int>>, std::vector<int>> checkDefinability(std::vector<int>& defining_variables,
                                                                            int defined_variable,
                                                                            std::vector<int>& assumptions);
  void addSharedVariable(int variable);
  void addClause(Clause& clause);
  int getMaxVariable() const;

 private:
  std::tuple<bool, std::vector<Clause>, std::vector<int>> checkForced(int variable, std::vector<int>& assumptions);
  std::vector<int> getConflict(std::vector<int>& assumptions);
  std::tuple<std::vector<Clause>,std::vector<std::tuple<std::vector<int>,int>>> getDefinition(std::vector<int>& defining_variables, int defined_variable);

  int& last_used_variable;
  bool compress;
  std::unordered_map<int, int> renaming, renaming_inverse;
  std::unordered_map<int, int> variable_to_equality_selector;
  std::unordered_map<int, int> variable_to_on_selector;
  std::unordered_map<int, int> variable_to_off_selector;
  CadicalSolver backbone_solver, fast_solver;
  std::unique_ptr<ITPSolver> interpolating_solver;

};

}

#endif // PEDANT_DEFINABILITYCHECKER_H_