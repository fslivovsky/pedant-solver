#include "preprocessor.h"
#include "utils.h"

namespace pedant {

Preprocessor::Preprocessor(std::vector<int>& universal_variables, std::unordered_map<int, std::vector<int>>& dependency_map, std::vector<Clause>& matrix): universal_variables(universal_variables), universal_variables_set(universal_variables.begin(), universal_variables.end()), dependency_map(dependency_map), matrix(matrix) {
  for (auto& [variable, dependencies]: dependency_map) {
    dependency_map_set[variable] = std::unordered_set<int>(dependencies.begin(), dependencies.end());
  }
}

int Preprocessor::applyForallReduction(Clause& clause) {
  Clause universal_clause = restrictTo(clause, universal_variables_set);
  Clause existential_clause = restrictTo(clause, dependency_map_set);
  clause = existential_clause;
  int reduced = 0;
  for (auto universal_literal: universal_clause) {
    auto universal_variable = var(universal_literal);
    bool is_dependency = false;
    for (auto existential_literal: existential_clause) {
      auto existential_variable = var(existential_literal);
      if (dependency_map_set[existential_variable].find(universal_variable) != dependency_map_set[existential_variable].end()) {
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

void Preprocessor::preprocess() {
  for (auto& clause: matrix) {
    applyForallReduction(clause);
  }
}

}