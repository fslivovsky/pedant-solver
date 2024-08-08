#include <algorithm>

#include "dependencyextractor.h"

#include "utils.h"


namespace pedant {

DependencyExtractor::DependencyExtractor(DQDIMACS& formula, const Configuration& config) :
     max_variable_in_matrix(formula.getMaxVar()),config(config), formula(formula) {
  bool ignore_innermost_existential_block = config.ignore_innermost_existentials && !formula.lastBlockType();
  existential_block_idx_end = ignore_innermost_existential_block ? formula.getNofExistentialBlocks() - 1 : formula.getNofExistentialBlocks();
}


void DependencyExtractor::computeDependencies() {
  auto& universal_variables = formula.getUniversals();

  if (universal_variables.empty()) { //There are no universal variables in the given formula
    auto all_existentials = formula.getAllExistentials();
    for (auto& e : all_existentials) {
      original_dependency_map[e] = {};
    }
    return;
  }

  const auto& existential_variables = formula.getExistentials();
  const auto& universal_blocks = formula.getUniversalBlocks();
  const auto& existential_blocks = formula.getExistentialBlocks();
  
  int universal_index = 0;
  if (formula.firstBlockType()) {
    std::sort(universal_variables.begin(), universal_variables.begin() + universal_blocks[1]);
    universal_index = 1;
  }

  for (int i = 0; i < existential_block_idx_end; i++) {
    int universal_block_index = universal_blocks[universal_index];
    for (auto it = existential_variables.begin() + existential_blocks[i]; it!=existential_variables.begin() + existential_blocks[i+1]; it++) {
      original_dependency_map.emplace(*it, std::vector<int>(universal_variables.begin(), universal_variables.begin() + universal_block_index));
    }
    if (universal_index<formula.getNofUniversalBlocks()) {
      std::sort(universal_variables.begin(), universal_variables.begin() + universal_blocks[universal_index + 1]);
      universal_index++;
    }
  }
  for (const auto& [var, deps] : formula.getExplicitDependencies()) {
    std::vector<int> sorted_dependencies(deps);
    original_dependency_map.emplace(var, std::move(sorted_dependencies));
  }
}


std::unordered_map<int, std::vector<int>> DependencyExtractor::computeExtendedDependencies() {
  std::unordered_map<int, std::vector<int>> extended_dependencies;
  auto& universal_variables = formula.getUniversals();

  if (universal_variables.empty()) { //There are no universal variables in the given formula
    auto all_existentials = formula.getAllExistentials();
    std::sort(all_existentials.begin(), all_existentials.end());
    std::vector<int> visited_existentials;
    visited_existentials.reserve(all_existentials.size());
    for (auto& e : all_existentials) {
      original_dependency_map[e] = {};
      extended_dependencies[e] = visited_existentials;
      visited_existentials.push_back(e);
    }
    return extended_dependencies;
  }

  auto& existential_variables = formula.getExistentials();
  const auto& universal_blocks = formula.getUniversalBlocks();
  const auto& existential_blocks = formula.getExistentialBlocks();

  int universal_index = 0;
  if (formula.firstBlockType()) {
    std::sort(universal_variables.begin(), universal_variables.begin() + universal_blocks[1]);
    universal_index = 1;
  }

  for (int i = 0; i < existential_block_idx_end; i++) {
    std::sort(existential_variables.begin() + existential_blocks[i], existential_variables.begin() + existential_blocks[i+1]);
    for (auto it = existential_variables.begin() + existential_blocks[i]; it!=existential_variables.begin() + existential_blocks[i+1]; it++) {
      auto e = *it;
      original_dependency_map.emplace(e, std::vector<int>(universal_variables.begin(), universal_variables.begin() + universal_blocks[universal_index]));
      extended_dependencies.emplace(e, std::vector<int>(universal_variables.begin(), universal_variables.begin() + universal_blocks[universal_index]));
      extended_dependencies.at(e).insert(extended_dependencies.at(e).end(), existential_variables.begin(), it);
    }
    if (universal_index<formula.getNofUniversalBlocks()) {
      std::sort(universal_variables.begin(), universal_variables.begin() + universal_blocks[universal_index + 1]);
      universal_index++;
    }
  }

  std::vector<int> processed;
  processed.reserve(formula.getExplicitDependencies().size());
  for (const auto& [var, deps] : formula.getExplicitDependencies()) {
    std::vector<int> sorted_dependencies(deps);
    std::sort(sorted_dependencies.begin(), sorted_dependencies.end());
    original_dependency_map.emplace(var, sorted_dependencies);
    extended_dependencies.emplace(var, std::move(sorted_dependencies));
    checkImplicitdependencies(var, extended_dependencies);
    checkExplicitDependencies(var, processed, extended_dependencies);
    processed.push_back(var);
  }
  return extended_dependencies;
}

void DependencyExtractor::checkImplicitdependencies(int var, std::unordered_map<int, std::vector<int>>& extended_dependencies) {
  const auto& var_deps = original_dependency_map.at(var);
  int msub_idx = getMaximalSubSet(var, 0, existential_block_idx_end);
  const auto& existential_blocks = formula.getExistentialBlocks();
  int max_subset_representative_idx = existential_blocks[msub_idx];
  int min_superset_representative = 0;
  int msup_start = 0;
  if (msub_idx != -1) {
    const auto& eblocks = formula.getExistentialBlocks();
    const auto& evars = formula.getExistentials();
    int representative = evars[eblocks[msub_idx]];
    const auto& deps = original_dependency_map.at(representative);
    if (var_deps.size() == deps.size()) {// dependencies included + same size -> same dependencies
      if (msub_idx != 0) {
        addImplicitToExplicit(var, msub_idx-1, extended_dependencies);
      } 
      if (msub_idx != formula.getExistentials().size() - 1) {
        addExplicitToImplicit(var, msub_idx+1, extended_dependencies);
      }
      auto it = std::lower_bound(evars.begin() + eblocks[msub_idx], evars.begin() + eblocks[msub_idx + 1], var);
      extended_dependencies.at(var).insert(extended_dependencies.at(var).end(), evars.begin() + eblocks[msub_idx], it);
      for (auto eit = it; eit != evars.begin() + eblocks[msub_idx + 1]; eit++) {
        extended_dependencies.at(*eit).push_back(var);
      }
      return;
    }
    addImplicitToExplicit(var, msub_idx, extended_dependencies);
    msup_start = msub_idx + 1;
  }
  int msup_idx = getMinimalSuperSet(var, msup_start, existential_block_idx_end);
  if (msup_idx != formula.getNofExistentialBlocks()) {
    addExplicitToImplicit(var, msup_idx, extended_dependencies);
  }
}

int DependencyExtractor::getMaximalSubSet(int var, int start_index, int end_index) {
  if (start_index == end_index) {
    return -1;
  }
  int mid_idx = (start_index + end_index) / 2;
  const auto& existential_variables = formula.getExistentials();
  const auto& existential_blocks = formula.getExistentialBlocks();
  const auto& var_deps = original_dependency_map.at(var);
  const auto& deps = original_dependency_map.at(existential_variables[existential_blocks[mid_idx]]);
  if (std::includes(var_deps.begin(), var_deps.end(), deps.begin(), deps.end())) {
    int x = getMaximalSubSet(var, mid_idx+1, end_index);
    return std::max(x, mid_idx);
  } else {
    return getMaximalSubSet(var, start_index, mid_idx);
  }
}

int DependencyExtractor::getMinimalSuperSet(int var, int start_index, int end_index) {
  if (start_index == end_index) {
    return formula.getNofExistentialBlocks();
  }
  int mid_idx = (start_index + end_index) / 2;
  const auto& existential_variables = formula.getExistentials();
  const auto& existential_blocks = formula.getExistentialBlocks();
  const auto& var_deps = original_dependency_map.at(var);
  const auto& deps = original_dependency_map.at(existential_variables[existential_blocks[mid_idx]]);
  if (std::includes(deps.begin(), deps.end(), var_deps.begin(), var_deps.end())) {
    int x = getMinimalSuperSet(var, start_index, mid_idx);
    return std::min(x, mid_idx);
  } else {
    return getMinimalSuperSet(var, mid_idx+1, end_index);
  }
}

void DependencyExtractor::addExplicitToImplicit(int var, int idx, std::unordered_map<int, std::vector<int>>& extended_dependencies) {
  int start_idx = formula.getExistentialBlocks()[idx];
  const auto& existential_variables = formula.getExistentials();
  for (auto it = existential_variables.begin()+start_idx; it!=existential_variables.end();it++) {
    extended_dependencies.at(*it).push_back(var);
  }
}

void DependencyExtractor::addImplicitToExplicit(int var, int idx, std::unordered_map<int, std::vector<int>>& extended_dependencies) {
  int stop_idx = formula.getExistentialBlocks()[idx + 1];
  const auto& existential_variables = formula.getExistentials();
  extended_dependencies.at(var).insert(extended_dependencies.at(var).end(), existential_variables.begin(), existential_variables.begin() + stop_idx);
}

void DependencyExtractor::checkExplicitDependencies(int var, const std::vector<int>& processed, std::unordered_map<int, std::vector<int>>& extended_dependencies) {
  const auto& var_deps = original_dependency_map.at(var);
  for (auto v : processed) {
    const auto& deps = original_dependency_map.at(v);
    if (std::includes(var_deps.begin(), var_deps.end(), deps.begin(), deps.end())) {
      if (var_deps.size() == deps.size()) {
        if (var < v) {
          extended_dependencies.at(v).push_back(var);
        } else {
          extended_dependencies.at(var).push_back(v);
        }
      } else {
        extended_dependencies.at(var).push_back(v);
      }
    } else if (std::includes(deps.begin(), deps.end(), var_deps.begin(), var_deps.end())) {
      extended_dependencies.at(v).push_back(var);
    }
  }
}

//Apply Dependency Scheme

std::unordered_map<int, std::vector<int>> DependencyExtractor::computeInverseDependencies() const {
  std::unordered_map<int, std::vector<int>> inverse_dependency_map;
  for (const auto& [key,value]: original_dependency_map) {
    for (int u:value) {
      inverse_dependency_map[u].push_back(key);
    }
  }
  return inverse_dependency_map;
}

std::unordered_map<int,std::vector<int>> DependencyExtractor::getContainingClauses() const {
  std::unordered_map<int,std::vector<int>> containing_clauses;
  const auto& matrix = formula.getMatrix();
  for (int i=0;i<matrix.size();i++) {
    for (int l:matrix[i]) {
      containing_clauses[l].push_back(i);
    }
  }
  return containing_clauses;
}

void DependencyExtractor::searchPaths(int literal_to_start,std::vector<bool>& toConsider,std::vector<bool>& marked, std::unordered_map<int, std::vector<int>>& containing_clauses) const {
  int to_consider_index=abs(literal_to_start)-1;
  int marked_index=(literal_to_start<0 ? -literal_to_start+max_variable_in_matrix : literal_to_start)-1;
  //We only have to consider the literal if the associated variable shall be considered and the literal was not already visited
  if (toConsider[to_consider_index]&&!marked[marked_index]) {
    marked[marked_index]=true;
    for (int clause_index : containing_clauses[-literal_to_start]) {
      for (int l:formula.getMatrix()[clause_index]) {
        //the next literal has to be associated to a different variable. As literal_to_start is already marked it suffices to exclude -literal_to_start.
        if (l!=-literal_to_start) {
          searchPaths(l,toConsider,marked,containing_clauses);   
        }
      }
    }
  }
}

void DependencyExtractor::setUpSearchVectors(int universal,std::vector<bool>& to_consider, std::vector<bool>& marked_positive, std::vector<bool>& marked_negative, const std::vector<int>& inverse_deps) const {
  for (int e:inverse_deps) {
    marked_positive[e-1]=false;
    marked_positive[e+max_variable_in_matrix-1]=false;
    marked_negative[e-1]=false;
    marked_negative[e+max_variable_in_matrix-1]=false;
    to_consider[e-1]=true;
  }
  //the innermost existentialblock is ignored and thus not part of the inverse dependencies
  if (existential_block_idx_end != formula.getNofExistentialBlocks()) {
    const auto& existentials = formula.getExistentials();
    for (auto it = existentials.begin() + formula.getExistentialBlocks()[existential_block_idx_end]; it != existentials.end(); it++) {
      auto e = *it;
      marked_positive[e-1]=false;
      marked_positive[e+max_variable_in_matrix-1]=false;
      marked_negative[e-1]=false;
      marked_negative[e+max_variable_in_matrix-1]=false;
      to_consider[e-1]=true;
    }
  }
  marked_negative[universal-1]=true;
  marked_positive[universal+max_variable_in_matrix-1]=true;
  to_consider[universal-1]=true;
}

/**
 * If suffices to reset to_consider, thus we do not reset the to_mark vectors
 **/
void DependencyExtractor::revertSearchVectors(int universal, std::vector<bool>& to_consider, const std::vector<int>& inverse_deps) const {
  for (int e:inverse_deps) {
    to_consider[e-1] = false;
  }
  //the innermost existentialblock is ignored and thus not part of the inverse dependencies
  if (existential_block_idx_end != formula.getNofExistentialBlocks()) {
    const auto& existentials = formula.getExistentials();
    for (auto it = existentials.begin() + formula.getExistentialBlocks()[existential_block_idx_end]; it != existentials.end(); it++) {
      auto e = *it;
      to_consider[e-1] = false;
    }
  }
  to_consider[universal-1] = false;
}


std::unordered_map<int, std::vector<int>> DependencyExtractor::applyDependencyScheme() const {
  std::unordered_map<int, std::vector<int>> inverse_dependency_map = computeInverseDependencies();
  std::unordered_map<int, std::vector<int>> containing_clauses = getContainingClauses();
  /*
    The subsequent vectors are not particulary efficient in terms of memory usage.
    If the variables in the given PCNF do not have consecutive "names" starting with 0,
    the vectors will contain unused entries.
    Moreover, effectively we only have to consider the existential variables, thus the entries that
    correspond to the universal variables are unused. Still this representation is used as:
    - a vector of booleans is a quite compact structure
    - access/updates are fast
  */
  std::vector<bool> marked_positive(2*max_variable_in_matrix,false);
  std::vector<bool> marked_negative(2*max_variable_in_matrix,false);
  std::vector<bool> toConsider(max_variable_in_matrix,false);
  std::unordered_map<int, std::vector<int>> pruned_dependencies;
  //if there are no dependencies then the map shall yield the empty vector
  for (const auto& [key,value]: original_dependency_map) {
    pruned_dependencies[key]={};
  } 
  for (int u:formula.getUniversals()) {
    setUpSearchVectors(u,toConsider,marked_positive,marked_negative,inverse_dependency_map[u]);
    searchPaths(u,toConsider,marked_positive,containing_clauses);
    searchPaths(-u,toConsider,marked_negative,containing_clauses);
    revertSearchVectors(u, toConsider, inverse_dependency_map[u]);
    for (int e:inverse_dependency_map[u]) {
      //There is a RRS path
      if (  (marked_positive[e-1] && marked_negative[e+max_variable_in_matrix-1]) ||
            (marked_positive[e+max_variable_in_matrix-1] && marked_negative[e-1]) ) {
        pruned_dependencies[e].push_back(u);
      }
    }
  }
  return pruned_dependencies;
}


std::unordered_map<int, std::vector<int>> DependencyExtractor::getDependencies() {
  computeDependencies();
  if (config.apply_dependency_schemes) {
    return applyDependencyScheme();
  } else {
    return original_dependency_map;
  }
}

std::pair<std::unordered_map<int, std::vector<int>>, std::unordered_map<int, std::vector<int>>> DependencyExtractor::getExtendedDependencies() {
  auto extended_deps = computeExtendedDependencies();
  if (config.apply_dependency_schemes) {
    return std::make_pair(applyDependencyScheme(), extended_deps);
  } else {
    return std::make_pair(original_dependency_map, extended_deps);
  }
}



}