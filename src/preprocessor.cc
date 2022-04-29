#include "preprocessor.h"
#include "utils.h"

namespace pedant {


Preprocessor::Preprocessor(DQDIMACS& formula, const Configuration& config) : formula(formula), config(config), 
    dependencies(formula, config) {
}


int Preprocessor::applyForallReduction(Clause& clause, const std::unordered_set<int>& universal_variables_set, const std::unordered_set<int>& innermost_existentials, 
    const std::unordered_map<int, std::unordered_set<int>>& dependency_map_set) { 
  if (!restrictTo(clause, innermost_existentials).empty()) {
    return 0;
  }
  Clause universal_clause = restrictTo(clause, universal_variables_set);
  Clause existential_clause = restrictTo(clause, dependency_map_set);
  clause = existential_clause;
  int reduced = 0;
  for (auto universal_literal: universal_clause) {
    auto universal_variable = var(universal_literal);
    bool is_dependency = false;
    for (auto existential_literal: existential_clause) {
      auto existential_variable = var(existential_literal);
      if (dependency_map_set.at(existential_variable).find(universal_variable) != dependency_map_set.at(existential_variable).end()) {
        is_dependency = true;
        break;
      }
    }
    if (is_dependency) {
      clause.push_back(universal_literal);
    } else {
      reduced++;
    }
  }
  return reduced;

}

InputFormula Preprocessor::preprocess() {
  InputFormula result;
  if (config.ignore_innermost_existentials && !formula.lastBlockType()) {
    //formula.getExistentialBlocks().size()-2 the start index of the last block
    result.start_index_innermost_existentials = formula.getExistentialBlocks()[formula.getExistentialBlocks().size()-2];
    result.end_index_innermost_existentials = formula.getExistentialBlocks()[formula.getExistentialBlocks().size()-1];
    result.innermost_existential_block_present = true;
  }

  if (config.extended_dependencies) {
    auto [deps, extended_deps] = dependencies.getExtendedDependencies();
    result.dependencies = std::move(deps);
    result.extended_dependenices = std::move(extended_deps);
    std::sort(formula.getExistentials().begin() + result.start_index_innermost_existentials, formula.getExistentials().begin() + result.end_index_innermost_existentials);
  } else {
    auto deps = dependencies.getDependencies();
    result.dependencies = deps;
    result.extended_dependenices = std::move(deps);
  }


  result.matrix = std::move(formula.getMatrix());
  result.universal_variables = std::move(formula.getUniversals());
  result.existential_variables = std::move(formula.getExistentials());

  size_t start_index_explicit_dependencies = result.existential_variables.size();
  for (const auto& [var, deps] : formula.getExplicitDependencies()) {
    result.existential_variables.push_back(var);
  }
  // It is not really necessary to sort these variables but we want to ensure that they have some fixed ordering
  std::sort(result.existential_variables.begin() + start_index_explicit_dependencies, result.existential_variables.end());

  result.definitions = formula.getDefinitionMap();

  if (config.apply_forall_reduction) {
    std::unordered_set<int> universal_set (result.universal_variables.begin(), result.universal_variables.end());
    std::unordered_set<int> innermost_existentials;
    if (config.ignore_innermost_existentials && !formula.lastBlockType()) {
      innermost_existentials.insert(result.existential_variables.begin() + result.start_index_innermost_existentials,
                                    result.existential_variables.begin() + result.end_index_innermost_existentials);
    } 
    std::unordered_map<int, std::unordered_set<int>> dependency_map_set;
    for (auto& [variable, dependencies] : result.dependencies) {
      dependency_map_set[variable] = std::unordered_set<int>(dependencies.begin(), dependencies.end());
    }
    for (auto& clause: result.matrix) {
      applyForallReduction(clause, universal_set, innermost_existentials,  dependency_map_set);
    }
  }
  
  result.setMaxVariable();
  return result;
}


}