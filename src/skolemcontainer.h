#ifndef PEDANT_SKOLEMCONTAINER_H_
#define PEDANT_SKOLEMCONTAINER_H_

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <random>
#include <string>

#include "solvertypes.h"
#include "simplevaliditychecker.h"
#include "utils.h"
#include "cadical.h"
#include "logging.h"
#include "modellogger.h"

#ifdef USE_MACHINE_LEARNING
#include "learning.h"
#endif

namespace pedant {

class SimpleValidityChecker; // Forward declaration.

class SkolemContainer {

 public:
  SkolemContainer(std::vector<int>& universal_variables, std::unordered_map<int, std::vector<int>>& dependency_map, int& last_used_variable);
  void initValidityCheckModel(SimpleValidityChecker& validity_checker);
  void addForcingClause(Clause& forcing_clause, bool reduced, SimpleValidityChecker& validity_checker);
  void addDefinition(int variable, std::vector<Clause>& definition,const std::vector<std::tuple<std::vector<int>,int>>& circuit_def, std::vector<int>& conflict, SimpleValidityChecker& validity_checker);
  std::vector<int> validityCheckAssumptions();
  std::vector<int> validityCheckSoftAssumptions();
  bool checkConsistency(std::vector<int>& assumptions, std::vector<int>& output_universal_counterexample, std::vector<int>& output_existential_counterexample);
  void setDefaultFunction(int variable, std::vector<Clause>& default_function_clauses, SimpleValidityChecker& validity_checker);
  void setRandomDefaultValue(int variable);
  void writeModelAsCNFToFile(const std::vector<int>& arbiter_assignment,const std::string& file_name);
  void writeModelAsAIGToFile(const std::vector<int>& arbiter_assignment,const std::string& file_name, bool binary_AIGER=true);

 private:
  void addConsistencyDisjunct(int literal, int literal_true_variable);
  void addValidityDisjunct(int variable, int variable_disjunct_active, SimpleValidityChecker& validity_checker);
  void addToConsistencyCheck(int variable, std::vector<Clause>& definition, std::vector<int>& conflict);
  void useDefaultValue(int variable, bool on);
  int getDefaultValueVariable(int variable);
  void setDefaultValue(int variable, bool value);
  std::vector<int> getInconsistentExistentialAssignment();
  Clause translateClauseConsistency(Clause& forcing_clause);

  int& last_used_variable;
  std::vector<int> existential_variables;
  std::vector<int> universal_variables;
  std::unordered_set<int> undefined_existentials;
  std::unordered_map<int, std::unordered_set<int>> existential_dependencies_map_set;
  std::unordered_map<int, std::vector<int>> existential_dependencies_map;
  std::unordered_map<int, int> literal_to_consistency_solver_variable;
  std::unordered_map<int, int> consistency_solver_variable_to_literal;
  std::unordered_map<int, int> literal_to_consistency_disjunction_terminal_index;
  std::unordered_map<int, int> variable_to_validity_disjunction_terminal_index;
  std::unordered_map<int, int> variable_to_default_active_variable;
  std::unordered_map<int, int> variable_to_default_active_variable_consistency;
  std::unordered_map<int, int> variable_to_default_value_index;
  std::unordered_map<int, int> variable_to_default_value_index_consistency;
  std::unordered_map<int, int> variable_to_default_value_consistency;
  std::unordered_map<int, int> variable_to_use_default_index;
  std::unordered_map<int, int> variable_to_default_function_selector_index;
  std::unordered_map<int, int> variable_to_conflict_variable;
  std::unordered_map<int, int> conflict_variable_to_variable;
  std::vector<int> conflict_variables;
  std::vector<int> consistency_disjunction_terminals;
  std::vector<int> validity_disjunction_terminals;
  std::vector<int> assumptions_for_defaults;
  std::vector<int> assumptions_for_defaults_consistency;
  CadicalSolver consistency_solver;
  std::default_random_engine re;
  std::uniform_int_distribution<> bernoulli;
  ModelLogger current_model;

  //Components related to learning
#ifdef USE_MACHINE_LEARNING
 public:
  void setLearnedDefaultFunction(int variable, SimpleValidityChecker& validity_checker);
  void addTrainingExample(int variable, std::vector<int>& universal_assignment, int label_literal);
  Learner& getLearner(int variable);
 private:
  std::unordered_map<int, std::unique_ptr<pedant::Learner>> variables_to_learners;
#endif
  
};

// Implementation of inline methods



inline std::vector<int> SkolemContainer::validityCheckAssumptions() {
  return validity_disjunction_terminals;
}

inline std::vector<int> SkolemContainer::validityCheckSoftAssumptions() {
  return assumptions_for_defaults;
}


inline void SkolemContainer::useDefaultValue(int variable, bool on) {
  current_model.useDefaultValue(variable,on);
  auto& use_default_literal = assumptions_for_defaults[variable_to_use_default_index[variable]];
  setLiteralSign(use_default_literal, on);
  // Use default function if default value is deactivated and vice versa.
  auto& default_function_selector = assumptions_for_defaults[variable_to_default_function_selector_index[variable]];
  setLiteralSign(default_function_selector, !on);
}

inline int SkolemContainer::getDefaultValueVariable(int variable) {
  return var(assumptions_for_defaults[variable_to_default_value_index[variable]]);
}

inline void SkolemContainer::setDefaultValue(int variable, bool value) {
  current_model.setDefaultValue(variable,value);
  auto& default_value_literal = assumptions_for_defaults[variable_to_default_value_index[variable]];
  setLiteralSign(default_value_literal, value);
  auto& default_value_literal_consistency = assumptions_for_defaults_consistency[variable_to_default_value_index_consistency[variable]];
  setLiteralSign(default_value_literal_consistency, value);
}

inline void SkolemContainer::setRandomDefaultValue(int variable) {
  bool value = bernoulli(re);
  setDefaultValue(variable, value);
  // Activate default value.
  useDefaultValue(variable, true);
}


inline void SkolemContainer::writeModelAsCNFToFile(const std::vector<int>& arbiter_assignment,const std::string& file_name) {
  current_model.writeModelAsCNFToFile(file_name,arbiter_assignment);
}

inline void SkolemContainer::writeModelAsAIGToFile(const std::vector<int>& arbiter_assignment,const std::string& file_name, bool binary_AIGER) {
  current_model.writeModelAsAIGToFile(file_name,arbiter_assignment,binary_AIGER);
}

//Functions related to Learning
#ifdef USE_MACHINE_LEARNING

inline Learner& SkolemContainer::getLearner(int variable) {
  return *variables_to_learners.at(variable);
}

inline void SkolemContainer::setLearnedDefaultFunction(int variable, SimpleValidityChecker& validity_checker) {
  std::vector<Clause> default_function_clauses = variables_to_learners[variable]->giveCNFRepresentation(true);
  DLOG(trace) << "Resetting default for " << variable << " with " << default_function_clauses.size() << " clauses." << std::endl;
  setDefaultFunction(variable, default_function_clauses, validity_checker);
}

#endif


}


#endif // PEDANT_SKOLEMCONTAINER_H_