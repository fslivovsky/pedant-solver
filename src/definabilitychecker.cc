#include "definabilitychecker.h"

#include <stdlib.h> 
#include <assert.h>

#include <algorithm>
#include <map>
#include <memory>
#include <iostream>

#include "utils.h"
#include "logging.h"

#include "solver_generator.h"

namespace pedant {

DefinabilityChecker::DefinabilityChecker( std::vector<int>& universal_variables, const std::vector<int>& existential_variables, 
                                          std::vector<Clause>& matrix, int& last_used_variable, const Configuration& config, bool compress) : 
                                          last_used_variable(last_used_variable), compress(compress), config(config) {
  if (!config.definitions) {
    return;
  }
  backbone_solver = giveSolverInstance(config.definability_solver);
  fast_solver = giveSolverInstance(config.definability_solver);
  // auto existential_variables = getKeys(dependency_map);
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
  for (const auto& v: existential_variables) {
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
  for (const auto& v: existential_variables) {
    auto on_selector = ++last_used_variable;
    variable_to_on_selector[v] = on_selector;
    first_part.push_back(Clause{-on_selector, v});
    auto off_selector = ++last_used_variable;
    variable_to_off_selector[v] = off_selector;
    second_part.push_back(Clause{-off_selector, -renaming[v]});
  }
  // Initialize SAT solvers.
  interpolating_solver = std::make_unique<ITPSolver>(first_part, second_part);
  fast_solver->appendFormula(first_part);
  fast_solver->appendFormula(second_part);
  backbone_solver->appendFormula(matrix);
}


DefinabilityChecker::DefinabilityChecker(DefinabilityChecker&& checker, int& last_used_variable, const Configuration& config) : 
      last_used_variable(last_used_variable), config(config), compress(checker.compress), 
      renaming(std::move(checker.renaming)),
      renaming_inverse(std::move(checker.renaming_inverse)),
      variable_to_equality_selector(std::move(checker.variable_to_equality_selector)),
      variable_to_on_selector(std::move(checker.variable_to_on_selector)),
      variable_to_off_selector(std::move(checker.variable_to_off_selector)),
      backbone_solver(std::move(checker.backbone_solver)),
      fast_solver(std::move(checker.fast_solver)),
      interpolating_solver(std::move(checker.interpolating_solver)) {  
}


void DefinabilityChecker::addVariable(int variable, bool shared) {
  auto variable_alias = ++last_used_variable;
  renaming[variable] = variable_alias;
  renaming_inverse[variable_alias] = variable;
  if (shared) {
    auto eq_selector = ++last_used_variable;
    variable_to_equality_selector[variable] = eq_selector;
    auto equality_clauses = clausalEncodingEquality(variable, variable_alias, eq_selector);
    fast_solver->appendFormula(equality_clauses);
    for (auto& clause: equality_clauses) {
      interpolating_solver->addClause(clause, false);
    }
  }
}

void DefinabilityChecker::addClause(Clause& clause) {
  if (!config.definitions) {
    return;
  }
  interpolating_solver->addClause(clause);
  fast_solver->addClause(clause);
  // Add variables to renaming if necessary.
  for (auto l: clause) {
    auto v = var(l);
    if (renaming.find(v) == renaming.end()) {
      renaming[v] = ++last_used_variable;
    }
  }
  auto renamed_clause = renameClause(clause, renaming);
  interpolating_solver->addClause(renamed_clause, false);
  fast_solver->addClause(renamed_clause);
  backbone_solver->addClause(clause);
}

std::tuple<bool, std::vector<Clause>, std::vector<int>> DefinabilityChecker::checkForced(int variable, const std::vector<int>& assumptions) {
  backbone_solver->assume(assumptions);
  backbone_solver->assume({ variable });
  auto solver_result = backbone_solver->solve();
  if (solver_result == 20) {
    auto core = backbone_solver->getFailed(assumptions);
    std::vector<Clause> definition{{-variable}};
    return std::make_tuple(true, definition, core);
  } else {
    assert(solver_result == 10);
    backbone_solver->assume(assumptions);
    backbone_solver->assume({ -variable });
    auto solver_result = backbone_solver->solve();
    if (solver_result == 20) {
      auto core = backbone_solver->getFailed(assumptions);
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
  negateEach(conflict);
  DLOG(trace) << "Conflict both parts: " << conflict << std::endl;
  auto conflict_original = renameClause(conflict, renaming_inverse);
  DLOG(trace) << "Original: " << conflict_original << std::endl;
  std::sort(conflict_original.begin(), conflict_original.end());
  std::sort(assumptions.begin(), assumptions.end());
  std::vector<int> intersection(conflict_original.size());
  auto it = std::set_intersection(conflict_original.begin(), conflict_original.end(), assumptions.begin(), assumptions.end(), intersection.begin());
  intersection.resize(it-intersection.begin());
  return intersection;
}

std::tuple<std::vector<Clause>,std::vector<std::tuple<std::vector<int>,int>>> DefinabilityChecker::getDefinitionInterpolant(const std::vector<int>& defining_variables, int defined_variable) {
  auto [definitions, _] = interpolating_solver->getDefinition(defining_variables, defined_variable, last_used_variable, compress);
  std::vector<Clause> clausal_encoding_definitions;
  for (auto& definition: definitions) {
    auto clauses_definition = clausalEncodingAND(definition);
    clausal_encoding_definitions.insert(clausal_encoding_definitions.end(), clauses_definition.begin(), clauses_definition.end());
  }
  return std::make_tuple(clausal_encoding_definitions, definitions);
}

std::tuple<bool, std::vector<int>> DefinabilityChecker::checkDefinability(const std::vector<int>& defining_variables, int defined_variable, const std::vector<int>& assumptions, int limit, bool minimize_assumptions) {
  DLOG(trace) << "Checking for definition of " << defined_variable << " in terms of " << defining_variables << std::endl;
  DLOG(trace) << "Assumptions: " << assumptions << std::endl;

  bool is_defined = checkDefinedInterpolate(defining_variables, defined_variable, assumptions, limit);
  if (!is_defined) {
    return std::make_tuple(false, std::vector<int>{} );
  }
  // There is a definition, extract the failed assumptions, and (possibly) minimize.
  auto failed_assumptions = getFailed(assumptions);
  if (minimize_assumptions) {
    std::unordered_set<int> defining_variables_set(defining_variables.begin(), defining_variables.end());
    // Try removing an assumption literal.
    for (int i = 0; i < failed_assumptions.size();) {
      int literal_removed = failed_assumptions[i];
      failed_assumptions[i] = failed_assumptions.back();
      failed_assumptions.pop_back();
      if ((defining_variables_set.find(var(literal_removed)) == defining_variables_set.end()) || !checkDefinedInterpolate(defining_variables, defined_variable, failed_assumptions, limit)) {
        // Put literal back into assumptions and advance index.
        failed_assumptions.push_back(failed_assumptions[i]);
        failed_assumptions[i] = literal_removed;
        i++;
      }
    }
  }
  std::sort(failed_assumptions.begin(), failed_assumptions.end());
  return std::make_tuple(true, failed_assumptions);
}

std::tuple<std::vector<Clause>, std::vector<std::tuple<std::vector<int>,int>>> DefinabilityChecker::getDefinition(const std::vector<int>& defining_variables, int defined_variable, const std::vector<int>& assumptions) {
  /* auto is_defined = checkDefined(defining_variables, defined_variable, assumptions, -1);
  if (!is_defined) {
    throw std::runtime_error("Definition extraction called when variable is undefined.");
  } */
  // The interpolating solver can be thrown off if variable or -variable are implied, so we deal with this case first.
  auto [is_forced, forced_definition, core] = checkForced(defined_variable, assumptions);
  if (is_forced) {
    std::vector<std::tuple<std::vector<int>,int>> cdef;
    cdef.push_back(std::make_tuple(std::vector<int>{},forced_definition[0][0]));
    return std::make_tuple(forced_definition, cdef);
  }
  // Call the "slow" interpolating solver and extract the definition from its proof.
  // bool itp_defined = checkDefinedInterpolate(defining_variables, defined_variable, assumptions, -1);
  // assert(itp_defined);
  try {
    auto [definition_clauses, definition_circuit] = getDefinitionInterpolant(defining_variables, defined_variable);
    last_used_variable = std::max(last_used_variable, maxVarIndex(definition_clauses));
    return std::make_tuple(definition_clauses, definition_circuit);
  }
  catch (const std::invalid_argument&) {
    // Handle errors occurring during interpolation by resetting the solver and trying again.
    std::cerr << "Invalid arguments during interpolation, resetting solver." << std::endl;
    interpolating_solver->resetSolver();
    return getDefinition(defining_variables, defined_variable, assumptions);
  }
  catch (const std::runtime_error&) {
    std::cerr << "BCP error during interpolation, resetting solver." << std::endl;
    interpolating_solver->resetSolver();
    checkDefinedInterpolate(defining_variables, defined_variable, assumptions, -1);
    return getDefinition(defining_variables, defined_variable, assumptions);
  }
}

bool DefinabilityChecker::checkDefined(const std::vector<int>& defining_variables, int defined_variable, const std::vector<int>& assumptions, int limit) {
  auto renamed_assumptions = renameClause(assumptions, renaming);
  fast_solver->assume(assumptions);
  fast_solver->assume(renamed_assumptions);
  std::vector<int> eq_selector_and_defined_assumptions;
  std::unordered_set<int> assumption_variables;
  for (auto l: assumptions) {
    assumption_variables.insert(var(l));
  }
  for (auto& variable: defining_variables) {
    if (assumption_variables.find(variable) == assumption_variables.end()) {
      try {
        eq_selector_and_defined_assumptions.push_back(variable_to_equality_selector.at(variable));
      }
      catch (const std::out_of_range&) {}
    }
  }
  eq_selector_and_defined_assumptions.push_back(variable_to_on_selector[defined_variable]);
  eq_selector_and_defined_assumptions.push_back(variable_to_off_selector[defined_variable]);
  fast_solver->assume(eq_selector_and_defined_assumptions);

  if (limit > -1) {
    return fast_solver->solve(limit) == 20;
  } else {
    return fast_solver->solve() == 20;
  }
}

bool DefinabilityChecker::checkDefinedInterpolate(const std::vector<int>& defining_variables, int defined_variable, const std::vector<int>& assumptions, int limit) {
  auto renamed_assumptions = renameClause(assumptions, renaming);
  std::vector<int> eq_selector_and_defined_assumptions;
  std::unordered_set<int> assumption_variables;
  for (auto l: assumptions) {
    assumption_variables.insert(var(l));
  }
  for (auto& variable: defining_variables) {
    if (assumption_variables.find(variable) == assumption_variables.end()) {
      try {
        eq_selector_and_defined_assumptions.push_back(variable_to_equality_selector.at(variable));
      }
      catch (const std::out_of_range&) {}
    }
  }
  eq_selector_and_defined_assumptions.push_back(variable_to_on_selector[defined_variable]);
  eq_selector_and_defined_assumptions.push_back(variable_to_off_selector[defined_variable]);

  std::vector<int> assumptions_all = assumptions;
  assumptions_all.insert(assumptions_all.end(), renamed_assumptions.begin(), renamed_assumptions.end());
  assumptions_all.insert(assumptions_all.end(), eq_selector_and_defined_assumptions.begin(), eq_selector_and_defined_assumptions.end());
  bool result = interpolating_solver->solve(assumptions_all, limit);
  return !result;
}

std::vector<int> DefinabilityChecker::getFailed(const std::vector<int>& assumptions) {
  auto renamed_assumptions = renameClause(assumptions, renaming);
  auto failed_assumptions = fast_solver->getFailed(assumptions);
  auto failed_renamed_assumptions = fast_solver->getFailed(renamed_assumptions);
  auto failed_renamed_original = renameClause(failed_renamed_assumptions, renaming_inverse);
  std::vector<int> assumptions_union;
  std::set_union(failed_assumptions.begin(), failed_assumptions.end(), failed_renamed_original.begin(), failed_renamed_original.end(), std::back_inserter(assumptions_union));
  sort(assumptions_union.begin(), assumptions_union.end() );
  assumptions_union.erase(std::unique(assumptions_union.begin(), assumptions_union.end()), assumptions_union.end());
  return assumptions_union;
}

}