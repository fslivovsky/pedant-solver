#include "solver.h"

#include <algorithm>
#include <iostream>

#include <assert.h>

#include "logging.h"

#include "solver_generator.h"


namespace pedant {



Solver::Solver( InputFormula& formula, Configuration& config) :
                last_used_variable(0), universal_variables(std::move(formula.universal_variables)),
                universal_variables_set(universal_variables.begin(), universal_variables.end()),
                existential_variables(std::move(formula.existential_variables)),
                undefined_variables(existential_variables.begin(), formula.innermost_existential_block_present ? existential_variables.begin() + formula.start_index_innnermost_existentials : existential_variables.end()),
                // undefined_variables(existential_variables.begin(), existential_variables.end()),
                dependencies(config, std::move(formula.dependencies), std::move(formula.extended_dependenices), undefined_variables, universal_variables_set),
                matrix(std::move(formula.matrix)), config(config), preprocessing_done(false), 
                definabilitychecker(universal_variables, existential_variables, matrix, last_used_variable, config, false), 
                variables_defined_by_universals(universal_variables.begin(), universal_variables.end()), 
                validitychecker(existential_variables, universal_variables, 
                  dependencies,
                  matrix, last_used_variable, skolemcontainer, config), 
                skolemcontainer(universal_variables, existential_variables,
                  dependencies, 
                  last_used_variable, validitychecker, config) {
  arbiter_solver = giveSolverInstance(config.arbiter_solver);
  if (formula.innermost_existential_block_present) {
    processInnermostExistentials(formula.start_index_innnermost_existentials, formula.end_index_innnermost_existentials);
  } 
  std::sort(existential_variables.begin(), existential_variables.end());
  std::sort(universal_variables.begin(), universal_variables.end());
  auto max_existential = existential_variables.empty() ? 0 : existential_variables.back();
  auto max_universal = universal_variables.empty() ? 0 : universal_variables.back();
  last_used_variable = std::max({ maxVarIndex(matrix), max_existential, max_universal, last_used_variable });
  skolemcontainer.initDefaultValues();
  for (auto variable : existential_variables) {
    arbiter_counts[variable] = 0;
  }
  if (config.check_for_unates) {
    unate_checker = std::make_unique<UnateChecker>(matrix, existential_variables, last_used_variable, config);
  }
}


int Solver::solve() {
  try {
    int iteration = 0;
    int unchecked_iterations = 0;
    
    if (config.definitions) {
      if (config.ignore_innnermost_existentials && !innermost_existentials.empty()) {
        //We do not want to look again for definitions for variables where we previously did not find definitions
        std::set<int> to_check1;
        std::set_difference(undefined_variables.begin(), undefined_variables.end(), innermost_existentials.begin(), innermost_existentials.end(), std::inserter(to_check1, to_check1.begin()));
        checkDefined(to_check1, arbiter_assignment, false, 1);
        std::set<int> to_check2;
        std::set_intersection(to_check1.begin(), to_check1.end(), undefined_variables.begin(), undefined_variables.end(), std::inserter(to_check2, to_check2.begin()));
        checkDefined(to_check2, arbiter_assignment, true, config.conflict_limit_definability_checker);
      } else {
        checkDefined(undefined_variables, arbiter_assignment, false, 1);
        checkDefined(undefined_variables, arbiter_assignment, true, config.conflict_limit_definability_checker);
      }
    }
    if (config.check_for_unates) {
      checkUnates();
    }
    if (config.check_for_fcs_matrix) {
      forcingClausesFromMatrix();
    }
    preprocessing_done = true;
    while (true) {
      if (InterruptHandler::interrupted(nullptr)) {
        throw InterruptedException();
      }
      iteration++;
      if (iteration % 500 == 0) {
        std::cerr << "Iteration: " << iteration << std::endl;
        std::cerr << "Recently forced: " << std::vector<int>(variables_recently_forced.begin(), variables_recently_forced.end()) << std::endl;
        variables_recently_forced.clear();
        setRandomDefaultValues();
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
      auto [failing_existential_assignment, failing_universal_assignment, failing_arbiter_assignment, complete_universal_assignment, complete_existential_assignment] = validitychecker.getConflict();
      if (analyzeConflict(failing_existential_assignment, failing_universal_assignment, failing_arbiter_assignment, complete_universal_assignment, complete_existential_assignment)) {
        unchecked_iterations++;
        if (unchecked_iterations % 600 == 0) {
          unchecked_iterations = 0;
        } else {
          continue;
        }
      }
      if (!findArbiterAssignment()) {
        return 20;
      }
    }
  } catch(InterruptedException) {
    return 0;
  }
}

void Solver::forcingClausesFromMatrix() {
  std::unordered_set<int> existential_variables_set(existential_variables.begin(), existential_variables.end());
  int found = 0;
  for (const Clause& cl : matrix) {
    DLOG(trace) << "Checking clause " << cl << " for forcing conflict." << std::endl;
    std::vector<int> existentials_in_clause;
    std::vector<int> universals_in_clause;
    for (int l:cl) {
      if (existential_variables_set.find(var(l)) != existential_variables_set.end()) {
        existentials_in_clause.push_back(l);
      } else {
        universals_in_clause.push_back(l);
      }
    }
    auto [has_forcing_clause, forced_literal] = hasForcingClause(existentials_in_clause);
    if (has_forcing_clause) {
      found++;
      skolemcontainer.setPolarity(var(forced_literal),forced_literal>0);
      negateEach(existentials_in_clause);
      negateEach(universals_in_clause);
      DLOG(trace) << "Failing existential assignment: " << existentials_in_clause << std::endl
                  << "Failing universal assignment: " << universals_in_clause << std::endl;
      analyzeForcingConflict(-forced_literal, existentials_in_clause, universals_in_clause);
    }
  }
  std::cerr << found << " out of " << matrix.size() << " clauses are forcing clauses. " << std::endl;
}

bool Solver::checkArbiterAssignment() {
  return validitychecker.checkArbiterAssignment(arbiter_assignment);
}

bool Solver::findArbiterAssignment() {
  int return_value = arbiter_solver->solve();
  assert(return_value == 10 || return_value == 20);
  if (return_value == 10) {
    arbiter_assignment = arbiter_solver->getValues(arbiter_variables);
    DLOG(trace) << "New arbiter assignment: " << arbiter_assignment << std::endl;
    return true;
  } else {
    return false;
  }
}

std::tuple<Clause, bool> Solver::getForcingClause(int literal, const std::vector<int>& failed_existentials, const std::vector<int>& failed_universals) {
  auto forced_variable = var(literal);
  auto forcing_clause = failed_existentials;
  forcing_clause.insert(forcing_clause.end(), failed_universals.begin(), failed_universals.end());
  forcing_clause = dependencies.restrictToExtendedDendencies(forcing_clause, forced_variable);//TODO: fix error forcing clauses is not sorted -> method is not applicable
  negateEach(forcing_clause);
  forcing_clause.push_back(literal);
  bool reduced = forcing_clause.size() < (failed_existentials.size() + failed_universals.size());
  return std::make_tuple(forcing_clause, reduced);
}

void Solver::addForcingClause(Clause& forcing_clause, bool reduced) {
  if (preprocessing_done) {
    solver_stats.forcing_clauses++;
    solver_stats.sum_forcing_clause_lengths += forcing_clause.size();
  }
  if (reduced) {
    variables_recently_forced.insert(var(forcing_clause.back()));
    definabilitychecker.addClause(forcing_clause);
    variables_to_check.insert(var(forcing_clause.back()));
  }
  skolemcontainer.addForcingClause(forcing_clause, reduced);
}

void Solver::addDefinition(int variable, std::vector<Clause>& definition, const std::vector<std::tuple<std::vector<int>,int>>& definition_circuit, std::vector<int>& conflict, bool reduced) {
  if (conflict.empty()) {
    solver_stats.defined++;
    undefined_variables.erase(variable);
  } else {
    solver_stats.conditional_definitions++;
  }
  skolemcontainer.addDefinition(variable, definition, definition_circuit, conflict, reduced);
  if (reduced) {
    // Add to definability checker.
    // Introduce auxiliary variable representing the conflict.
    auto definition_active = ++last_used_variable;
    auto activation_clause = conflict;
    negateEach(activation_clause);
    activation_clause.push_back(definition_active);
    definabilitychecker.addClause(activation_clause);
    for (auto& clause: definition) {
      clause.push_back(-definition_active);
      definabilitychecker.addClause(clause);
      clause.pop_back();
    }
  }
}

template<typename T> void Solver::checkDefined(T variables_to_check, const std::vector<int>& assumptions, bool use_extended_dependencies, int conflict_limit) {
  std::vector<int> found_defined;
  DLOG(trace) << "Checking with conflict limit " << conflict_limit << "." << std::endl;
  std::set<int> queue(variables_to_check.begin(), variables_to_check.end());
  int i = 0;
  while (!queue.empty() && i < config.incremental_definability_max_iterations) {
    i++;
    std::set<int> new_queue;
    for (auto variable: queue) {
      std::vector<int> dependency_vector (dependencies.getExtendedDependencies(variable));
      if (!config.extended_dependencies) {
        const auto& deps = dependencies.getDependencies(variable);
        dependency_vector.insert(dependency_vector.end(), deps.begin(), deps.end());
      }
      auto [defined, conflict] = definabilitychecker.checkDefinability(dependency_vector, variable, assumptions, conflict_limit + 1);
      if( defined) {
        std::unordered_set<int> dependencies_set(dependency_vector.begin(), dependency_vector.end());
        found_defined.push_back(variable);
        auto definition = getDefinition(variable, dependency_vector, conflict);
        if (config.dynamic_dependencies) {
          auto support = restrictTo(getSupport(conflict, definition), dependencies_set);
          std::set<int> support_set(support.begin(), support.end());
          DLOG(trace) << "Support: " << support << std::endl;
          new_queue.erase(variable);
          updateDynamicDependencies(variable, support_set, new_queue);
        }
      }
      std::cerr << "Iteration " << i << ": " << found_defined.size() << "/" << variables_to_check.size() << " found defined with conflict limit " << conflict_limit << " " << variable <<". \r";
    }
    queue = new_queue;
    dependencies.performUpdate();
  }
  std::cerr << found_defined.size() << "/" << variables_to_check.size() << " found defined with conflict limit " << conflict_limit << " in " << i << " iterations." << std::endl;
}

std::vector<Clause> Solver::getDefinition(int variable, const std::vector<int>& dependencies, std::vector<int>& conflict) {
  DLOG(trace) << "Definition found for variable " << variable << " under assignment " << conflict << std::endl;
  auto [definition, definition_circuit] = definabilitychecker.getDefinition(dependencies, variable, conflict);
  // Only necessary for definitions with additional universal variables
  // bool reduced = restrictDefinitionByConstant0(definition, dependency_map_set[variable], universal_variables_set);
  bool reduced = false;
  DLOG(trace) << "Definition size: " << definition.size() << " clauses." << std::endl;
  addDefinition(variable, definition, definition_circuit, conflict, reduced);
  return definition;
}


void Solver::updateDynamicDependencies(int variable, std::set<int>& support_set, std::set<int>& updated_variables) {
  dependencies.scheduleUpdate(variable, support_set, updated_variables);
}

std::tuple<int, bool> Solver::getArbiter(int existential_literal, const std::vector<int>& complete_universal_assignment, bool introduce_clauses) {
  auto existential_variable = var(existential_literal);
  auto assignment_dependencies = dependencies.restrictToDendencies(complete_universal_assignment, existential_variable);
  auto [arbiter, is_new, is_proper_arbiter] = skolemcontainer.getArbiter(existential_variable, assignment_dependencies, introduce_clauses);
  int arbiter_literal = renameLiteral(existential_literal, arbiter);
  if (is_new) {
    solver_stats.arbiters_introduced++;
    arbiter_counts[existential_variable]++;
    arbiter_to_index[arbiter] = arbiter_variables.size();
    arbiter_variables.push_back(arbiter);
    arbiter_to_existential[arbiter] = existential_variable;
    arbiter_assignment.push_back(arbiter_literal);
  }
  return std::make_tuple(arbiter_literal, is_new);
}


void Solver::analyzeForcingConflict(int forced_literal, const std::vector<int> failed_existentials, const std::vector<int>& failed_universals) {
  if (preprocessing_done) {
    solver_stats.linear_conflicts++;
  }

  auto [forcing_clause, reduced] = getForcingClause(-forced_literal, failed_existentials, failed_universals);
  auto forced_variable = var(forced_literal);
  bool add_forcing_clause = config.use_forcing_clauses;
  // Consider the definition under the conflicting assignment.
  if (config.conditional_definitions) {
    std::vector<int> conflicting_assignment = failed_universals;
    conflicting_assignment.insert(conflicting_assignment.end(), failed_existentials.begin(), failed_existentials.end());
    conflicting_assignment.erase(std::remove(conflicting_assignment.begin(), conflicting_assignment.end(), forced_literal), conflicting_assignment.end());
    std::vector<int> dependency_vector(dependencies.getExtendedDependencies(forced_variable));
    auto [defined, conflict] = definabilitychecker.checkDefinability(dependency_vector, forced_variable, conflicting_assignment, 0, true);
    auto conflict_reduced = dependencies.restrictToExtendedDendencies(conflict, forced_variable);
    bool reduced_is_smaller = (conflict_reduced.size() < conflict.size());
    // Decide whether to use the function or the forcing clause. Due to the conflict limit, "defined" can be false.
    if (defined && (conflict_reduced.size() < (forcing_clause.size() - config.def_limit))) {
      //add_forcing_clause = false; // FS: Allow both the definition and the forcing clause.
      DLOG(trace) << "Conditional definition of " << forced_variable << " found under conflict " << conflict_reduced << std::endl;
      auto [definition, definition_circuit] = definabilitychecker.getDefinition(dependency_vector, forced_variable, conflict);
      // DLOG(trace) << "Support: " << restrictTo(getSupport(conflict, definition), x) << std::endl;
      DLOG(trace) << "Support: " <<  dependencies.restrictToExtendedDendencies(getSupport(conflict, definition), forced_variable) << std::endl;
      addDefinition(forced_variable, definition, definition_circuit, conflict_reduced, reduced_is_smaller);
      skolemcontainer.setDefaultValueActive(forced_variable, false);
    }
  }
  if (add_forcing_clause) {
    DLOG(trace) << "Forcing clause: " << forcing_clause << std::endl;
    addForcingClause(forcing_clause, reduced);
    auto forced_variable = var(forced_literal);
    auto forced_literal_sign = (-forced_literal > 0);
    skolemcontainer.setDefaultValue(forced_variable, forced_literal_sign);
    skolemcontainer.setDefaultValueActive(forced_variable, true);
  }
}

bool Solver::analyzeConflict( const std::vector<int>& failed_existentials, const std::vector<int>& failed_universals, 
                              const std::vector<int>& failed_arbiters, const std::vector<int>& complete_universal_assignment,
                              const std::vector<int>& complete_existential_assignment) {
  DLOG(trace) << "Failing existential assignment: " << failed_existentials << std::endl
              << "Failing universal assignment: " << failed_universals << std::endl
              << "Failing arbiters assignment: " << failed_arbiters << std::endl;
  solver_stats.existential_conflict_literals += failed_existentials.size();
  solver_stats.universal_conflict_literals += failed_universals.size();
  solver_stats.arbiter_conflict_literals += failed_arbiters.size();
  auto [has_forcing_clause, forced_literal] = failed_arbiters.empty() ? hasForcingClause(failed_existentials) : std::make_tuple(false, 0);
  if (has_forcing_clause) {
    if (config.use_forcing_clauses || config.conditional_definitions) {
      analyzeForcingConflict(forced_literal, failed_existentials, failed_universals);
    }
    if (!config.always_add_arbiter_clause) {
      insertIntoDefaultContainer(-forced_literal,complete_universal_assignment,complete_existential_assignment);
      return has_forcing_clause;
    } else if (!config.add_samples_for_arbiters) {
      insertIntoDefaultContainer(-forced_literal,complete_universal_assignment,complete_existential_assignment);
    }
  }
  auto arbiter_clause = failed_arbiters;
  negateEach(arbiter_clause);
  for (int i = 0; i < failed_existentials.size(); i++) {
    auto l = failed_existentials[i];
    if (config.add_samples_for_arbiters) {
      insertIntoDefaultContainer(-l,complete_universal_assignment,complete_existential_assignment);
    }
    auto [arbiter_literal, is_new] = getArbiter(l, complete_universal_assignment, !has_forcing_clause);
    arbiter_clause.push_back(-arbiter_literal);
    skolemcontainer.setDefaultValue(var(l), arbiter_literal < 0);
  }
  DLOG(trace) << "Adding arbiter clause: " << arbiter_clause << std::endl;
  arbiter_solver->addClause(arbiter_clause);
  solver_stats.arbiter_clauses++;
  return has_forcing_clause;
}

void Solver::checkUnates() {
  std::vector<int> variables_to_consider(undefined_variables.begin(), undefined_variables.end());
  std:cerr << "Checking for unates" << std::endl;
  auto unate_clauses = unate_checker->findUnates(variables_to_consider, arbiter_assignment); // FS: We assume that the arbiter assignment is empty here.
  std::cerr << "Detected " << unate_clauses.size() << " unate literals" << std::endl;
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

std::tuple<bool, int> Solver::hasForcingClause(const std::vector<int>& existential_assignment) {
  if (existential_assignment.empty()) {
    return std::make_tuple(false, 0);
  }
  std::set<int> assigned_existentials;
  int max_existential = 0;
  for (int i = 0; i < existential_assignment.size(); i++) {
    auto l = existential_assignment[i];
    auto v = var(l);
    assigned_existentials.insert(v);
    if (max_existential == 0 || dependencies.largerExtendedDependencies(v, var(max_existential))) {
    // FOR TESTING with QBFs: if (max_existential_index == -1 || (v > max_existential)) {
      max_existential = l;
    }
  }
  // Remove max element.
  assigned_existentials.erase(var(max_existential));
  bool has_forcing_clause = dependencies.includedInExtendedDependencies(var(max_existential),assigned_existentials);
  return std::make_tuple(has_forcing_clause, max_existential);
}

void Solver::setRandomDefaultValues() {
  for (auto v: undefined_variables) {
    skolemcontainer.setRandomDefaultValue(v);
  }
}

void Solver::setDefaultValuesFromResponse(const std::vector<int>& universal_assignment, const std::vector<int>& arbiter_assignment) {
  auto existential_response = validitychecker.getExistentialResponse(universal_assignment, arbiter_assignment);
  for (auto l: existential_response) {
    skolemcontainer.setDefaultValue(var(l), l > 0);
  }
}

void Solver::insertIntoDefaultContainer(int forced_literal, const std::vector<int>& universal_assignment, const std::vector<int>& existential_assignment) {
  if (config.use_existentials_in_tree) {
    skolemcontainer.insertIntoDefaultContainer(-forced_literal,universal_assignment, existential_assignment);
  } else {
    skolemcontainer.insertIntoDefaultContainer(-forced_literal,universal_assignment);
  }
}


void Solver::processInnermostExistentials(int start_index_block, int end_index_block) {
  for (auto it = existential_variables.begin() + end_index_block; it != existential_variables.end(); it++) {
    undefined_variables.insert(*it);
  }
  std::vector<int> dependency_vector(universal_variables.begin(), universal_variables.end());
  innermost_existentials.insert(innermost_existentials.end(), existential_variables.begin() + start_index_block, existential_variables.begin() + end_index_block);
  if (config.extended_dependencies) {
    dependency_vector.insert(dependency_vector.end(), existential_variables.begin(), existential_variables.begin() + start_index_block);
    //the dependencies of all existential variables with explicit dependencies are contained in the dependencies of the existentials from the innermost block.
    //Note: if there is a variable with explicit dependencies that may depend on all universal variables then we get different extended dependencies as in the usual case.
    //No matter if the respective variable is smaller or larger than a variable in the innermost block the variable with the explicit dependencies is part of the extended dependencies
    //of the variable in the innermost block
    dependency_vector.insert(dependency_vector.end(), existential_variables.begin()+end_index_block, existential_variables.end());
  }

  std::unordered_set<int> dependency_set (dependency_vector.begin(), dependency_vector.end());
  std::set<int> dependency_set_2 (dependency_vector.begin(), dependency_vector.end());
  std::set<int> x;
  int definitions_found = 0;
  int nof_processed = 0;
  int nof_variables_to_check = end_index_block - start_index_block;

  std::set<int> to_preocess_set;
  for (auto variable : innermost_existentials) {
    bool defined = config.definitions;
    if (config.definitions) {
      auto [def, conflict] = definabilitychecker.checkDefinability(dependency_vector, variable, arbiter_assignment, config.conflict_limit_definability_checker + 1);
      defined = conflict.empty();
      defined = defined && def;
      if (defined) {
        definitions_found++;
        //Proceed similarly as in the method getDefinition
        DLOG(trace) << "Definition found for variable " << variable << std::endl;
        auto [definition, definition_circuit] = definabilitychecker.getDefinition(dependency_vector, variable, conflict);
        DLOG(trace) << "Definition size: " << definition.size() << " clauses." << std::endl;
        auto support = restrictTo(getSupport(conflict, definition), dependency_set);
        std::set<int> support_set(support.begin(), support.end());
        DLOG(trace) << "Support: " << support << std::endl;
        dependencies.scheduleUpdate(variable, support_set, to_preocess_set);
        addDefinition(variable, definition, definition_circuit, conflict, false);
      }
    }
    if (!defined) {
      undefined_variables.insert(variable);
      dependencies.setDependencies(variable, universal_variables);
      dependencies.setExtendedDependencies(variable, dependency_set_2);
    } else {
      dependencies.setDependencies(variable, universal_variables);
    }
    if (config.extended_dependencies) {
      dependency_vector.push_back(variable);
      dependency_set.insert(variable);
      dependency_set_2.insert(variable);
    }
    nof_processed++;
    std::cerr << "Processed: " << nof_processed << "out of " << nof_variables_to_check << " -- nof definitions: " << definitions_found << ".\r";
  }
  dependencies.performUpdate();
}


void Solver::printStatistics() {
  std::cerr << "========== STATISTICS ==========" << std::endl;
  std::cerr << "Conflicts: " << solver_stats.conflicts << std::endl;
  std::cerr << "Definitions: " << solver_stats.defined << std::endl;
  std::cerr << "Linear conflicts: " << solver_stats.linear_conflicts << std::endl;
  std::cerr << "Forcing clauses: " << solver_stats.forcing_clauses << std::endl;
  if (solver_stats.forcing_clauses) {
    std::cerr << "Average forcing clause length: " << double(solver_stats.sum_forcing_clause_lengths) / double(solver_stats.forcing_clauses)  << std::endl;
  }
  std::cerr << "Conditional definitions: " << solver_stats.conditional_definitions << std::endl;
  std::cerr << "Arbiters: " << solver_stats.arbiters_introduced << std::endl;
  std::cerr << "Arbiter clauses: " << solver_stats.arbiter_clauses << std::endl;
  std::cerr << "Unates: " << solver_stats.unates << std::endl;
  if (solver_stats.conflicts) {
    std::cerr << "Average existential conflict size: " << double(solver_stats.existential_conflict_literals)/double(solver_stats.conflicts) << std::endl;
    std::cerr << "Average universal conflict size: " << double(solver_stats.universal_conflict_literals)/double(solver_stats.conflicts) << std::endl;
    std::cerr << "Average arbiter conflict size: " << double(solver_stats.arbiter_conflict_literals)/double(solver_stats.conflicts) << std::endl;
  }

  auto [nof_learnt_default_clauses, nof_learnt_default_clauses_per_variable,nof_clause_learnt_by_sampling] = skolemcontainer.getDefaultStatistic();
  std::cerr << "Number of learned default clauses: " << nof_learnt_default_clauses <<std::endl;
  if (existential_variables.size()>0) {
    std::cerr << "Average number of learned default clauses: " <<  double(nof_learnt_default_clauses) / double(existential_variables.size()) << std::endl;
  }
  int min_nof_clauses = INT_MAX, max_nof_clauses = 0;
  for (auto& [var, nof_clauses] : nof_learnt_default_clauses_per_variable) {
    min_nof_clauses = nof_clauses < min_nof_clauses ? nof_clauses : min_nof_clauses;
    max_nof_clauses = nof_clauses > max_nof_clauses ? nof_clauses : max_nof_clauses;
  }
  std::cerr << "Minimal number of learned default clauses: " << min_nof_clauses << std::endl;
  std::cerr << "Maximal number of learned default clauses: " << max_nof_clauses << std::endl;

  if (config.use_sampling) {
    std::cerr << "Number of learned clauses through sampling: " << nof_clause_learnt_by_sampling << std::endl;
  }
}

}