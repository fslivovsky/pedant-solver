// #include <algorithm>
#include <iostream>

#include <assert.h>
#include "utils.h"

#include "simplevaliditychecker.h"



namespace pedant {

SimpleValidityChecker::SimpleValidityChecker( std::vector<int>& universal_variables, 
    std::unordered_map<int, std::vector<int>>& dependency_map, 
    std::vector<Clause>& matrix, 
    int& last_used_variable): universal_variables(universal_variables), last_used_variable(last_used_variable) {
  existential_variables = getKeys(dependency_map);
  auto max_existential = existential_variables.empty() ? 0 : *std::max_element(existential_variables.begin(), existential_variables.end());
  auto max_universal = universal_variables.empty() ? 0 : *std::max_element(universal_variables.begin(), universal_variables.end());
  last_used_variable = std::max({ last_used_variable, max_existential, max_universal });
  assert(last_used_variable >= maxVarIndex(matrix)); // All variables in the matrix must occur in the quantifier prefix.
  DLOG(trace) << "Last variable used by Validity Checker: " << last_used_variable << std::endl;

  #ifdef USE_GLUCOSE
  conflict_extraction_solver.appendFormula(matrix,last_used_variable);
  #else
  conflict_extraction_solver.appendFormula(matrix);
  #endif

  selectors.reserve(existential_variables.size());
  for (auto variable: existential_variables) {
    auto selector = ++last_used_variable;
    variable_to_selector_index[variable] = selectors.size();
    selectors.push_back(selector);
  }
  auto matrix_negated = negateFormula(matrix, last_used_variable);
  validity_check_solver.appendFormula(matrix_negated);
  DLOG(trace) << "Negated matrix: " << matrix_negated << std::endl;
}

bool SimpleValidityChecker::checkArbiterAssignment(std::vector<int>& arbiter_assignment, SkolemContainer& skolem_container) {
  auto model_validity_check_assumptions = skolem_container.validityCheckAssumptions();
  validity_check_solver.assume(model_validity_check_assumptions);
  auto model_validity_check_soft_assumptions = skolem_container.validityCheckSoftAssumptions();
  validity_check_solver.assume(model_validity_check_soft_assumptions);
  validity_check_solver.assume(arbiter_assignment);
  validity_check_solver.assume(selectors);
  auto solver_result = validity_check_solver.solve();
  if (solver_result == 10) {
    // Counterexample found, store for conflict extraction.
    failing_existential_assignment = validity_check_solver.getValues(existential_variables);
    failing_universal_assignment = validity_check_solver.getValues(universal_variables);
    failing_arbiter_assignment = arbiter_assignment;
    DLOG(trace) << "Falsifying existential assignment: " << failing_existential_assignment << std::endl
                << "Falsifying universal assignment: " << failing_universal_assignment << std::endl
                << "Falsifying arbiter assignment: " << failing_arbiter_assignment << std::endl;
    return false;
  } else {
    // Check whether UNSAT answer is due to an inconsistency in the forcing clauses.
    DLOG(trace) << "Validity check passed. " << std::endl;
    assert(solver_result == 20);
    bool consistent = skolem_container.checkConsistency(arbiter_assignment, failing_universal_assignment, failing_existential_assignment);
    if (!consistent) {
      failing_arbiter_assignment = arbiter_assignment;
    }
    return consistent;
  }
}

void SimpleValidityChecker::setDefined(int variable) {
  existential_variables.erase(std::find(existential_variables.begin(), existential_variables.end(), variable));
  auto new_selector = ++last_used_variable;
  selector(variable) = new_selector;
}

void SimpleValidityChecker::addClauseValidityCheck(int variable, Clause& clause) {
  clause.push_back(-selector(variable));
  DLOG(trace) << "Adding clause to validity check solver: " << clause << std::endl;
  validity_check_solver.addClause(clause);
  clause.pop_back();
}

std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> SimpleValidityChecker::getConflict() {
  conflict_extraction_solver.assume(failing_arbiter_assignment);
  conflict_extraction_solver.assume(failing_universal_assignment);
  conflict_extraction_solver.assume(failing_existential_assignment);
  DLOG(trace) << "Falsifying existential assignment: " << failing_existential_assignment << std::endl
              << "Falsifying universal assignment: " << failing_universal_assignment << std::endl
              << "Falsifying arbiter assignment: " << failing_arbiter_assignment << std::endl;
  int result = conflict_extraction_solver.solve();
  assert (result == 20);
  return std::make_tuple(failing_universal_assignment, conflict_extraction_solver.getFailed(failing_existential_assignment), conflict_extraction_solver.getFailed(failing_arbiter_assignment));
}

// void SimpleValidityChecker::setDefined(int variable) {
//   existential_variables.erase(std::find(existential_variables.begin(), existential_variables.end(), variable));
//   selectors.erase(std::find(selectors.begin(), selectors.end(), variable_to_selector[variable]));
//   auto selector = ++last_used_variable;
//   variable_to_selector[variable] = selector;
//   selectors.push_back(selector);
// }

// void SimpleValidityChecker::addClauseValidityCheck(int variable, Clause& clause) {
//   clause.push_back(-variable_to_selector[variable]);
//   validity_check_solver.addClause(clause);
//   clause.pop_back();
// }

}