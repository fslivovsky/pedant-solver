#include <algorithm>
#include <iostream>

#include <assert.h>
#include "utils.h"

#include "simplevaliditychecker.h"
#include "solver_generator.h"

namespace pedant {

SimpleValidityChecker::SimpleValidityChecker(
    const std::vector<int>& existentials,
    const std::vector<int>& universal_variables,
    const DependencyContainer& dependencies,
    std::vector<Clause>& matrix, 
    int& last_used_variable,
    SkolemContainer& skolem_container, const Configuration& config): 
    existential_variables(existentials), universal_variables(universal_variables), matrix(matrix), last_used_variable(last_used_variable), 
    skolem_container(skolem_container), supporttracker(universal_variables, dependencies, last_used_variable, config), config(config) {
  std::sort(existential_variables.begin(), existential_variables.end());
  validity_check_solver = giveSolverInstance(config.validity_solver);
  conflict_extraction_solver = giveSolverInstance(config.conflict_solver);
  auto max_existential = existential_variables.empty() ? 0 : *std::max_element(existential_variables.begin(), existential_variables.end());
  auto max_universal = universal_variables.empty() ? 0 : *std::max_element(universal_variables.begin(), universal_variables.end());
  last_used_variable = std::max({ last_used_variable, max_existential, max_universal });
  assert(last_used_variable >= maxVarIndex(matrix)); // All variables in the matrix must occur in the quantifier prefix.
  DLOG(trace) << "Last variable used by Validity Checker: " << last_used_variable << std::endl;
  conflict_extraction_solver->appendFormula(matrix);
  selectors.reserve(existential_variables.size());
  for (auto variable: existential_variables) {
    auto selector = ++last_used_variable;
    variable_to_selector_index[variable] = selectors.size();
    selectors.push_back(selector);
  }
  auto [matrix_negated, clause_variables] = negateFormula(matrix, last_used_variable);
  clause_satisfied_variables = clause_variables;
  validity_check_solver->appendFormula(matrix_negated);
  DLOG(trace) << "Negated matrix: " << matrix_negated << std::endl;
  // Compute existential and universal dependencies.
  std::unordered_set<int> existential_variables_set(existential_variables.begin(), existential_variables.end());
  std::unordered_set<int> universal_variables_set(universal_variables.begin(), universal_variables.end());
  for (auto variable: existential_variables) {
    supporttracker.setAlias(variable, variable);
    supporttracker.setAlias(-variable, -variable);
  }
  supporttracker.setSatSolver(validity_check_solver);
}

bool SimpleValidityChecker::checkArbiterAssignment(std::vector<int>& arbiter_assignment) {
  validity_check_solver->assume(skolem_container.validityCheckAssumptions());
  validity_check_solver->assume(arbiter_assignment);
  validity_check_solver->assume(selectors);
  auto solver_result = validity_check_solver->solve();
  if (solver_result == 10) {
    DLOG(trace) << "Validity check failed." << std::endl;
    setFailingAssignments(arbiter_assignment);
    DLOG(trace) << "Falsifying universal assignment: " << failing_universal_assignment << std::endl
                << "Falsifying existential assignment: " << failing_existential_assignment << std::endl;
    DLOG(trace) << "Full assignment: " << validity_check_solver->getModel() << std::endl;
    return false;
  } else {
    // Check whether UNSAT answer is due to an inconsistency in the forcing clauses.
    DLOG(trace) << "Validity check passed. " << std::endl;
    DLOG(trace) << "Failed selectors: " << validity_check_solver->getFailed(selectors) << std::endl;
    assert(solver_result == 20);
    bool consistent = skolem_container.checkConsistency(arbiter_assignment, failing_existential_assignment, failing_universal_assignment, failing_arbiter_assignment, full_universal_assignment);
    return consistent;
  }
}

void SimpleValidityChecker::setDefined(int variable) {
  DLOG(trace) << "Removing " << variable << " from undefined variables in validity checker." << std::endl;
  existential_variables.erase(std::find(existential_variables.begin(), existential_variables.end(), variable));
  auto new_selector = ++last_used_variable;
  selector(variable) = new_selector;
}

void SimpleValidityChecker::addClauseValidityCheck(int variable, Clause& clause) {
  clause.push_back(-selector(variable));
  DLOG(trace) << "Adding clause to validity check solver: " << clause << std::endl;
  validity_check_solver->addClause(clause);
  clause.pop_back();
}

std::tuple<Clause, Clause, Clause, Clause, Clause> SimpleValidityChecker::getConflict() {
  bool result = hasConflict(failing_existential_assignment, failing_universal_assignment, failing_arbiter_assignment);
  if (!result) {
    DLOG(trace) << "Error: no conflict found for assignments: " << failing_existential_assignment << ", " << failing_universal_assignment << std::endl;
    DLOG(trace) << "Assignment of existentials: " << conflict_extraction_solver->getValues(existential_variables) << std::endl
                << "Assignment of universals: " << conflict_extraction_solver->getValues(universal_variables) << std::endl;
    DLOG(trace) << "Full assignment: " << conflict_extraction_solver->getModel() << std::endl;
  }
  assert(result);
  auto failing_existential_core = conflict_extraction_solver->getFailed(failing_existential_assignment);
  auto failing_universal_core = conflict_extraction_solver->getFailed(failing_universal_assignment);
  auto failing_arbiter_core = conflict_extraction_solver->getFailed(failing_arbiter_assignment);
  return std::make_tuple(failing_existential_core, failing_universal_core, failing_arbiter_core, full_universal_assignment, full_existential_assignment);
}


void SimpleValidityChecker::setFailingAssignments(std::vector<int>& arbiter_assignment) {
  full_universal_assignment = validity_check_solver->getValues(universal_variables);
  full_existential_assignment = validity_check_solver->getValues(existential_variables);
  if (config.sup_strat == Core) {
    failing_universal_assignment = full_universal_assignment;
    failing_existential_assignment = full_existential_assignment;
    failing_arbiter_assignment = arbiter_assignment;
    return;
  }
  // Restrict to variables relevant for a clause in the matrix that is falsified.
  auto clause_satisfied_variables_assignment = validity_check_solver->getValues(clause_satisfied_variables);
  int i;
  for (i = 0; i < clause_satisfied_variables_assignment.size() && clause_satisfied_variables_assignment[i] > 0; i++);
  assert(i < clause_satisfied_variables_assignment.size());
  Clause& falsified_clause = matrix[i];
  int forced_variable = supporttracker.getForcedSource(falsified_clause);
  auto [existential_support, universal_support, arbiter_support] = supporttracker.computeSupport(falsified_clause, forced_variable);
  failing_existential_assignment = validity_check_solver->getValues(existential_support);
  failing_universal_assignment = validity_check_solver->getValues(universal_support);
  failing_arbiter_assignment = validity_check_solver->getValues(arbiter_support);
}

void SimpleValidityChecker::minimizeAssumptions(std::vector<int>& assumptions_to_minimize, std::vector<int>& assumptions_to_keep, std::vector<int>& other_assumptions_to_keep) {
  DLOG(trace) << "Trying to minimize " << assumptions_to_minimize.size() << " assumptions." << std::endl;
  std::vector<int> arbiter_assumptions{};
  // Try removing an assumption literal.
  int assumptions_removed = 0;
  for (int i = 0; i < assumptions_to_minimize.size();) {
    int literal_removed = assumptions_to_minimize[i];
    assumptions_to_minimize[i] = assumptions_to_minimize.back();
    assumptions_to_minimize.pop_back();
    if (!hasConflict(assumptions_to_minimize, assumptions_to_keep, other_assumptions_to_keep, 0)) {
      // Put literal back into assumptions and advance index.
      assumptions_to_minimize.push_back(assumptions_to_minimize[i]);
      assumptions_to_minimize[i] = literal_removed;
      i++;
    } else {
      assumptions_removed++;
    }
  }
  DLOG(trace) << "Removed " << assumptions_removed << " assumption literal(s)." << std::endl;
}

std::vector<int> SimpleValidityChecker::getExistentialResponse(const std::vector<int>& universal_assignment, const std::vector<int>& arbiter_assignment) {
  conflict_extraction_solver->assume(universal_assignment);
  conflict_extraction_solver->assume(arbiter_assignment);
  auto result = conflict_extraction_solver->solve();
  if (result == 10) {
    return conflict_extraction_solver->getValues(existential_variables);
  } else {
    return {};
  }
}

}