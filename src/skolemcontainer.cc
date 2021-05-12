#include "skolemcontainer.h"

#include <iostream>
#include <algorithm>
#include <map>

#include "utils.h"

namespace pedant {

SkolemContainer::SkolemContainer(std::vector<int>& universal_variables, 
      std::unordered_map<int, std::vector<int>>& dependency_map, int& last_used_variable) : 
      last_used_variable(last_used_variable), universal_variables(universal_variables), bernoulli(0, 1), 
      current_model(existential_variables,universal_variables, dependency_map.size()) {
  Clause forcing_clauses_are_inconsistent;
  for (auto& [v, dependencies]: std::map<int, std::vector<int>>(dependency_map.begin(), dependency_map.end())) {
    existential_variables.push_back(v);
    undefined_existentials.insert(v);
    // Initialize literal map for consistency checks.
    auto positive_variable = ++last_used_variable;
    auto negative_variable = ++last_used_variable;
    literal_to_consistency_solver_variable[v] = positive_variable;
    literal_to_consistency_solver_variable[-v] = negative_variable;
    consistency_solver_variable_to_literal[positive_variable] = v;
    consistency_solver_variable_to_literal[negative_variable] = -v;
    DLOG(trace) << "Variables representing " << v << " and " << -v << ": " << literal_to_consistency_solver_variable[v] << " " << literal_to_consistency_solver_variable[-v] << std::endl;
    // conflict_variable -> forced_literal_to_solver_literal[v] & forced_literal_to_solver_literal[-v] (forced false if one not true).
    auto conflict_variable = ++last_used_variable;
    variable_to_conflict_variable[v] = conflict_variable;
    conflict_variables.push_back(conflict_variable);
    conflict_variable_to_variable[conflict_variable] = v;
    std::vector<Clause> definition_conflict_variable { { literal_to_consistency_solver_variable[v],  -conflict_variable }, 
                                                       { literal_to_consistency_solver_variable[-v], -conflict_variable },
                                                       { -literal_to_consistency_solver_variable[v], -literal_to_consistency_solver_variable[-v], conflict_variable} };
    consistency_solver.appendFormula(definition_conflict_variable);
    forcing_clauses_are_inconsistent.push_back(conflict_variable);
    // Add learner.
  #ifdef USE_MACHINE_LEARNING
    variables_to_learners[v] = std::make_unique<Learner>(v, dependencies);
  #endif
    // Add variables for default values.
    variable_to_default_active_variable_consistency[v] = ++last_used_variable;
    variable_to_default_value_consistency[v] = ++last_used_variable;
    variable_to_default_value_index_consistency[v] = assumptions_for_defaults_consistency.size();
    assumptions_for_defaults_consistency.push_back(-variable_to_default_value_consistency[v]);
  }
  consistency_solver.addClause(forcing_clauses_are_inconsistent);
  // For each (forced) literal, initialize disjunction over clauses forcing this literal.
  for (auto& [forced_literal, solver_literal]: std::map<int, int>(literal_to_consistency_solver_variable.begin(), literal_to_consistency_solver_variable.end())) {
    auto consistency_disjunction_terminal = ++last_used_variable;
    literal_to_consistency_disjunction_terminal_index[forced_literal] = consistency_disjunction_terminals.size();
    consistency_disjunction_terminals.push_back(-consistency_disjunction_terminal);
    // We allow this literal to be set to true by default only if the default is active and 
    // the default value has the correct sign.
    auto use_default_value_variable = ++last_used_variable;
    auto v = var(forced_literal);
    auto default_active_variable = variable_to_default_active_variable_consistency[v];
    auto default_value_variable = variable_to_default_value_consistency[v];
    Clause default_must_be_active = { default_active_variable, -use_default_value_variable };
    consistency_solver.addClause(default_must_be_active);
    auto default_value_literal = (forced_literal > 0) ? default_value_variable : -default_value_variable;
    Clause default_value_must_match = { default_value_literal, -use_default_value_variable };
    consistency_solver.addClause(default_value_must_match);
    Clause initial_disjunct{ consistency_disjunction_terminal, use_default_value_variable, -solver_literal };
    consistency_solver.addClause(initial_disjunct);
    DLOG(trace) << "Initial disjunction for " << forced_literal << ": " << initial_disjunct << std::endl;
  }
  DLOG(trace) << "Clause asserting inconsistency: " << forcing_clauses_are_inconsistent << std::endl;
  // Compute Existential (Extended) Dependencies
  for (auto& [variable, extended_dependencies]: ComputeExtendedDependencies(dependency_map)) {
    std::vector<int> existential_dependencies;
    std::copy_if(extended_dependencies.begin(), extended_dependencies.end(), std::back_inserter(existential_dependencies), [dependency_map] (int variable) { return dependency_map.find(variable) != dependency_map.end(); } );
    existential_dependencies_map[variable] = existential_dependencies;
    existential_dependencies_map_set[variable] = std::unordered_set<int>(existential_dependencies.begin(), existential_dependencies.end());
  }
}

bool SkolemContainer::checkConsistency(std::vector<int>& assumptions, std::vector<int>& output_universal_counterexample, std::vector<int>& output_existential_counterexample) {
  // Assume (negated) terminals to "close" disjunctions.
  DLOG(trace) << "Assumptions for consistency check: " << consistency_disjunction_terminals << std::endl
              << assumptions << std::endl;
  consistency_solver.assume(consistency_disjunction_terminals);
  consistency_solver.assume(assumptions);
  consistency_solver.assume(assumptions_for_defaults_consistency);
  auto result = consistency_solver.solve();
  bool consistent = (result == 20);
  if (!consistent) {
    DLOG(trace) << "Model of inconsistency check: " << consistency_solver.getModel() << std::endl;
    output_universal_counterexample = consistency_solver.getValues(universal_variables);
    output_existential_counterexample = getInconsistentExistentialAssignment();
    DLOG(trace) << "Universal counterexample: " << output_universal_counterexample << std::endl;
    DLOG(trace) << "Failing existentials: " << output_existential_counterexample << std::endl;
  }
  return consistent;
}

void SkolemContainer::addToConsistencyCheck(int variable, std::vector<Clause>& definition, std::vector<int>& conflict) {
  auto conflict_active = ++last_used_variable;
  // This variable may only be set to true if all literals in conflict are falsified. We add binary clauses to enforce that.
  auto conflict_translated = translateClauseConsistency(conflict);
  for (auto& l: conflict_translated) {
    Clause binary_clause{ l, -conflict_active };
    consistency_solver.addClause(binary_clause);
  }
  // If the conflict is active, set conflict_active to true.
  negateEach(conflict_translated);
  conflict_translated.push_back(conflict_active);
  consistency_solver.addClause(conflict_translated);
  // Rename output variable.
  auto variable_renamed = ++last_used_variable;
  std::unordered_map<int, int> renaming{{variable, variable_renamed}};
  auto definition_renamed = renameFormula(definition, renaming);
  // Add definition to solver.
  for (auto& clause: definition_renamed) {
    auto clause_translated = translateClauseConsistency(clause);
    consistency_solver.addClause(clause_translated);
  }
  // Introduce new variables that may be set to true only if 1) conflict_active is true and 2) variable_renamed is true/false, respectively.
  auto variable_true = ++last_used_variable;
  Clause variable_true_only_if_conflict { conflict_active, -variable_true };
  Clause variable_true_only_if_definition_true { variable_renamed, -variable_true };
  Clause variable_true_if_conflict_and_definition_true { -conflict_active, -variable_renamed, variable_true };
  consistency_solver.addClause(variable_true_only_if_conflict);
  consistency_solver.addClause(variable_true_only_if_definition_true);
  consistency_solver.addClause(variable_true_if_conflict_and_definition_true);
  auto variable_false = ++last_used_variable;
  Clause variable_false_only_if_conflict { conflict_active, -variable_false };
  Clause variable_false_only_if_definition_false { -variable_renamed, -variable_false };
  Clause variable_false_if_conflict_and_definition_false { -conflict_active, variable_renamed, variable_false };
  consistency_solver.addClause(variable_false_only_if_conflict);
  consistency_solver.addClause(variable_false_only_if_definition_false);
  consistency_solver.addClause(variable_false_if_conflict_and_definition_false);
  // Add these variables to the disjunctions over all ways of making a variable true or false.
  addConsistencyDisjunct(variable, variable_true);
  addConsistencyDisjunct(-variable, variable_false);
}

std::vector<int> SkolemContainer::getInconsistentExistentialAssignment() {
  auto conflict_variables_assignment = consistency_solver.getValues(conflict_variables);
  std::vector<int> conflicts;
  // Get all true conflict literals.
  std::copy_if(conflict_variables_assignment.begin(), conflict_variables_assignment.end(), std::back_inserter(conflicts), [](int literal){ return literal > 0; } );
  // Compute corresponding existential variables.
  std::for_each(conflicts.begin(), conflicts.end(), [this](int &variable){ variable = conflict_variable_to_variable[variable]; });
  DLOG(trace) << "Conflicted variables: " << conflicts << std::endl;
  assert(!conflicts.empty());
  int conflicted_variable;
  do {
    // We (arbitrarily) choose the first conflict variable here.
    conflicted_variable = conflicts.front();
    std::vector<int> conflicted_dependencies;
    auto existential_dependencies = existential_dependencies_map_set[conflicted_variable];
    // Subset conflicts to (existential) dependencies.
    std::copy_if(conflicts.begin(), conflicts.end(), std::back_inserter(conflicted_dependencies), [existential_dependencies] (int variable) { return existential_dependencies.find(variable) != existential_dependencies.end(); } );
    std::swap(conflicts, conflicted_dependencies);
  } while (!conflicts.empty());
  // Variable "conflicted_variable" only has existential variables with consistent assignments.
  DLOG(trace) << "Minimal conflicted variable: " << conflicted_variable << std::endl;
  auto existential_dependencies_vector = existential_dependencies_map[conflicted_variable];
  std::for_each(existential_dependencies_vector.begin(), existential_dependencies_vector.end(), [this](int &variable){ variable = literal_to_consistency_solver_variable[variable]; });
  std::vector<int> failing_existential_assignment = consistency_solver.getValues(existential_dependencies_vector);
  std::for_each(failing_existential_assignment.begin(), failing_existential_assignment.end(), 
    [this](int &literal) { 
      auto consistency_solver_variable = var(literal);
      auto original_variable = consistency_solver_variable_to_literal[consistency_solver_variable];
      literal = renameLiteral(literal, original_variable);
    });
  return restrictTo(failing_existential_assignment, undefined_existentials);
}

void SkolemContainer::initValidityCheckModel(SimpleValidityChecker& validity_checker) {
  for (auto e: existential_variables) {
    // We keep the terminals in a vector to pass to the SAT solver inside the validity checker as assumptions,
    // and map each existential variable to the index where its current terminal is stored.
    auto validity_disjunction_terminal = ++last_used_variable;
    variable_to_validity_disjunction_terminal_index[e] = validity_disjunction_terminals.size();
    validity_disjunction_terminals.push_back(-validity_disjunction_terminal);
    // We also create an indicator for each variable that is true when the default value/function is active.
    auto default_active = ++last_used_variable;
    variable_to_default_active_variable[e] = default_active;
    // We initialize the disjunction for e. Variable "default_active" is forced true if no other rule fires.
    Clause initial_disjunct = { default_active, validity_disjunction_terminal };
    validity_checker.addClauseValidityCheck(e, initial_disjunct);
    // A variable that can be used as an assumption determining whether we use the default value or not.
    auto use_default_value = ++last_used_variable;
    variable_to_use_default_index[e] = assumptions_for_defaults.size();
    assumptions_for_defaults.push_back(use_default_value);
    // A variable representing the (configurable) default value for e.
    auto default_value_variable = ++last_used_variable;
    variable_to_default_value_index[e] = assumptions_for_defaults.size();
    assumptions_for_defaults.push_back(-default_value_variable);
    // Clauses enforcing the default value if activate and used.
    std::vector<Clause> default_clauses = { {-use_default_value, -default_active,  default_value_variable, -e}, 
                                            {-use_default_value, -default_active, -default_value_variable,  e} };
    for (Clause& clause: default_clauses) {
      validity_checker.addClauseValidityCheck(e, clause);
    }
    // Finally, we reserve an index in the assumptions for storing the selector for the current default function.
    auto default_function_selector = ++last_used_variable;
    variable_to_default_function_selector_index[e] = assumptions_for_defaults.size();
    assumptions_for_defaults.push_back(-default_function_selector);
  }
  int max_universal=universal_variables.empty()?0:*std::max_element(universal_variables.begin(),universal_variables.end());
  int max_existential=existential_variables.empty()?0:*std::max_element(existential_variables.begin(),existential_variables.end());
  current_model.init(std::max(max_universal,max_existential));
}

void SkolemContainer::addForcingClause(Clause& forcing_clause, bool reduced, SimpleValidityChecker& validity_checker) {
  // Add renamed forcing clause to consistency check (enforce propagation).
  auto translated_forcing_clause = translateClauseConsistency(forcing_clause);
  consistency_solver.addClause(translated_forcing_clause);
  DLOG(trace) << "Translated clause: " << translated_forcing_clause << std::endl;

  current_model.addForcingClause(forcing_clause);
  // First, add the clause to the extracting solver in the validity checker.
  if (reduced) {
    validity_checker.addClauseConflictExtraction(forcing_clause);
  }
  auto forced_literal = forcing_clause.back();
  auto forced_variable = var(forced_literal);
  bool sign = (forced_literal > 0);
  setDefaultValue(forced_variable, sign);
  // Swap forced literal with new variable for disjunct.
  //TODO:
  forcing_clause.pop_back();
  auto forcing_clause_active = ++last_used_variable;
  forcing_clause.push_back(forcing_clause_active);

  // Add this modified clause to the validity check, along with a binary clause that makes sure forced_literal is forced.
  validity_checker.addClauseValidityCheck(forced_variable, forcing_clause);
  //TODO:
  // forcing_clause.pop_back();
  Clause binary_clause = { -forcing_clause_active, forced_literal };
  validity_checker.addClauseValidityCheck(forced_variable, binary_clause);

  // Create new disjunct for the validity check.
  addValidityDisjunct(forced_variable, forcing_clause_active, validity_checker);
  std::vector<Clause> constant_clauses{{forced_literal}};
  // For the both the consistency and validity check, add binary clauses encoding l -> -forcing_clause_active for each l in forcing_clause.
  forcing_clause.pop_back();
  for (auto& l: forcing_clause) {
    Clause binary_clause_validity{ -l, -forcing_clause_active };
    validity_checker.addClauseValidityCheck(forced_variable, binary_clause_validity);
    Clause binary_clause_consistency{ -forcing_clause_active };
    // For the consistency check, we have to replace existential literals.
    if (literal_to_consistency_solver_variable.find(-l) != literal_to_consistency_solver_variable.end()) {
      auto negated_literal_variable = literal_to_consistency_solver_variable[-l];
      binary_clause_consistency.push_back(negated_literal_variable);
    } else {
      binary_clause_consistency.push_back(-l);
    }
    consistency_solver.addClause(binary_clause_consistency);
  }
  forcing_clause.push_back(forced_literal); // Restore original forcing clause.
  // Finally, add new disjunct in consistency solver.
  addConsistencyDisjunct(forced_literal, forcing_clause_active);
}

void SkolemContainer::addDefinition(int variable, std::vector<Clause>& definition, const std::vector<std::tuple<std::vector<int>,int>>& circuit_def, std::vector<int>& conflict, SimpleValidityChecker& validity_checker) {
  current_model.addDefinition(variable,conflict,definition,circuit_def);
  if (conflict.empty()) {
    validity_checker.setDefined(variable);
    undefined_existentials.erase(variable);
  }
  auto conflict_active = ++last_used_variable; // Use a new variable to represent "conflict".
  Clause conflict_selector_clause = conflict;
  negateEach(conflict_selector_clause);
  conflict_selector_clause.push_back(conflict_active);
  validity_checker.addClauseValidityCheck(variable, conflict_selector_clause);
  // Add binary clauses deactivating conflict_active if a conflict literal is falsified.
  for (auto& l: conflict) {
    Clause binary_clause{ l, -conflict_active };
    // current_model.addClauseConditionalDefinition(variable,binary_clause);
    validity_checker.addClauseValidityCheck(variable, binary_clause);
  }
  for (auto& clause: definition) {
    clause.push_back(-conflict_active);
    validity_checker.addClauseValidityCheck(variable, clause);
    clause.pop_back();
  }
  addToConsistencyCheck(variable, definition, conflict);
  addValidityDisjunct(variable, conflict_active, validity_checker);
}

void SkolemContainer::addConsistencyDisjunct(int literal, int literal_true_variable) {
  // Add literal_true_variable to long clause encoding (all "literal_true_variable" variables false) -> -literal_map[literal]
  auto solver_literal = literal_to_consistency_solver_variable[literal];
  auto terminal_index = literal_to_consistency_disjunction_terminal_index[literal];
  auto old_terminal = var(consistency_disjunction_terminals[terminal_index]);
  // We extend the "long clause" by adding a new ternary clause.
  int new_terminal = ++last_used_variable;
  consistency_disjunction_terminals[terminal_index] = -new_terminal;
  Clause ternary_clause = {-old_terminal, literal_true_variable, new_terminal};
  consistency_solver.addClause(ternary_clause);
  DLOG(trace) << "Adding new clause to disjunction: " << ternary_clause << std::endl;
  // Here, we also want to allow literal_map[literal] to be true if it matches the default.
  // We can use a new variable "default_active", shared between both polarities, that can be true only if no other disjunct is true.
  // Another variable, which is added to this disjunction, is can be set true only if the default is active, and if the 
  // default value (for now, we get rid of the default function) matches literal_map[literal].
  auto default_active_variable = variable_to_default_active_variable_consistency[var(literal)];
  Clause disjunct_disables_default = { -literal_true_variable, -default_active_variable };
  consistency_solver.addClause(disjunct_disables_default);
}

void SkolemContainer::addValidityDisjunct(int variable, int variable_disjunct_active, SimpleValidityChecker& validity_checker) {
  auto terminal_index = variable_to_validity_disjunction_terminal_index[variable];
  auto new_terminal = ++last_used_variable;
  auto old_terminal = var(validity_disjunction_terminals[terminal_index]);
  Clause ternary_clause = {-old_terminal, variable_disjunct_active, new_terminal};
  validity_checker.addClauseValidityCheck(variable, ternary_clause);
  validity_disjunction_terminals[terminal_index] = -new_terminal;
  DLOG(trace) << "Adding new disjunct for variable " << variable << " in validity checker: " << ternary_clause << std::endl;
}

void SkolemContainer::setDefaultFunction(int variable, std::vector<Clause>& default_function_clauses, SimpleValidityChecker& validity_checker) {
  current_model.setDefaultFunction(variable,default_function_clauses);
  auto new_default_selector = ++last_used_variable;
  assumptions_for_defaults[variable_to_default_function_selector_index[variable]] = new_default_selector;
  auto default_active = variable_to_default_active_variable[variable];
  for (auto& clause: default_function_clauses) {
    clause.push_back(-new_default_selector);
    clause.push_back(-default_active);
    validity_checker.addClauseValidityCheck(variable, clause);
  }
  // Deactivate default value.
  useDefaultValue(variable, false);
}

Clause SkolemContainer::translateClauseConsistency(Clause& forcing_clause) {
  // For the consistency check, positive and negative polarities are represented by separate variables.
  // This function replaces each literal in a clause by the corresponding variable.
  Clause translated_forcing_clause;
  for (auto l: forcing_clause) {
    if (literal_to_consistency_solver_variable.find(l) != literal_to_consistency_solver_variable.end()) {
      translated_forcing_clause.push_back(literal_to_consistency_solver_variable[l]);
    } else {
      translated_forcing_clause.push_back(l);
    }
  }
  return translated_forcing_clause;
}

//functions related to learning
#ifdef USE_MACHINE_LEARNING

void SkolemContainer::addTrainingExample(int variable, std::vector<int>& universal_assignment, int label_literal) {
  variables_to_learners[variable]->addSample(universal_assignment, label_literal);
}

#endif

}