#ifndef PEDANT_CONSISTENCYCHECKER_H_
#define PEDANT_CONSISTENCYCHECKER_H_

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <memory>
#include <set>

#include "solvertypes.h"
#include "satsolver.h"
#include "logging.h"
#include "disjunction.h"
#include "utils.h"
#include "configuration.h"
#include "supporttracker.h"
#include "dependencycontainer.h"
#include "solverdata.h"

namespace pedant {

struct VariableData {
  VariableData(int& last_used_variable, std::vector<int>& disjunction_terminals): 
      literal_disjunctions{Disjunction(last_used_variable, disjunction_terminals), Disjunction(last_used_variable, disjunction_terminals)},
      default_disjunctions{Disjunction(last_used_variable, disjunction_terminals), Disjunction(last_used_variable, disjunction_terminals)} {};
  int literal_variables[2];
  int no_forcing_clause_active;
  int default_value_index;
  int use_default_value_index;
  Disjunction literal_disjunctions[2];
  Disjunction default_disjunctions[2];
};

class ConsistencyChecker {
 public:
  ConsistencyChecker(const DependencyContainer& dependencies, const std::vector<int>& existential_variables, 
      const std::vector<int>& universal_variables, int& last_used_variable, SolverData& shared_data, const Configuration& config);
  bool checkConsistency(const std::vector<int>& arbiter_assumptions, std::vector<int>& existential_counterexample, std::vector<int>& universal_counterexample, 
      std::vector<int>& arbiter_counterexample, std::vector<int>& complete_universal_counterexample);
  void addArbiterVariable(int existential_variable, int arbiter_variable);
  void addForcingClause(const Clause& clause);
  void addDefinition(int variable, const std::vector<Clause>& definition, const std::vector<int>& conflict);
  std::tuple<int, int> addDefinitionSelector(int variable, const std::vector<Clause>& definition, int selector);
  void setDefaultValue(int existential_variable, bool value);
  void setDefaultValueActive(int existential_variable, bool active);
  void addDefaultClause(const Clause& premise,int label);
  void addAssumption(const std::vector<int>& assms);

 private:
  void initVariableData(int variable);
  void initEncoding();
  Clause translateClause(const Clause& clause);
  int getInconsistentExistentialVariable();
  void disableDefaultFunctionSelector(int existential_variable);
  int newDefaultFunctionSelector(int existential_variable);
  std::vector<int> getAssignment(std::vector<int>& existential_variables);
  std::vector<int> getTranslatedDefinitionSupport(int variable, const Clause& conflict, const std::vector<Clause>& definition);


  std::unordered_set<int> existential_variables_set;
  std::unordered_set<int> undefined_existential_variables;
  const std::vector<int>& universal_variables;
  std::unordered_map<int, VariableData> variable_data;
  std::vector<int> default_function_selectors;
  std::unordered_map<int, int> variable_to_default_function_selector_index;
  std::vector<int> disjunction_terminals;
  std::vector<int> conflict_variables;
  std::unordered_map<int, int> conflict_variable_to_variable;
  std::vector<int> default_assumptions;
  const DependencyContainer& dependencies;
  // const std::unordered_map<int, std::set<int>>& extended_dependency_map;
  std::unordered_map<int, int> literal_variable_to_variable;
  std::unordered_map<int, int> literal_variable_to_literal;
  SolverData& shared_data;
  // std::unordered_map<int, int> arbiter_to_existential_variable;
  std::unordered_map<int,int> default_fires;


  // CadicalSolver consistency_solver;
  std::shared_ptr<SatSolver> consistency_solver;
  int& last_used_variable;
  const Configuration& config;
  SupportTracker supporttracker;

};

// Implementation of inline methods.

inline void ConsistencyChecker::addArbiterVariable(int existential_variable, int arbiter_variable) {
  // arbiter_to_existential_variable[arbiter_variable] = existential_variable;
  supporttracker.addArbiterVariable(arbiter_variable);
}

inline void ConsistencyChecker::disableDefaultFunctionSelector(int existential_variable) {
   if (variable_to_default_function_selector_index[existential_variable] != -1) {
     int& selector = default_function_selectors[variable_to_default_function_selector_index[existential_variable]];
     selector = -var(selector);
   }
}

inline void ConsistencyChecker::setDefaultValue(int existential_variable, bool value) {
  DLOG(trace) << "Setting default of " << existential_variable << " to " << value << std::endl;
  setLiteralSign(default_assumptions[variable_data.at(existential_variable).default_value_index], value);
}

inline void ConsistencyChecker::setDefaultValueActive(int existential_variable, bool active) {
  DLOG(trace) << "Setting default activity of " << existential_variable << " to " << active << std::endl;
  setLiteralSign(default_assumptions[variable_data.at(existential_variable).use_default_value_index], active);
  if (variable_to_default_function_selector_index[existential_variable] != -1) {
    setLiteralSign(default_function_selectors[variable_to_default_function_selector_index[existential_variable]], !active);
  }
}
inline std::vector<int> ConsistencyChecker::getAssignment(std::vector<int>& existential_variables) {
  std::vector<int> assignment;
  for (auto v: existential_variables) {
    auto& vd = variable_data.at(v);
    bool negated = (consistency_solver->val(vd.literal_variables[false]) > 0);
    assignment.push_back((v ^ -negated) + negated);
  }
  return assignment;
}

}

#endif // PEDANT_CONSISTENCYCHECKER_H_