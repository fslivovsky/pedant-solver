
#include "dqdimacs.h"
#include <algorithm>


namespace pedant {

DQDIMACS::DQDIMACS() : max_var(0), check_max_var(true) {
  existential_blocks.push_back(0);
  universal_blocks.push_back(0);
}

DQDIMACS::DQDIMACS(int max_var) : max_var(max_var), check_max_var(false) {
  existential_blocks.push_back(0);
  universal_blocks.push_back(0);
}


void DQDIMACS::addClause(const std::vector<int>& clause) {
  matrix.push_back(clause);
}

void DQDIMACS::addUniversalBlock(const std::vector<int>& variables) {
  if (variables.empty()) {
    return;
  }
  if (existential_variables.empty()) {
    first_block_type = true;
  }
  universal_variables.insert(universal_variables.end(), variables.begin(), variables.end());
  bool last_block_universal = lastBlockType();
  if (!last_block_universal || getNofUniversalBlocks()==0) {
    universal_blocks.push_back(universal_variables.size());
  } else {
    universal_blocks.back() = universal_variables.size();
  }

  if (check_max_var) {
    int x = *std::max_element(variables.begin(), variables.end());
    max_var = std::max(max_var, x);
  }

}

void DQDIMACS::addExistentialBlock(const std::vector<int>& variables) {
  if (variables.empty()) {
    return;
  }
  if (universal_variables.empty()) {
    first_block_type = false;
  }
  existential_variables.insert(existential_variables.end(), variables.begin(), variables.end());
  bool last_block_existential = !lastBlockType();
  if (!last_block_existential || getNofExistentialBlocks()==0) {
    existential_blocks.push_back(existential_variables.size());
  } else {
    existential_blocks.back() = existential_variables.size();
  }

  if (check_max_var) {
    int x = *std::max_element(variables.begin(), variables.end());
    max_var = std::max(max_var, x);
  }
}

void DQDIMACS::addExplicitDependencies(int var, const std::vector<int>& dependencies) {
  explicit_dependencies[var] = dependencies;
  if (check_max_var) {
    max_var = std::max(max_var, var);
  }
}

void DQDIMACS::addDefinition(int var, std::vector<std::vector<int>>& definition_clauses, std::vector<std::tuple<std::vector<int>,int>>& definition_circuit) {
  // definitions.emplace(var, definition_clauses, definition_circuit);
  definitions[var] = std::tuple(definition_clauses, definition_circuit);
}

bool DQDIMACS::isDefined(int var) const {
  return definitions.find(var) != definitions.end();
}

std::tuple<std::vector<std::vector<int>>, std::vector<std::tuple<std::vector<int>,int>>> DQDIMACS::getDefinition(int var) {
  return definitions.at(var);
}

std::unordered_map<int, std::tuple<std::vector<std::vector<int>>, std::vector<std::tuple<std::vector<int>,int>>>>& DQDIMACS::getDefinitionMap() {
  return definitions;
}

int DQDIMACS::getMaxVar() const {
  return max_var;
}

bool DQDIMACS::firstBlockType() const {
  return first_block_type;
}

bool DQDIMACS::lastBlockType() const {
  if (existential_blocks.size()==universal_blocks.size()) {
    return !first_block_type;
  } else {
    return first_block_type;
  }
}


int DQDIMACS::getNofUniversalBlocks() const {
  return universal_blocks.size() - 1;
}

int DQDIMACS::getNofExistentialBlocks() const {
  return existential_blocks.size() - 1;
}

std::vector<std::vector<int>>& DQDIMACS::getMatrix() {
  return matrix;
}

std::vector<int>& DQDIMACS::getUniversals() {
  return universal_variables;
}

std::vector<int>& DQDIMACS::getExistentials() {
  return existential_variables;
}

std::vector<int> DQDIMACS::getAllExistentials() {
  std::vector<int> result(existential_variables);
  for (const auto& [var, deps] : explicit_dependencies) {
    result.push_back(var);
  }
  return result;
}

std::vector<int>& DQDIMACS::getUniversalBlocks() {
  return universal_blocks;
}

std::vector<int>& DQDIMACS::getExistentialBlocks() {
  return existential_blocks;
}

std::unordered_map<int, std::vector<int>>& DQDIMACS::getExplicitDependencies() {
  return explicit_dependencies;
}



}