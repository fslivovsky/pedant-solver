#ifndef PEDANT_SKOLEMCONTAINER_H_
#define PEDANT_SKOLEMCONTAINER_H_

#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>
#include <memory>
#include <random>
#include <string>
#include <tuple>

#include "solvertypes.h"
#include "simplevaliditychecker.h"
#include "utils.h"
#include "cadical.h"
#include "glucose-ipasir.h"
#include "logging.h"
#include "modellogger.h"
#include "disjunction.h"
#include "consistencychecker.h"
#include "configuration.h"
#include "defaultvaluecontainer.h"
#include "dependencycontainer.h"
#include "solverdata.h"

namespace pedant {

class SimpleValidityChecker; // Forward declaration.

// https://stackoverflow.com/questions/20511347/a-good-hash-function-for-a-vector
class IntVectorHasher {
 public:
  std::size_t operator()(std::vector<int> const& vec) const {
    std::size_t ret = 0;
    for(auto& i : vec) {
      ret ^= std::hash<int>()(i);
    }
    return ret;
  }
};

class SkolemContainer {

 public:
  SkolemContainer(const std::vector<int>& universal_variables, const std::vector<int>& existential_variables, DependencyContainer& dependencies,
                  int& last_used_variable, SimpleValidityChecker& validity_checker, SolverData& shared_data, const Configuration& config);
  void initValidityCheckModel();
  void initDefaultValues();
  void addForcingClause(Clause& forcing_clause, bool reduced);
  void addDefinition(int variable, std::vector<Clause>& definition, const std::vector<std::tuple<std::vector<int>,int>>& circuit_def, std::vector<int>& conflict, bool reduced = false);
  const std::vector<int>& validityCheckAssumptions();
  bool checkConsistency(const std::vector<int>& arbiter_assumptions, std::vector<int>& existential_counterexample, std::vector<int>& universal_counterexample, 
      std::vector<int>& arbiter_counterexample, std::vector<int>& complete_universal_counterexample);
  void writeModelAsCNFToFile(const std::vector<int>& arbiter_assignment,const std::string& file_name);
  void writeModelAsAIGToFile(const std::vector<int>& arbiter_assignment,const std::string& file_name, bool binary_AIGER=true);
  std::tuple<int, bool, bool> getArbiter(int existential_variable, const std::vector<int>& assignment_dependencies, bool introduce_clauses);
  std::vector<int> getArbiterAnnotation(int arbiter);
  void setDefaultValue(int existential_variable, bool value);
  void setRandomDefaultValue(int existential_variable);
  void setDefaultValueActive(int existential_variable, bool active);

  void insertIntoDefaultContainer(int existential_literal, const std::vector<int>& universal_assignment);
  void insertIntoDefaultContainer(int existential_literal, const std::vector<int>& universal_assignment, const std::vector<int>& existential_assignment);
  //If there is no tree this method can be used to set the default value
  void setPolarity(int variable, bool polarity);

  void insertSamplesIntoDefaultContainer(const std::vector<Clause>& matrix, const std::set<int>& variables_to_sample);

  //TODO: Debug / Logging
  auto getDefaultStatistic() const;

 private:
  void addDefinitionWithSelector(int variable, std::vector<Clause>& definition, int selector, bool reduced);
  void addForcingDisjunct(int variable, int variable_disjunct_active);
  int createArbiter(int existential_variable, const std::vector<int>& annotation);
  bool existsArbiter(int existential_variable, const std::vector<int>& assignment_dependencies);
  void disableDefaultFunctionSelector(int existential_variable);
  int newDefaultFunctionSelector(int existential_variable);
  void addArbiterClause(Clause& arbiter_clause);
  void realizeArbiter(int arbiter);
  void addDefaultClauses(int variable, std::vector<Clause>& clauses);
  void addDefaultClause(int variable, Clause& clause, int use_default_variable);
  void intitDefaultContainer();

  SolverData& shared_data;
  // std::unordered_map<int, int> arbiter_to_existential;
  std::unordered_map<int, Disjunction> arbiter_disjunction;
  std::unordered_map<int, int> arbiter_to_arbiter_active_variable;
  std::unordered_map<int, std::vector<int>> arbiter_to_annotation;
  std::unordered_map<int, std::unordered_map<std::vector<int>, int, IntVectorHasher>> existential_arbiter_map;
  std::unordered_set<int> proper_arbiters;

