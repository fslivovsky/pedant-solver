#include "solver.h"

#include <algorithm>
#include <iostream>

#include <assert.h>

#include "logging.h"

namespace pedant {

Solver::Solver( std::vector<int>& universal_variables, std::unordered_map<int,
                std::vector<int>>& dependency_map, std::vector<Clause>& matrix, 
                Configuration& config):
                last_used_variable(0), universal_variables(universal_variables),
                universal_variables_set(universal_variables.begin(), universal_variables.end()), 
                dependency_map(dependency_map), matrix(matrix), 
                definabilitychecker(universal_variables, dependency_map, matrix, last_used_variable, false), 
                extended_dependency_map(ComputeExtendedDependencies(dependency_map)),
                validitychecker(universal_variables, extended_dependency_map, matrix, last_used_variable),
                // validitychecker(universal_variables, dependency_map, matrix, last_used_variable), 
                skolemcontainer(universal_variables, dependency_map, last_used_variable),
                use_random_defaults(false), config(config) {
  for (auto& [variable, dependencies]: dependency_map) {
    dependency_map_set[variable] = std::unordered_set<int>(dependencies.begin(), dependencies.end());
  }
  for (auto& [variable, extended_dependencies]: extended_dependency_map) {
    extended_dependency_map_set[variable] = std::unordered_set<int>(extended_dependencies.begin(), extended_dependencies.end());
  }
  existential_variables = getKeys(dependency_map);
  undefined_variables = std::set<int>(existential_variables.begin(), existential_variables.end());
  auto max_existential = existential_variables.empty() ? 0 : *std::max_element(existential_variables.begin(), existential_variables.end());
  auto max_universal = universal_variables.empty() ? 0 : *std::max_element(universal_variables.begin(), universal_variables.end());
  last_used_variable = std::max({ maxVarIndex(matrix), max_existential, max_universal, last_used_variable });
  arbiter_solver.appendFormula(matrix);
  forced_solver.appendFormula(matrix);
  extended_dependency_map = ComputeExtendedDependencies(dependency_map);
  skolemcontainer.initValidityCheckModel(validitychecker);
  unate_checker = std::make_unique<UnateChecker>(matrix, existential_variables, last_used_variable);
}

int Solver::solve() {
  try {
    int iteration = 0;
    checkDefined(undefined_variables, arbiter_assignment, true);
    checkUnates();
    while (true) {
      if (InterruptHandler::interrupted(nullptr)) {
        throw InterruptedException();
      }
      iteration++;
      if (iteration % 500 == 0) {
        use_random_defaults = !use_random_defaults;
        std::cerr << "Iteration: " << iteration << std::endl;
        std::vector<int> empty_assignment;
        checkDefined(variables_to_check, empty_assignment, true);
        setRandomDefaultValues(undefined_variables);
        variables_to_check.clear();
        //variables_recently_forced.clear();
      }
      if (checkArbiterAssignment()) {
        if (config.extract_cnf_model) {
          skolemcontainer.writeModelAsCNFToFile(arbiter_assignment,config.cnf_model_filename);
        }
        if (config.extract_aag_model) {
          skolemcontainer.writeModelAsAIGToFile(arbiter_assignment,config.aag_model_filename,false);
        }
        if (config.extract_aig_model) {
          skolemcontainer.writeModelAsAIGToFile(arbiter_assignment,config.aig_model_filename,true);
        }
        return 10;
      }
      solver_stats.conflicts++;
      auto [counterexample_universals, failed_existential_literals, failed_arbiter_literals] = validitychecker.getConflict();
      if (analyzeConflict(counterexample_universals, failed_existential_literals, failed_arbiter_literals)) {
        continue;
      }
      if (!findArbiterAssignment()) {
        return 20;
      }
    }
  } catch(InterruptedException) {
    return 0;
  }
}

bool Solver::checkArbiterAssignment() {
  return validitychecker.checkArbiterAssignment(arbiter_assignment, skolemcontainer);
}

bool Solver::findArbiterAssignment() {
  int return_value = arbiter_solver.solve();
  assert(return_value == 10 || return_value == 20);
  if (return_value == 10) {
    arbiter_assignment = arbiter_solver.getValues(arbiter_variables);
    DLOG(trace) << "New arbiter assignment: " << arbiter_assignment << std::endl;
    return true;
  } else {
    return false;
  }
}

std::tuple<bool, std::vector<int>> Solver::checkForced(int literal, std::vector<int>& assignment_fixed, std::vector<int>& assignment_variable) {
  auto v = var(literal);
  assignment_fixed.push_back(-literal);
  forced_solver.assume(assignment_fixed);
  auto assignment_existential_dependencies = restrictTo(assignment_variable, extended_dependency_map_set[v]);
  forced_solver.assume(assignment_existential_dependencies);
  bool is_forced = (forced_solver.solve() == 20);
  assignment_fixed.pop_back();
  if (is_forced) {
    auto core = forced_solver.getFailed(assignment_fixed);
    auto core_variable = forced_solver.getFailed(assignment_existential_dependencies);
    core.insert(core.end(), core_variable.begin(), core_variable.end()); // TODO: Return tuples?
    return std::make_tuple(true, core);
  } else {
    return std::make_tuple(false, std::vector<int>{});
  }
}

std::tuple<Clause, bool> Solver::getForcingClause(int literal, std::vector<int>& assignment) {
  DLOG(trace) << "Forcing assignment: " << assignment << std::endl;
  auto variable = var(literal);
  auto& variable_dependencies = extended_dependency_map_set[variable];
  bool reduced = false;
  for (int i = 0; i < assignment.size();) {
    auto l = assignment[i];
    auto v = var(l);
    if (arbiter_variables_set.find(v) == arbiter_variables_set.end() && variable_dependencies.find(v) == variable_dependencies.end()) {
      assignment[i] = assignment.back();
      assignment.back() = l;
      assignment.pop_back();
      reduced = true;
    } else {
      i++;
    }
  }
  negateEach(assignment);
  assignment.push_back(literal);
  return std::make_tuple(assignment, reduced);
}

void Solver::addForcingClause(Clause& forcing_clause, bool reduced) {
  solver_stats.forcing_clauses++;
  if (reduced) {
    definabilitychecker.addClause(forcing_clause);
    forced_solver.addClause(forcing_clause);
  }
  skolemcontainer.addForcingClause(forcing_clause, reduced, validitychecker);
  assert(validitychecker.checkForcedExtraction(forcing_clause));
}

void Solver::addDefinition(int variable, std::vector<Clause>& definition, const std::vector<std::tuple<std::vector<int>,int>>& definition_circuit, std::vector<int>& conflict) {
  if (conflict.empty()) {
    solver_stats.defined++;
  }
  skolemcontainer.addDefinition(variable, definition, definition_circuit, conflict, validitychecker);
}

std::tuple<bool, std::vector<Clause>, std::vector<std::tuple<std::vector<int>,int>>, std::vector<int>> Solver::getDefinition(int variable, std::vector<int>& assumptions, bool use_extended_dependencies) {
  std::vector<int> dependencies = use_extended_dependencies ? extended_dependency_map[variable] : dependency_map[variable];
  // TODO: Usually, either all arbiters are assigned or no arbiter is assigned by assumptions, so the following can be done quicker.
  std::set<int> unassigned_arbiters(arbiter_variables.begin(), arbiter_variables.end());
  for (auto& l: assumptions) {
    auto v = var(l);
    unassigned_arbiters.erase(v);
  }
  dependencies.insert(dependencies.end(), unassigned_arbiters.begin(), unassigned_arbiters.end());
  return definabilitychecker.checkDefinability(dependencies, variable, assumptions);
}

template<typename T> void Solver::checkDefined(T& variables_to_check, std::vector<int>& assumptions, bool use_extended_dependencies) {
  std::vector<int> found_defined;
  int checked = 0;
  for (auto variable: variables_to_check) {
    // auto [defined, definition,definition_circuit, conflict] = getDefinition(variable, assumptions, use_extended_dependencies);
    auto [defined, definition, definition_circuit, conflict] = getDefinition(variable, assumptions, use_extended_dependencies);
    if (defined) {
      DLOG(trace) << "Definition found for variable " << variable << std::endl;
      addDefinition(variable, definition, definition_circuit, conflict);
      found_defined.push_back(variable);
    }
    DLOG(info) << ++checked << "/" << variables_to_check.size() << " checked. \r";
  }
  if (checked) {
    DLOG(info) << found_defined.size() << "/" << variables_to_check.size() << " found defined." << std::endl;
  }
  for (auto variable: found_defined) {
    undefined_variables.erase(variable);
  }
}

int Solver::addArbiter(int literal, std::vector<int>& assignment) {
  solver_stats.arbiters_introduced++;
  auto variable = var(literal);
  auto [arbiter_variable, positive_clause, negative_clause] = createArbiter(variable, assignment);
  variables_to_check.insert(variable);
  arbiter_solver.addClause(positive_clause);
  arbiter_solver.addClause(negative_clause);
  forced_solver.addClause(positive_clause);
  forced_solver.addClause(negative_clause);
  validitychecker.addArbiterVariable(arbiter_variable);
  //validitychecker.addClauseConflictExtraction(positive_clause);
  //validitychecker.addClauseConflictExtraction(negative_clause);
  skolemcontainer.addForcingClause(positive_clause, true, validitychecker);
  skolemcontainer.addForcingClause(negative_clause, true, validitychecker);
  definabilitychecker.addSharedVariable(arbiter_variable);
  definabilitychecker.addClause(positive_clause);
  definabilitychecker.addClause(negative_clause);
  int arbiter_literal = renameLiteral(literal, arbiter_variable);
  arbiter_assignment.push_back(arbiter_literal);
  return arbiter_literal;
}

std::tuple<int, Clause, Clause> Solver::createArbiter(int variable, std::vector<int>& assignment) {
  int new_arbiter_variable = newVariable();
  arbiter_variables.push_back(new_arbiter_variable);
  arbiter_variables_set.insert(new_arbiter_variable);
  std::unordered_set<int> variable_dependencies(dependency_map[variable].begin(), dependency_map[variable].end()); // TODO: Save or use vector<bool>.
  Clause positive_clause = restrictTo(assignment, variable_dependencies);
  DLOG(trace) << "New arbiter " << new_arbiter_variable << " for " << variable << " and assignment " << positive_clause << std::endl;
  negateEach(positive_clause);
  Clause negative_clause = positive_clause;
  positive_clause.push_back(-new_arbiter_variable);
  positive_clause.push_back(variable);
  negative_clause.push_back(new_arbiter_variable);
  negative_clause.push_back(-variable);
  DLOG(trace) << "Positive clause: " << positive_clause << std::endl
              << "Negative clause: " << negative_clause << std::endl;
  return std::make_tuple(new_arbiter_variable, positive_clause, negative_clause);
}

bool Solver::analyzeConflict(std::vector<int>& counterexample_universals, std::vector<int>& failed_existential_literals, std::vector<int>& failed_arbiter_literals) {
  DLOG(trace) << "Failing arbiter assignment: " << failed_arbiter_literals << std::endl
              << "Failing existentials: " << failed_existential_literals << std::endl
              << "Universal counterexample: " << counterexample_universals << std::endl;
  std::vector<int> undecided_existential_literals;
  bool opposite_forced = false;
  auto counterexample_complete = counterexample_universals;
  counterexample_complete.insert(counterexample_complete.end(), arbiter_assignment.begin(), arbiter_assignment.end());
  for (auto l: failed_existential_literals) {
    auto v = var(l);
    // Check whether l is forced by the complete counterexample (arbiters + universals).
    auto [is_forced, forcing_assignment] = checkForced(l, counterexample_complete, failed_existential_literals);
    // If forced false, add arbiters in core to failed_arbiter_literals.
    if (is_forced) {
      auto forcing_assignment_arbiters = restrictTo(forcing_assignment, arbiter_variables_set);
      DLOG(trace) << l << " forced, forcing assignment: " << forcing_assignment << std::endl;
      failed_arbiter_literals.insert(failed_arbiter_literals.end(), forcing_assignment_arbiters.begin(), forcing_assignment_arbiters.end());
    } else {
      #ifdef USE_MACHINE_LEARNING
      if (use_random_defaults) {
        skolemcontainer.addTrainingExample(v, counterexample_universals, -l);
      }
      #endif
      // If -l is forced, add a forcing clause.
      auto [opposite_is_forced, opposite_forcing_assignment] = checkForced(-l, counterexample_complete, failed_existential_literals);
      if (opposite_is_forced) {
        variables_recently_forced.insert(v);
        opposite_forced = true;
        auto [forcing_clause, reduced] = getForcingClause(-l, opposite_forcing_assignment);
        DLOG(trace) << -l << " forced" << std::endl;
        DLOG(trace) << "Forcing clause: " << forcing_clause << std::endl;
        //assert(forcing_clause != last_forcing_clause);
        //last_forcing_clause = forcing_clause;
        addForcingClause(forcing_clause, reduced);
        if (reduced) {
          variables_to_check.insert(v);
        }
        // auto forcing_clause_universal = restrictTo(forcing_clause, universal_variables_set);
        // if (!forcing_clause_universal.empty()) {
        //   auto [defined, definition, conflict] = getDefinition(v, arbiter_assignment, true);
        //   if (defined && !conflict.empty()) {
        //     std::cerr << "Conditional definition of " << v << " found under conflict " << conflict << std::endl;
        //     addDefinition(v, definition, conflict);
        //   }
        // }
      } else {
        // Otherwise, we will later introduce an arbiter for this literal.
        undecided_existential_literals.push_back(l);
        DLOG(trace) << l << " undecided." << std::endl;
      }
    }
  }
  if (opposite_forced) {
    return true;
  }
  DLOG(trace) << "Undecided failing existentials: " << undecided_existential_literals << std::endl;
  // Create arbiter variables for undecided literals.
  for (auto l: undecided_existential_literals) {
    int arbiter_literal = addArbiter(l, counterexample_universals);
    failed_arbiter_literals.push_back(arbiter_literal);
  }
  // Remove duplicate literals.
  std::sort(failed_arbiter_literals.begin(), failed_arbiter_literals.end());
  failed_arbiter_literals.erase(std::unique(failed_arbiter_literals.begin(), failed_arbiter_literals.end()), failed_arbiter_literals.end());
  minimizeFailedArbiterAssignment(failed_arbiter_literals, counterexample_universals);
  // Add clause forbidding this assignment.
  negateEach(failed_arbiter_literals);
  DLOG(trace) << "Adding arbiter clause: " << failed_arbiter_literals << std::endl;
  arbiter_solver.addClause(failed_arbiter_literals);
  solver_stats.arbiter_clauses++;
  return false;
}

#ifdef USE_MACHINE_LEARNING

template<typename T> void Solver::setLearnedDefaultFunctions(T& variables_to_check) {
  for (int v: variables_to_check) {
    skolemcontainer.setLearnedDefaultFunction(v, validitychecker);
  }
}

#endif

template<typename T> void Solver::setRandomDefaultValues(T& variables_to_set) {
  for (int v: variables_to_set) {
    skolemcontainer.setRandomDefaultValue(v);
  }
}

void Solver::checkUnates() {
  std::vector<int> variables_to_consider(undefined_variables.begin(), undefined_variables.end());
  DLOG(info) << "Checking for unates" << std::endl;
  auto unate_clauses = unate_checker->findUnates(variables_to_consider, arbiter_assignment); // FS: We assume that the arbiter assignment is empty here.
  DLOG(info)<< "Detected " << unate_clauses.size() << " unate literals" << std::endl;
  solver_stats.unates += unate_clauses.size();
  for (auto& clause: unate_clauses) {
    assert(clause.size() == 1);
    int variable = var(clause.front());
    undefined_variables.erase(variable);
    DLOG(trace) << clause.front() << " is unate." << std::endl;
    std::vector<Clause> definition_clauses{ clause };
    std::vector<int> conflict{};
    addDefinition(variable, definition_clauses,{std::make_tuple(std::vector<int>(),clause[0])}, conflict);
    // Also add clauses to definability checker and conflict extraction in validity checker.
    definabilitychecker.addClause(clause);
    validitychecker.addClauseConflictExtraction(clause);
  }
}

void Solver::addArbitersForConstants() {
  for (auto variable: undefined_variables) {
    if (dependency_map[variable].empty()) {
      std::vector<int> empty;
      addArbiter(variable, empty);
    }
  }
}

void Solver::minimizeFailedArbiterAssignment(std::vector<int>& failed_arbiter_assignment, std::vector<int>& universal_counterexample) {
  forced_solver.assume(failed_arbiter_assignment);
  forced_solver.assume(universal_counterexample);
  int result = forced_solver.solve();
  assert(result == 20);
  auto failed_arbiter_core = forced_solver.getFailed(failed_arbiter_assignment);
  failed_arbiter_assignment = failed_arbiter_core;
}

void Solver::printStatistics() {
  DLOG(info) << "========== STATISTICS ==========" << std::endl;
  DLOG(info) << "Conflicts: " << solver_stats.conflicts << std::endl;
  DLOG(info) << "Definitions: " << solver_stats.defined << std::endl;
  DLOG(info) << "Arbiters: " << solver_stats.arbiters_introduced << std::endl;
  DLOG(info) << "Arbiter clauses: " << solver_stats.arbiter_clauses << std::endl;
  DLOG(info) << "Unates: " << solver_stats.unates << std::endl;
}

}