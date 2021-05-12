#include "definabilitychecker.h"

#include <stdlib.h> 
#include <assert.h>

#include <algorithm>
#include <map>
#include <memory>
#include <iostream>

#include "utils.h"
#include "logging.h"

namespace pedant {

DefinabilityChecker::DefinabilityChecker(std::vector<int>& universal_variables, std::unordered_map<int, std::vector<int>>& dependency_map, std::vector<Clause>& matrix, int& last_used_variable, bool compress): last_used_variable(last_used_variable), compress(compress) {
  auto existential_variables = getKeys(dependency_map);
  auto max_existential = existential_variables.empty() ? 0 : *std::max_element(existential_variables.begin(), existential_variables.end());
  auto max_universal = universal_variables.empty() ? 0 : *std::max_element(universal_variables.begin(), universal_variables.end());
  last_used_variable = std::max({ last_used_variable, max_existential, max_universal });
  assert(last_used_variable >= maxVarIndex(matrix)); // All variables in the matrix must occur in the quantifier prefix.
  // Create renaming (and inverse) for "formula" by mapping variable v -> v + max_variable.
  for (auto v: universal_variables) {
    auto renamed_v = ++last_used_variable;
    renaming[v] = renamed_v;
    renaming_inverse[renamed_v] = v;
  }
  for (auto& v: existential_variables) {
    auto renamed_v = ++last_used_variable;
    renaming[v] = renamed_v;
    renaming_inverse[renamed_v] = v;
  }
  auto first_part = matrix;
  auto second_part = renameFormula(matrix, renaming);
  // Create dictionary of selector variables to activate equality v == renaming[v].
  // Append equality clauses to copy of formula.
  for (auto& [v, v_renamed]: std::map<int, int>(renaming.begin(), renaming.end())) {
    int selector = ++last_used_variable;
    variable_to_equality_selector[v] = selector;
    auto equality_clauses = clausalEncodingEquality(v, v_renamed, selector);
    second_part.insert(second_part.end(), equality_clauses.begin(), equality_clauses.end());
  }
  // Assumptions for switching literals on and off must be local to each part, so we have to define new selectors.
  for (auto& v: existential_variables) {
    auto on_selector = ++last_used_variable;
    variable_to_on_selector[v] = on_selector;
    first_part.push_back(Clause{-on_selector, v});
    auto off_selector = ++last_used_variable;
    variable_to_off_selector[v] = off_selector;
    second_part.push_back(Clause{-off_selector, -renaming[v]});
  }
  // Initialize SAT solvers.
  interpolating_solver = std::make_unique<ITPSolver>(first_part, second_part);
  fast_solver.appendFormula(first_part);
  fast_solver.appendFormula(second_part);
  backbone_solver.appendFormula(matrix);
}

void DefinabilityChecker::addSharedVariable(int variable) {
  auto variable_alias = ++last_used_variable;
  renaming[variable] = variable_alias;
  renaming_inverse[variable_alias] = variable;
  auto eq_selector = ++last_used_variable;
  variable_to_equality_selector[variable] = eq_selector;
  auto equality_clauses = clausalEncodingEquality(variable, variable_alias, eq_selector);
  fast_solver.appendFormula(equality_clauses);
  for (auto& clause: equality_clauses) {
    interpolating_solver->addClause(clause, false);
  }
}

void DefinabilityChecker::addClause(Clause& clause) {
  interpolating_solver->addClause(clause);
  fast_solver.addClause(clause);
  auto renamed_clause = renameClause(clause, renaming);
  interpolating_solver->addClause(renamed_clause, false);
  fast_solver.addClause(renamed_clause);
  backbone_solver.addClause(clause);
}

std::tuple<bool, std::vector<Clause>, std::vector<int>> DefinabilityChecker::checkForced(int variable, std::vector<int>& assumptions) {
  assumptions.push_back(variable);
  auto solver_result = backbone_solver.solve(assumptions);
  assumptions.pop_back();
  if (solver_result == 20) {
    auto core = backbone_solver.getFailed(assumptions);
    std::vector<Clause> definition{{-variable}};
    return std::make_tuple(true, definition, core);
  } else {
    assert(solver_result == 10);
    assumptions.push_back(-variable);
    auto solver_result = backbone_solver.solve(assumptions);
    assumptions.pop_back();
    if (solver_result == 20) {
      auto core = backbone_solver.getFailed(assumptions);
      std::vector<Clause> definition{{variable}};
      return std::make_tuple(true, definition, core);
    } else {
      assert(solver_result == 10);
      return std::make_tuple(false, std::vector<Clause>{}, std::vector<int>{});
    }
  }
}

std::vector<int> DefinabilityChecker::getConflict(std::vector<int>& assumptions) {
  auto conflict = interpolating_solver->getConflict();
  auto conflict_original = renameClause(conflict, renaming_inverse);
  std::sort(conflict_original.begin(), conflict_original.end());
  std::sort(assumptions.begin(), assumptions.end());
  std::vector<int> intersection(conflict_original.size());
  auto it = std::set_intersection(conflict_original.begin(), conflict_original.end(), assumptions.begin(), assumptions.end(), intersection.begin());
  intersection.resize(it-intersection.begin());
  return intersection;
}

std::tuple<std::vector<Clause>,std::vector<std::tuple<std::vector<int>,int>>> DefinabilityChecker::getDefinition(std::vector<int>& defining_variables, int defined_variable) {
  auto [definitions, _] = interpolating_solver->getDefinition(defining_variables, defined_variable, last_used_variable, compress);
  std::vector<Clause> clausal_encoding_definitions;
  for (auto& definition: definitions) {
    auto clauses_definition = clausalEncodingAND(definition);
    clausal_encoding_definitions.insert(clausal_encoding_definitions.end(), clauses_definition.begin(), clauses_definition.end());
  }
  return std::make_tuple(clausal_encoding_definitions,definitions);
}

std::tuple<bool, std::vector<Clause>, std::vector<std::tuple<std::vector<int>,int>>, std::vector<int>> DefinabilityChecker::checkDefinability(std::vector<int>& defining_variables, int defined_variable, std::vector<int>& assumptions) {
  DLOG(trace) << "Checking for definition of " << defined_variable << " in terms of " << defining_variables << std::endl;
  auto [is_forced, forced_definition, core] = checkForced(defined_variable, assumptions);
  if (is_forced) {
    std::vector<std::tuple<std::vector<int>,int>> cdef;
    cdef.push_back(std::make_tuple(std::vector<int>{},forced_definition[0][0]));
    return std::make_tuple(is_forced, forced_definition, cdef, core);
  }
  auto renamed_assumptions = renameClause(assumptions, renaming);
  fast_solver.assume(assumptions);
  fast_solver.assume(renamed_assumptions);
  std::vector<int> eq_selector_and_defined_assumptions;
  for (auto& variable: defining_variables) {
    try {
      eq_selector_and_defined_assumptions.push_back(variable_to_equality_selector.at(variable));
    } 
    catch (const std::out_of_range&) {}
  }
  // Note: the Python implementation adds the remaining selectors as negated assumptions. We omit that here.
  eq_selector_and_defined_assumptions.push_back(variable_to_on_selector[defined_variable]);
  eq_selector_and_defined_assumptions.push_back(variable_to_off_selector[defined_variable]);
  fast_solver.assume(eq_selector_and_defined_assumptions);

  auto solver_result = fast_solver.solve();
  if (solver_result == 10) {
    auto assignment = fast_solver.getValues(defining_variables);
    return std::make_tuple(false, std::vector<Clause>{}, std::vector<std::tuple<std::vector<int>,int>>{}, assignment);
  }
  // UNSAT, call "slow" interpolating solver and extract definition from proof.
  assert(solver_result == 20);
  std::vector<int> assumptions_all = assumptions;
  assumptions_all.insert(assumptions_all.end(), renamed_assumptions.begin(), renamed_assumptions.end());
  assumptions_all.insert(assumptions_all.end(), eq_selector_and_defined_assumptions.begin(), eq_selector_and_defined_assumptions.end());
  bool satisfiable = interpolating_solver->solve(assumptions_all);
  assert(!satisfiable);
  try {
    auto [definition_clauses,definition_circuit] = getDefinition(defining_variables, defined_variable);
    last_used_variable = std::max(last_used_variable, maxVarIndex(definition_clauses));
    return std::make_tuple(true, definition_clauses,definition_circuit, getConflict(assumptions));
  }
  catch (const std::invalid_argument&) {
    // Handle errors during interpolation by resetting solver and trying again.
    DLOG(info) << "Invalid arguments during interpolation, resetting solver." << std::endl;
    interpolating_solver->resetSolver();
    return checkDefinability(defining_variables, defined_variable, assumptions);
  }
}

}