  int& last_used_variable;
  const std::vector<int>& existential_variables;
  const std::vector<int>& universal_variables;
  std::unordered_set<int> undefined_existentials;
  std::unordered_map<int, Disjunction> variable_to_forcing_disjunction;
  std::unordered_map<int, int> variable_to_no_forcing_clause_active_variable;
  std::unordered_map<int, int> variable_to_use_default_index;
  std::unordered_map<int, int> variable_to_default_function_selector_index;
  std::vector<int> validity_check_assumptions;
  std::default_random_engine re;
  std::uniform_int_distribution<> bernoulli;
  ConsistencyChecker consistencychecker;
  SimpleValidityChecker& validity_checker;
  const Configuration& config;
  DefaultValueContainer default_values;
  ModelLogger current_model;
  std::unordered_map<int,int> apply_default;
  DependencyContainer& dependencies;
  // std::unordered_map<int, std::set<int>>& extended_dependency_map;
  
};

// Implementation of inline methods

inline const std::vector<int>& SkolemContainer::validityCheckAssumptions() {
  return validity_check_assumptions;
}
  
inline bool SkolemContainer::checkConsistency(const std::vector<int>& arbiter_assumptions, std::vector<int>& existential_counterexample, 
    std::vector<int>& universal_counterexample, std::vector<int>& arbiter_counterexample, std::vector<int>& complete_universal_counterexample) {
  consistencychecker.addAssumption(default_values.getSelectors());
  return consistencychecker.checkConsistency(arbiter_assumptions, existential_counterexample, universal_counterexample, arbiter_counterexample, complete_universal_counterexample);
}

inline void SkolemContainer::writeModelAsCNFToFile(const std::vector<int>& arbiter_assignment,const std::string& file_name) {
  current_model.writeModelAsCNFToFile(file_name,arbiter_assignment);
}

inline void SkolemContainer::writeModelAsAIGToFile(const std::vector<int>& arbiter_assignment,const std::string& file_name, bool binary_AIGER) {
  current_model.writeModelAsAIGToFile(file_name,arbiter_assignment,binary_AIGER);
}

inline bool SkolemContainer::existsArbiter(int existential_variable, const std::vector<int>& assignment_dependencies) {
  return existential_arbiter_map[existential_variable].find(assignment_dependencies) != existential_arbiter_map[existential_variable].end();
}

inline std::vector<int> SkolemContainer::getArbiterAnnotation(int arbiter) {
  return arbiter_to_annotation[arbiter];
}

inline void SkolemContainer::disableDefaultFunctionSelector(int existential_variable) {
  if (variable_to_default_function_selector_index[existential_variable] != -1) {
    int& selector = validity_check_assumptions[variable_to_default_function_selector_index[existential_variable]];
    selector = -var(selector);
  }
}

inline void SkolemContainer::setDefaultValue(int existential_variable, bool value) {
  default_values.setFixedDefaultPolarity(existential_variable,value);
}

inline void SkolemContainer::setRandomDefaultValue(int existential_variable) {
  bool value = bernoulli(re);
  setDefaultValue(existential_variable, value);
}

inline void SkolemContainer::setDefaultValueActive(int existential_variable, bool active) {
  setLiteralSign(validity_check_assumptions[variable_to_use_default_index[existential_variable]], active);
  if (variable_to_default_function_selector_index[existential_variable] != -1) {
    setLiteralSign(validity_check_assumptions[variable_to_default_function_selector_index[existential_variable]], !active);
  }
  consistencychecker.setDefaultValueActive(existential_variable, active);
}

inline void SkolemContainer::setPolarity(int variable, bool polarity) {
  default_values.setFixedDefaultPolarity(variable,polarity);
}

inline void SkolemContainer::addDefaultClauses(int variable, std::vector<Clause>& clauses) {
  int use_default = apply_default.at(variable);
  for (Clause& cl:clauses) {
    addDefaultClause(variable,cl,use_default);
  }
}

inline void SkolemContainer::intitDefaultContainer() {
  std::vector<std::pair<int,std::vector<Clause>>> x = default_values.initialize();
  for (auto& [var,clauses] : x) {
    int use_default = apply_default.at(var);
    for (auto& cl : clauses) {
      addDefaultClause(var,cl,use_default);
    }
  }
}

inline auto SkolemContainer::getDefaultStatistic() const {
  return default_values.getStatistic();
}


}


#endif // PEDANT_SKOLEMCONTAINER_H_