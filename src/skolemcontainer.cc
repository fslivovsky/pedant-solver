#include "skolemcontainer.h"

#include <iostream>
#include <algorithm>
#include <map>
#include <exception>

#include "utils.h"

namespace pedant {

SkolemContainer::SkolemContainer(const std::vector<int>& universal_variables, 
      const std::vector<int>& existential_variables,
      DependencyContainer& dependencies,
      int& last_used_variable, SimpleValidityChecker& validity_checker, const Configuration& config) : 
      last_used_variable(last_used_variable), bernoulli(0, 1), 
      universal_variables(universal_variables), existential_variables(existential_variables),
      undefined_existentials(existential_variables.begin(), existential_variables.end()),
      current_model(existential_variables,universal_variables, default_values, config),
      consistencychecker(dependencies, 
          existential_variables, universal_variables, last_used_variable,config),
      validity_checker(validity_checker), config(config),
      default_values(universal_variables, existential_variables, dependencies, 
          last_used_variable,validity_check_assumptions, config),
      dependencies(dependencies) {
  initValidityCheckModel();
}

void SkolemContainer::initDefaultValues() {
  intitDefaultContainer();
}

void SkolemContainer::initValidityCheckModel() {
  for (auto e: existential_variables) {
    variable_to_forcing_disjunction.emplace(e, Disjunction(last_used_variable, validity_check_assumptions));
    auto& forcing_disjunction = variable_to_forcing_disjunction.at(e);
    // We create an indicator for each variable that is true if no forcing clause is active.
    auto no_forcing_clause_active = ++last_used_variable;
    DLOG(trace) << "[Skolemcontainer] No forcing clause active indicator for variable " << e << ": " << no_forcing_clause_active << std::endl;
    variable_to_no_forcing_clause_active_variable[e] = no_forcing_clause_active;
    validity_checker.setNoForcingClauseActiveVariable(e, no_forcing_clause_active);
    // We initialize the disjunction for e. Variable "default_active" is forced true if no other rule fires.
    Clause initial_forcing_disjunct = forcing_disjunction.createDisjunct({ no_forcing_clause_active });
    validity_checker.addClauseValidityCheck(e, initial_forcing_disjunct);
    // Create a default value.
    int use_default = ++last_used_variable;
    apply_default[e] = use_default;
    // A flag indicating whether to use the default value.
    variable_to_use_default_index[e] = validity_check_assumptions.size();
    auto use_default_value = ++last_used_variable;
    validity_check_assumptions.push_back(use_default_value);
    Clause default_activator {-no_forcing_clause_active, -use_default_value, use_default};
    validity_checker.addClauseValidityCheck(e, default_activator);
    // Set default function selector to -1 (undefined).
    variable_to_default_function_selector_index[e] = -1;
  }
  int max_universal=universal_variables.empty()?0:*std::max_element(universal_variables.begin(),universal_variables.end());
  int max_existential=existential_variables.empty()?0:*std::max_element(existential_variables.begin(),existential_variables.end());
  current_model.init(std::max(max_universal, max_existential));

  if (config.random_seed_set) {
    re.seed(config.random_seed);
  }
}

void SkolemContainer::addForcingClause(Clause& forcing_clause, bool reduced) {
  consistencychecker.addForcingClause(forcing_clause);
  current_model.addForcingClause(forcing_clause);
  // First, add the clause to the extracting solver in the validity checker if it is not entailed by the matrix.
  if (reduced) {
    validity_checker.addClauseConflictExtraction(forcing_clause);
  }
  auto forced_literal = forcing_clause.back();
  auto forced_variable = var(forced_literal);
  // Swap forced literal with new variable for disjunct.
  forcing_clause.pop_back();
  auto forcing_clause_active = ++last_used_variable;
  forcing_clause.push_back(forcing_clause_active);

  // Add this modified clause to the validity check, along with a binary clause that makes sure forced_literal is forced.
  validity_checker.addClauseValidityCheck(forced_variable, forcing_clause);
  Clause binary_clause = { -forcing_clause_active, forced_literal };
  validity_checker.addClauseValidityCheck(forced_variable, binary_clause);
  // Add to support tracker within the validity checker.
  validity_checker.addDefiningClause(forced_variable, forcing_clause, forcing_clause_active);

  // If this clause fires, deactivate the default.
  auto default_active_variable = variable_to_no_forcing_clause_active_variable[forced_variable];
  Clause deactivate_default = { -forcing_clause_active, -default_active_variable };
  validity_checker.addClauseValidityCheck(forced_variable, deactivate_default);

  // Create new disjunct for the validity check.
  addForcingDisjunct(forced_variable, forcing_clause_active);
  // Add binary clauses encoding l -> -forcing_clause_active for each l in forcing_clause.
  forcing_clause.pop_back();
  for (auto& l: forcing_clause) {
    Clause binary_clause_deactivate{ -l, -forcing_clause_active };
    validity_checker.addClauseValidityCheck(forced_variable, binary_clause_deactivate);
  }
  forcing_clause.push_back(forced_literal); // Restore original forcing clause.
}

void SkolemContainer::addDefinition(int variable, std::vector<Clause>& definition, const std::vector<std::tuple<std::vector<int>,int>>& circuit_def, std::vector<int>& conflict, bool reduced) {
  current_model.addConditionalDefinition(variable, conflict, definition, circuit_def);
  consistencychecker.addDefinition(variable, definition, conflict);
  if (conflict.empty() && undefined_existentials.find(variable) != undefined_existentials.end()) {
    validity_checker.setDefined(variable);
    undefined_existentials.erase(variable);
  }
  auto conflict_active = ++last_used_variable; // Use a new variable to represent "conflict".
  Clause conflict_selector_clause = conflict;
  negateEach(conflict_selector_clause);
  conflict_selector_clause.push_back(conflict_active);
  validity_checker.addClauseValidityCheck(variable, conflict_selector_clause);
  if (reduced) {
    validity_checker.addClauseConflictExtraction(conflict_selector_clause);
  }
  // Add binary clauses deactivating conflict_active if a conflict literal is falsified.
  for (auto& l: conflict) {
    Clause binary_clause_deactivate{ l, -conflict_active };
    validity_checker.addClauseValidityCheck(variable, binary_clause_deactivate);
  }
  // Default function modifications.
  // Create a new selector that is true if, and only if, 
  // 1) the conflict is active, or
  // 1) an indicator for using the new default function selector is active.
  int selector = ++last_used_variable;
  int new_default_function_active = ++last_used_variable;
  Clause selector_deactivate = { conflict_active, new_default_function_active, -selector };
  Clause selector_activate_conflict = {-conflict_active, selector };
  Clause selector_activate_default = {-new_default_function_active, selector };
  validity_checker.addClauseValidityCheck(variable, selector_deactivate);
  validity_checker.addClauseValidityCheck(variable, selector_activate_conflict);
  validity_checker.addClauseValidityCheck(variable, selector_activate_default);
  // Now specificy when new_default_function_active is set.
  // This is the case if 1) no forcing clause is active, and 2) a new selector for using the default function is set.
  int new_default_function_selector = newDefaultFunctionSelector(variable);
  auto no_forcing_clause_active = variable_to_no_forcing_clause_active_variable[variable];
  Clause new_default_function_active_noforcing = { -new_default_function_active, no_forcing_clause_active };
  Clause new_default_function_active_selector = { -new_default_function_active, new_default_function_selector };
  Clause new_default_function_active_enforced = { -no_forcing_clause_active, -new_default_function_selector, new_default_function_active };
  validity_checker.addClauseValidityCheck(variable, new_default_function_active_noforcing);
  validity_checker.addClauseValidityCheck(variable, new_default_function_active_selector);
  validity_checker.addClauseValidityCheck(variable, new_default_function_active_enforced);

  // If this definition is active, deactivate the variable indicating that the variable is not forced.
  Clause deactivate_noforcing = { -conflict_active, -no_forcing_clause_active };
  validity_checker.addClauseValidityCheck(variable, deactivate_noforcing);

  // In the conflict extraction solver, we only need the implication conflict_active -> selector.
  if (reduced) {
    Clause conflict_implies_selector = { -conflict_active, selector };
    validity_checker.addClauseConflictExtraction(conflict_implies_selector);
  }
  addForcingDisjunct(variable, conflict_active);
  addDefinitionWithSelector(variable, definition, selector, !conflict.empty() || reduced);

  auto support = dependencies.restrictToExtendedDendencies(getSupport(conflict, definition), variable);//TODO: add condition config.extededdeps
  validity_checker.addDefiningClause(variable, support, conflict_active);
}

void SkolemContainer::addDefinitionWithSelector(int variable, std::vector<Clause>& definition, int selector, bool reduced) {
  for (auto& clause: definition) {
    clause.push_back(-selector);
    validity_checker.addClauseValidityCheck(variable, clause);
    if (reduced) {
      validity_checker.addClauseConflictExtraction(clause);
    }
    clause.pop_back();
  }
}

void SkolemContainer::addForcingDisjunct(int variable, int variable_disjunct_active) {
  auto forcing_disjunction = variable_to_forcing_disjunction.at(variable);
  Clause new_disjunct_clause = forcing_disjunction.createDisjunct({ variable_disjunct_active });
  validity_checker.addClauseValidityCheck(variable, new_disjunct_clause);
  DLOG(trace) << "Adding new disjunct for variable " << variable << " in validity checker: " << new_disjunct_clause << std::endl;
}

std::tuple<int, bool, bool> SkolemContainer::getArbiter(int existential_variable, const std::vector<int>& assignment_dependencies, bool introduce_clauses) {
  auto assignment_dependencies_sorted = assignment_dependencies;
  std::sort(assignment_dependencies_sorted.begin(), assignment_dependencies_sorted.end());
  bool is_new_arbiter = false;
  if (!existsArbiter(existential_variable, assignment_dependencies_sorted)) {
    createArbiter(existential_variable, assignment_dependencies_sorted);
    is_new_arbiter = true;
  }
  int arbiter = existential_arbiter_map[existential_variable][assignment_dependencies_sorted];
  if (introduce_clauses && proper_arbiters.find(arbiter) == proper_arbiters.end()) {
    realizeArbiter(arbiter);
  }
  bool is_proper_arbiter = (proper_arbiters.find(arbiter) != proper_arbiters.end());
  return std::make_tuple(arbiter, is_new_arbiter, is_proper_arbiter);
}

int SkolemContainer::createArbiter(int existential_variable, const std::vector<int>& annotation) {
  auto new_arbiter = ++last_used_variable;
  // Store annotation (sorted).
  auto annotation_copy = annotation;
  std::sort(annotation_copy.begin(), annotation_copy.end());
  arbiter_to_annotation[new_arbiter] = annotation_copy;
  DLOG(trace) << "Creating new arbiter " << new_arbiter << " for variable " << existential_variable << " and annotation " << annotation_copy << std::endl;
  arbiter_to_existential[new_arbiter] = existential_variable;
  existential_arbiter_map[existential_variable][annotation_copy] = new_arbiter;
  return new_arbiter;
}

void SkolemContainer::realizeArbiter(int arbiter) {
  DLOG(trace) << "Realizing arbiter: " << arbiter << std::endl;
  auto existential_variable = arbiter_to_existential[arbiter];
  consistencychecker.addArbiterVariable(existential_variable, arbiter);
  validity_checker.addArbiterVariable(arbiter); // For support tracker.
  // Add clauses linking arbiter and existential variable.
  Clause positive_clause = arbiter_to_annotation[arbiter];
  negateEach(positive_clause);
  positive_clause.push_back(-arbiter);
  positive_clause.push_back(existential_variable);
  addArbiterClause(positive_clause);
  Clause negative_clause = arbiter_to_annotation[arbiter];
  negateEach(negative_clause);
  negative_clause.push_back(arbiter);
  negative_clause.push_back(-existential_variable);
  addArbiterClause(negative_clause);
  proper_arbiters.insert(arbiter);
}

int SkolemContainer::newDefaultFunctionSelector(int existential_variable) {
  if (variable_to_default_function_selector_index[existential_variable] != -1) {
     disableDefaultFunctionSelector(existential_variable);
  }
  variable_to_default_function_selector_index[existential_variable] = validity_check_assumptions.size();
  auto selector = ++last_used_variable;
  DLOG(trace) << "[Consistencychecker] New default function selector for variable " << existential_variable << ": " << selector << std::endl;
  validity_check_assumptions.push_back(-selector);
  return selector;
}

void SkolemContainer::addArbiterClause(Clause& arbiter_clause) {
  consistencychecker.addForcingClause(arbiter_clause);
  validity_checker.addClauseConflictExtraction(arbiter_clause);
  current_model.addForcingClause(arbiter_clause);
  auto implied_literal = arbiter_clause.back();
  auto implied_variable = var(implied_literal);
  arbiter_clause.pop_back();
  auto clause_active = ++last_used_variable;
  arbiter_clause.push_back(clause_active);
  validity_checker.addClauseValidityCheck(implied_variable, arbiter_clause);
  validity_checker.addDefiningClause(implied_variable, arbiter_clause, clause_active);
  Clause binary_clause = { -clause_active, implied_literal };
  validity_checker.addClauseValidityCheck(implied_variable, binary_clause);
  auto no_forcing_active = variable_to_no_forcing_clause_active_variable[implied_variable];
  Clause deactivate_no_forcing_active = { -clause_active, -no_forcing_active };
  validity_checker.addClauseValidityCheck(implied_variable, deactivate_no_forcing_active);
  // Create new disjunct for the validity check.
  auto forcing_disjunction = variable_to_forcing_disjunction.at(implied_variable);
  Clause new_disjunct_clause = forcing_disjunction.createDisjunct({ clause_active });
  validity_checker.addClauseValidityCheck(implied_variable, new_disjunct_clause);
  DLOG(trace) << "Adding new forcing disjunct from arbiter clause for variable " << implied_variable << " in validity checker: " << new_disjunct_clause << std::endl;
  // Add binary clauses encoding l -> -clause_active for each l.
  arbiter_clause.pop_back(); // clause_active
  for (auto& l: arbiter_clause) {
    Clause binary_clause_deactivate{ -l, -clause_active };
    validity_checker.addClauseValidityCheck(implied_variable, binary_clause_deactivate);
  }
  arbiter_clause.push_back(implied_literal); // Restore arbiter clause.
}

void SkolemContainer::insertIntoDefaultContainer(int existential_literal, const std::vector<int>& universal_assignment) {
  int variable = var(existential_literal);
  std::vector<Clause> clauses = default_values.insertConflict(existential_literal,universal_assignment);
  addDefaultClauses(variable,clauses);
}

void SkolemContainer::insertIntoDefaultContainer(int existential_literal, const std::vector<int>& universal_assignment, const std::vector<int>& existential_assignment) {
  std::vector<int> assignment (universal_assignment.begin(),universal_assignment.end());
  assignment.insert(assignment.end(), existential_assignment.begin(), existential_assignment.end());
  std::vector<Clause> clauses = default_values.insertConflict(existential_literal,assignment);
  int variable = var(existential_literal);
  addDefaultClauses(variable,clauses);
}

void SkolemContainer::addDefaultClause(int variable, Clause& clause, int use_default_variable) {
  int label = clause.back();
  clause.pop_back();
  consistencychecker.addDefaultClause(clause,label);
  clause.push_back(label);
  clause.push_back(-use_default_variable);
  validity_checker.addClauseValidityCheck(variable, clause);
  clause.pop_back();
}

void SkolemContainer::insertSamplesIntoDefaultContainer(const std::vector<Clause>& matrix, const std::set<int>& variables_to_sample) {
  std::vector<int> variables_to_sample_vector (variables_to_sample.begin(), variables_to_sample.end());
  auto clauses = default_values.insertSample(matrix,variables_to_sample_vector);
  if (!clauses.empty()) {
    for (int i=0; i<variables_to_sample_vector.size(); i++) {
      int variable = variables_to_sample_vector[i];
      addDefaultClauses(variable,clauses[i]);
    }
  }
}


// #endif

}