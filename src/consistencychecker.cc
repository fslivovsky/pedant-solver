#include "consistencychecker.h"

#include <map>

#include <assert.h>

#include "utils.h"

#include "solver_generator.h"

namespace pedant {

ConsistencyChecker::ConsistencyChecker( const DependencyContainer& dependencies, const std::vector<int>& existential_variables,
                                        const std::vector<int>& universal_variables, int& last_used_variable, SolverData& shared_data,  const Configuration& config) : 
                                        existential_variables_set(existential_variables.begin(), existential_variables.end()), universal_variables(universal_variables), 
                                        last_used_variable(last_used_variable), shared_data(shared_data), config(config), 
                                        supporttracker(universal_variables, dependencies, last_used_variable, shared_data, config), dependencies(dependencies) {
  consistency_solver = giveSolverInstance(config.consistency_solver);
  supporttracker.setSatSolver(consistency_solver);
  // Create ordered map to ensure deterministic order of iteration.
  for (auto variable: existential_variables) {
    initVariableData(variable);
    // Introduce variable representing an inconsistency in the model of "variable".
    auto conflict_variable = ++last_used_variable;
    conflict_variable_to_variable[conflict_variable] = variable;
    conflict_variables.push_back(conflict_variable);
    DLOG(trace) << "Conflict variable for " << variable << ": " << conflict_variable << std::endl;
    // Set default function selector to undefined (-1).
    variable_to_default_function_selector_index[variable] = -1;
  }
  initEncoding();
}

void ConsistencyChecker::initVariableData(int variable) {
  variable_data.emplace(variable, VariableData(last_used_variable, disjunction_terminals));
  auto& this_variable_data = variable_data.at(variable);
  // Create new variables representing the two polarities.
  this_variable_data.literal_variables[false] = ++last_used_variable;
  this_variable_data.literal_variables[true] = ++last_used_variable;
  literal_variable_to_variable[this_variable_data.literal_variables[false]] = variable;
  literal_variable_to_variable[this_variable_data.literal_variables[true]] = variable;
  literal_variable_to_literal[this_variable_data.literal_variables[false]] = -variable;
  literal_variable_to_literal[this_variable_data.literal_variables[true]] = variable;
  DLOG(trace) << "Variables representing " << variable << ": " << this_variable_data.literal_variables[true] << ", " << this_variable_data.literal_variables[false] << std::endl;
  this_variable_data.no_forcing_clause_active = ++last_used_variable;
  DLOG(trace) << "No forcing active: " << this_variable_data.no_forcing_clause_active << std::endl;
  // A (configurable) default value.
  this_variable_data.default_value_index = default_assumptions.size();
  auto default_value_variable = ++last_used_variable;
  default_assumptions.push_back(-default_value_variable);
  // A flag indicating whether to use the default value (so as to use a default function instead).
  this_variable_data.use_default_value_index = default_assumptions.size();
  auto use_default_value_variable = ++last_used_variable;
  default_assumptions.push_back(use_default_value_variable);
  DLOG(trace) << "Variable " << variable << ", default value variable: " << default_value_variable << ", use default variable: " << use_default_value_variable << std::endl;
  // Set variables in support tracker.
  supporttracker.setNoForcedVariable(this_variable_data.literal_variables[false], this_variable_data.no_forcing_clause_active);
  supporttracker.setNoForcedVariable(this_variable_data.literal_variables[true], this_variable_data.no_forcing_clause_active);
  supporttracker.setAlias(this_variable_data.literal_variables[false], -variable);
  supporttracker.setAlias(this_variable_data.literal_variables[true], variable);
}

void ConsistencyChecker::initEncoding() {
  consistency_solver->addClause(conflict_variables); // A conflict variable must be true;
  for (auto conflict_variable: conflict_variables) {
    auto variable = conflict_variable_to_variable[conflict_variable];
    VariableData& vd = variable_data.at(variable);
    // At least one polarity must be set.
    consistency_solver->addClause( {vd.literal_variables[true], vd.literal_variables[false]} );
    // The conflict variable may be true only if the variables for both polarities are true;
    consistency_solver->addClause( {vd.literal_variables[true], -conflict_variable} );
    consistency_solver->addClause( {vd.literal_variables[false], -conflict_variable} );
    // If both literal variables are true, a conflict must occur.
    consistency_solver->addClause( {-vd.literal_variables[false], -vd.literal_variables[true], conflict_variable} );
    
    auto positive_default_fires = ++last_used_variable;
    consistency_solver->addClause(vd.default_disjunctions[true].createDisjunct({ -positive_default_fires}));
    auto negative_default_fires = ++last_used_variable;
    consistency_solver->addClause(vd.default_disjunctions[false].createDisjunct({ -negative_default_fires}));
    default_fires[conflict_variable] = positive_default_fires;
    default_fires[-conflict_variable] = negative_default_fires;

    auto no_forcing_clause_active = vd.no_forcing_clause_active;
    consistency_solver->addClause( { -positive_default_fires, no_forcing_clause_active });
    consistency_solver->addClause( { -negative_default_fires, no_forcing_clause_active });

    auto use_default_value_variable = var(default_assumptions[vd.use_default_value_index]);
    consistency_solver->addClause( { -positive_default_fires, use_default_value_variable });
    consistency_solver->addClause( { -negative_default_fires, use_default_value_variable });

    consistency_solver->addClause(vd.literal_disjunctions[true].createDisjunct({ positive_default_fires, -vd.literal_variables[true] }));
    consistency_solver->addClause(vd.literal_disjunctions[false].createDisjunct({ negative_default_fires, -vd.literal_variables[false] }));

  }
}

void ConsistencyChecker::addAssumption(const std::vector<int>& assumptions) {
  consistency_solver->assume(assumptions);
}



bool ConsistencyChecker::checkConsistency(const std::vector<int>& arbiter_assumptions, std::vector<int>& existential_counterexample, 
    std::vector<int>& universal_counterexample, std::vector<int>& arbiter_counterexample, std::vector<int>& complete_universal_counterexample) {
  consistency_solver->assume(disjunction_terminals);
  consistency_solver->assume(arbiter_assumptions);
  consistency_solver->assume(default_assumptions);
  consistency_solver->assume(default_function_selectors);
  auto result = consistency_solver->solve();

  bool consistent = (result == 20);
  if (!consistent) {
    #ifndef NDEBUG //If Loggig is disabled we do not need to compute these assignments
      DLOG(trace) << "Model of inconsistency check: " << consistency_solver->getModel() << std::endl;
      universal_counterexample = consistency_solver->getValues(universal_variables);
      DLOG(trace) << "Universal counterexample: " << universal_counterexample << std::endl;
    #endif
    auto conflicted_variable = getInconsistentExistentialVariable();

    if (config.sup_strat == Core) {
      #ifdef NDEBUG
        universal_counterexample = consistency_solver->getValues(universal_variables);
      #endif
      arbiter_counterexample = consistency_solver->getValues(arbiter_assumptions);
      complete_universal_counterexample = universal_counterexample;
      auto existential_dependencies = dependencies.getExistentialDependencies(conflicted_variable);//not very efficient

      std::for_each(existential_dependencies.begin(), existential_dependencies.end(), [this](int &variable){ 
        auto& v_data = variable_data.at(variable);
        variable = v_data.literal_variables[true];
      });
      auto existential_counterexample_translated = consistency_solver->getValues(existential_dependencies);
      std::for_each(existential_counterexample_translated.begin(), existential_counterexample_translated.end(), 
        [this](int &lit) { 
          int variable = lit > 0 ? lit : -lit - 1;
          lit = literal_variable_to_literal[variable];
      });
      existential_counterexample.clear();
      std::copy_if (existential_counterexample_translated.begin(), existential_counterexample_translated.end(), std::back_inserter(existential_counterexample), 
        [this](int lit){return dependencies.isUndefined(var(lit));} );
      return consistent;
    }

    int forced_variable = supporttracker.getForcedSource({ conflicted_variable, -conflicted_variable }, true);
    auto [existential_support, universal_support, arbiter_support] = supporttracker.computeSupport({ conflicted_variable, -conflicted_variable }, forced_variable, true);

    DLOG(trace) << "Existential support: " << existential_support << std::endl;
    DLOG(trace) << "Universal support: " << universal_support << std::endl;
    DLOG(trace) << "Arbiter support: " << arbiter_support << std::endl;

    existential_counterexample.clear();
    for (auto l: consistency_solver->getValues(existential_support)) {
      assert(l > 0);
      auto original_literal = literal_variable_to_literal[l];
      existential_counterexample.push_back(original_literal);
    }
    universal_counterexample = consistency_solver->getValues(universal_support);
    arbiter_counterexample = consistency_solver->getValues(arbiter_support);
    complete_universal_counterexample = consistency_solver->getValues(universal_variables);
  } else {
    DLOG(trace) << "Consistency check passed." << std::endl;
    DLOG(trace) << "Failed disjunction terminals: " << consistency_solver->getFailed(disjunction_terminals) << std::endl;
    DLOG(trace) << "Failed arbiter assumptions: " << consistency_solver->getFailed(arbiter_assumptions) << std::endl;
    DLOG(trace) << "Failed default assumptions: " << consistency_solver->getFailed(default_assumptions) << std::endl;
    DLOG(trace) << "Failed default function selectors: " << consistency_solver->getFailed(default_function_selectors) << std::endl;
  }
  return consistent;
}

void ConsistencyChecker::addForcingClause(const Clause& clause) {
  DLOG(trace) << "Adding defining clause " << clause << std::endl;
  auto clause_copy = clause;
  auto defined_literal = clause_copy.back(); // We assume that the final literal is the one that is set.
  bool sign = (defined_literal > 0);
  auto defined_variable = var(defined_literal);
  clause_copy.pop_back();
  negateEach(clause_copy); // Literals must be negated to get the right translated clause, then negated again.
  auto translated_clause = translateClause(clause_copy);
  // Introduce new variable that can be true only if the defining clause "fires".
  auto clause_active = ++last_used_variable;
  DLOG(trace) << "[Consistencychecker] Clause active variable: " << clause_active << std::endl;
  // The clause can be set to "active" only if each of its literals is true.
  for (auto& l: translated_clause) {
    consistency_solver->addClause({ -clause_active, l });
  }
  auto& vd = variable_data.at(defined_variable);
  consistency_solver->addClause({ -clause_active, -vd.no_forcing_clause_active });
  negateEach(translated_clause);
  translated_clause.push_back(clause_active);
  // If the premise consisting of translated literals is satisfied, the clause must be active.
  consistency_solver->addClause(translated_clause);
  DLOG(trace) << "[Consistencychecker] Translated defining clause: " << translated_clause << std::endl;
  // Force the variable representing the defined literal to be set to true whenever this clause is active.
  consistency_solver->addClause({ -clause_active, vd.literal_variables[sign] });
  // Add a disjunct allowing the solver to set the variable representing "defined_literal" to true if the clause is active.
  Clause new_disjunct;
  new_disjunct = vd.literal_disjunctions[sign].createDisjunct({ clause_active });
  DLOG(trace) << "New disjunct for setting literal variables: " << new_disjunct << std::endl;
  consistency_solver->addClause(new_disjunct);
  supporttracker.addDefiningClause(vd.literal_variables[sign], translated_clause, clause_active);
}

void ConsistencyChecker::addDefinition(int variable, const std::vector<Clause>& definition, const std::vector<int>& conflict) {
  auto conflict_active = ++last_used_variable;
  // This variable may only be set to true if all literals in conflict are falsified. We add binary clauses to enforce that.
  auto conflict_translated = translateClause(conflict);
  for (auto& l: conflict_translated) {
    consistency_solver->addClause({ -conflict_active, l });
  }
  // If all conflict literals are falsified, the conflict is active.
  negateEach(conflict_translated);
  conflict_translated.push_back(conflict_active);
  consistency_solver->addClause(conflict_translated);
  // Default function modifications.
  // Create a new selector that is true if, and only if, 
  // 1) the conflict is active, or
  // 2) a new default function selector is set.
  int selector = ++last_used_variable;
  int use_new_default_function_selector = ++last_used_variable;
  consistency_solver->addClause({ conflict_active, use_new_default_function_selector, -selector });
  consistency_solver->addClause({-conflict_active, selector });
  consistency_solver->addClause({-use_new_default_function_selector, selector });
  // Now specificy when use_new_default_function_selector is set to true.
  // This is the case if 1) no forcing clause is active, 2) no arbiter clause is active, and 3) a new selector for using the default function is set.
  int new_default_function_selector = newDefaultFunctionSelector(variable);
  auto& vd = variable_data.at(variable);
  consistency_solver->addClause({-use_new_default_function_selector, vd.no_forcing_clause_active });
  consistency_solver->addClause({-use_new_default_function_selector, new_default_function_selector });
  consistency_solver->addClause({-vd.no_forcing_clause_active, -new_default_function_selector, use_new_default_function_selector });
  auto [active_and_circuit_true, active_and_circuit_false] = addDefinitionSelector(variable, definition, selector);
  consistency_solver->addClause({ -conflict_active, -vd.no_forcing_clause_active });
  // Add definition in support tracker.
  auto definition_support = getTranslatedDefinitionSupport(variable, conflict, definition);
  supporttracker.addDefiningClause(vd.literal_variables[true], definition_support, active_and_circuit_true);
  supporttracker.addDefiningClause(vd.literal_variables[false], definition_support, active_and_circuit_false);
}

std::tuple<int, int> ConsistencyChecker::addDefinitionSelector(int variable, const std::vector<Clause>& definition, int selector) {
  // Rename output variable.
  auto output_variable_renamed = ++last_used_variable;
  std::unordered_map<int, int> renaming{{variable, output_variable_renamed}};
  auto definition_renamed = renameFormula(definition, renaming);
  // Add definition to solver.
  for (auto& clause: definition_renamed) {
    auto clause_translated = translateClause(clause);
    consistency_solver->addClause(clause_translated);
  }
  // Introduce new variables that may be set to true only if 1) conflict_active is true and 2) variable_renamed is true/false, respectively.
  auto active_and_circuit_true = ++last_used_variable;
  consistency_solver->addClause({ selector, -active_and_circuit_true });
  consistency_solver->addClause({ output_variable_renamed, -active_and_circuit_true });
  consistency_solver->addClause({ -selector, -output_variable_renamed, active_and_circuit_true });
  auto active_and_circuit_false = ++last_used_variable;
  consistency_solver->addClause({ selector, -active_and_circuit_false });
  consistency_solver->addClause({ -output_variable_renamed, -active_and_circuit_false });
  consistency_solver->addClause({ -selector, output_variable_renamed, active_and_circuit_false });
  // Add these variables to the disjunctions for making a variable true or false.
  auto& vd = variable_data.at(variable);
  consistency_solver->addClause(vd.literal_disjunctions[true].createDisjunct( {active_and_circuit_true} ));
  consistency_solver->addClause(vd.literal_disjunctions[false].createDisjunct( {active_and_circuit_false} ));
  // Force the variable representing the output to be set to true whenever this definition is active.
  consistency_solver->addClause({ -active_and_circuit_true, vd.literal_variables[true] });
  consistency_solver->addClause({ -active_and_circuit_false, vd.literal_variables[false] });
  return std::make_tuple(active_and_circuit_true, active_and_circuit_false);
}

Clause ConsistencyChecker::translateClause(const Clause& clause) {
  Clause translated_clause;
  for (auto l: clause) {
    auto v = var(l);
    if (existential_variables_set.find(v) != existential_variables_set.end()) {
      auto& v_data = variable_data.at(v);
      bool polarity = (l > 0);
      translated_clause.push_back(v_data.literal_variables[polarity]);
    } else {
      translated_clause.push_back(l);
    }
  }
  return translated_clause;
}

int ConsistencyChecker::getInconsistentExistentialVariable() {
  std::vector<int> conflicted_variables;
  // Determine existential variables that are conflicted.
  for (auto l: consistency_solver->getValues(conflict_variables)) {
    if (l > 0) {
      conflicted_variables.push_back(conflict_variable_to_variable[l]);
    }
  }
  assert(!conflicted_variables.empty());
  // Search for a conflicted variable whose existential dependencies are unconflicted.
  int conflicted_variable;
  do {
    // We (arbitrarily) choose the first conflicted variable here and restrict the conflicts to its (existential) dependencies.
    conflicted_variable = conflicted_variables.front();
    // auto& existential_dependencies = extended_dependency_map.at(conflicted_variable);
    // conflicted_variables = restrictTo(conflicted_variables, existential_dependencies);
    conflicted_variables = dependencies.restrictToExtendedDendencies(conflicted_variables, conflicted_variable);//TODO: check config extended deps
  } while (!conflicted_variables.empty());
  // Variable "conflicted_variable" only depends on unconflicted existential variables.
  DLOG(trace) << "Minimally conflicted variable: " << conflicted_variable << std::endl;
  return conflicted_variable;
}

int ConsistencyChecker::newDefaultFunctionSelector(int existential_variable) {
  if (variable_to_default_function_selector_index[existential_variable] != -1) {
     disableDefaultFunctionSelector(existential_variable);
  }
  variable_to_default_function_selector_index[existential_variable] = default_function_selectors.size();
  auto selector = ++last_used_variable;
  default_function_selectors.push_back(-selector);
  return selector;
}

std::vector<int> ConsistencyChecker::getTranslatedDefinitionSupport(int variable, const Clause& conflict, const std::vector<Clause>& definition) {
  // auto conflict_restricted = translateClause(restrictTo(conflict, extended_dependency_map.at(variable)));
  auto conflict_restricted = dependencies.restrictToExtendedDendencies(conflict, variable);
  std::transform(conflict_restricted.begin(), conflict_restricted.end(), conflict_restricted.begin(), [] (int literal) { return var(literal); });
  std::set<int> definition_literals(conflict_restricted.begin(), conflict_restricted.end());
  
  for (const auto& clause: definition) {
    auto clause_restricted = translateClause(dependencies.restrictToExtendedDendencies(clause, variable));
    // auto clause_restricted = translateClause(restrictTo(clause, extended_dependency_map.at(variable)));
    std::transform(clause_restricted.begin(), clause_restricted.end(), clause_restricted.begin(), [] (int literal) { return var(literal); });
    definition_literals.insert(clause_restricted.begin(), clause_restricted.end());
  }
  return std::vector<int>(definition_literals.begin(), definition_literals.end());
}

void ConsistencyChecker::addDefaultClause(const Clause& premise,int label) {
  Clause cl = premise;
  negateEach(cl);
  cl = translateClause(cl);
  int clause_active = ++last_used_variable;
  for (int l:cl) {
    consistency_solver->addClause({l, -clause_active});
  }
  // negateEach(cl);
  // cl.push_back(clause_active);
  // consistency_solver->addClause(cl);//really necessary?
  auto variable = var(label);
  auto& vd = variable_data.at(variable);
  consistency_solver->addClause(vd.default_disjunctions[label>0].createDisjunct({clause_active}));
}



}