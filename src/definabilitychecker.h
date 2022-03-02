#ifndef PEDANT_DEFINABILITYCHECKER_H_
#define PEDANT_DEFINABILITYCHECKER_H_

#include <vector>
#include <memory>
#include <unordered_map>
#include <tuple>

#include "solvertypes.h"
#include <memory>
#include "satsolver.h"
#include "ITPsolver.h"
#include "configuration.h"

namespace pedant {

class DefinabilityChecker {

 public:
  DefinabilityChecker(std::vector<int>& universal_variables, const std::vector<int>& existential_variables, std::vector<Clause>& matrix, int& last_used_variable, const Configuration& config, bool compress=false);
  DefinabilityChecker(DefinabilityChecker&& checker, int& last_used_variable, const Configuration& config);	
  std::tuple<bool, std::vector<int>> checkDefinability(const std::vector<int>& defining_variables, int defined_variable, const std::vector<int>& assumptions, int limit, bool minimize_assumptions=false);
  std::tuple<std::vector<Clause>, std::vector<std::tuple<std::vector<int>,int>>> getDefinition(const std::vector<int>& defining_variables, int defined_variable, const std::vector<int>& assumptions);
  void addVariable(int variable, bool shared=true);
  void addClause(Clause& clause);
  int getMaxVariable() const;

 private:
  std::tuple<bool, std::vector<Clause>, std::vector<int>> checkForced(int variable, const std::vector<int>& assumptions);
  std::vector<int> getConflict(std::vector<int>& assumptions);
  std::tuple<std::vector<Clause>,std::vector<std::tuple<std::vector<int>,int>>> getDefinitionInterpolant(const std::vector<int>& defining_variables, int defined_variable);
  bool checkDefinedInterpolate(const std::vector<int>& defining_variables, int defined_variable, const std::vector<int>& assumptions, int limit);
  bool checkDefined(const std::vector<int>& defining_variables, int defined_variable, const std::vector<int>& assumptions, int limit);
  std::vector<int> getFailed(const std::vector<int>& assumptions);

  int& last_used_variable;
  bool compress;
  std::unordered_map<int, int> renaming, renaming_inverse;
  std::unordered_map<int, int> variable_to_equality_selector;
  std::unordered_map<int, int> variable_to_on_selector;
  std::unordered_map<int, int> variable_to_off_selector;
  std::shared_ptr<SatSolver> backbone_solver, fast_solver;
  std::shared_ptr<ITPSolver> interpolating_solver;
  const Configuration& config;

};

}

#endif // PEDANT_DEFINABILITYCHECKER_H_