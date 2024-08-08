#include <cassert>
#include <algorithm>

#include "dependencycontainer.h"
#include "utils.h"



namespace pedant {

//BaseDependencyContainer

BaseDependencyContainer::BaseDependencyContainer( const Configuration& config, std::unordered_map<int, std::vector<int>>&& dependencies,
                                                  const std::unordered_set<int>& universal_variables,
                                                  const std::vector<int>& ordered_universals) :
                                                  config(config), universal_variables(universal_variables), 
                                                  dependencies(dependencies), ordered_universals(ordered_universals) {
}

bool BaseDependencyContainer::hasDependencies(int var) const {
  if (innermost_existentials.find(var) != innermost_existentials.end()) {
    return true;
  }
  return dependencies.find(var) != dependencies.end() && !dependencies.at(var).empty();
}

std::vector<int> BaseDependencyContainer::restrictToDendencies(const std::vector<int>& literals, int var) const {
  const auto& deps = dependencies.at(var);
  return restrictToVector(literals, deps);
}
  
void BaseDependencyContainer::setDependencies(int var, const std::vector<int>& deps) {//dependencies shall be sorted
  dependencies.emplace(var, deps);
}

void BaseDependencyContainer::addInnermostExistential(int var) {
  innermost_existentials.insert(var);
}

const std::vector<int>& BaseDependencyContainer::getDependencies(int var) const {
  if (innermost_existentials.find(var) != innermost_existentials.end()) {
    return ordered_universals;
  }
  return dependencies.at(var);
}

 /**
 * @param literals sorted with respect to absolute values
 * @param range sorted
 **/
std::vector<int> BaseDependencyContainer::restrictToVector(const std::vector<int>& literals, const std::vector<int>& range) const{
  std::vector<int> literals_range;
  for (int i = 0, j = 0; i < literals.size() && j < range.size() ;) {
    auto variable = abs(literals[i]);
    if (variable < range[j]) {
      i++;
    } else if (variable == range[j]) {
      literals_range.push_back(literals[i]);
      i++;
      j++;
    } else {
      j++;
    }
  }
  return literals_range;
} 


// DependencyContainer_Vector

DependencyContainer_Vector::DependencyContainer_Vector( const Configuration& config, std::unordered_map<int, std::vector<int>>&& dependencies, 
                                                        std::unordered_map<int, std::vector<int>>&& extended_dependencies,
                                                        const std::set<int>& undefined_variables, const std::unordered_set<int>& universal_variables,
                                                        const std::vector<int>& ordered_universals) :
                                                        BaseDependencyContainer(config, std::move(dependencies), universal_variables, ordered_universals),
                                                        extended_dependencies_map(std::move(extended_dependencies)), undefined_variables(undefined_variables) {
  for (auto& [var, deps] : extended_dependencies_map) {
    std::sort(deps.begin(), deps.end());
  }
}

void DependencyContainer_Vector::updateExtendedDependencies(int variable, const std::set<int>& support_set, std::set<int>& updated_variables) {
  extended_dependencies_to_compute.insert(variable);
  supports.emplace(variable, std::vector<int>(support_set.begin(), support_set.end()));
  extended_dependencies_map.erase(variable);
  for (const auto& var : undefined_variables) {
    auto& deps = extended_dependencies_map.at(var);
    if (std::includes(deps.begin(), deps.end(), support_set.begin(), support_set.end())) {
      updated_variables.insert(var);
      std::vector<int> x {variable};
      updateVectorMap(var, x);
    }
  }
}

void DependencyContainer_Vector::setExtendedDependencies(int var, const std::vector<int>& deps) {
  extended_dependencies_map.emplace(var, deps);
}

void DependencyContainer_Vector::setExtendedDependencies(int var, const std::set<int>& dependencies) {
  extended_dependencies_map.emplace(var, std::vector<int>(dependencies.begin(), dependencies.end()));
}

void DependencyContainer_Vector::scheduleUpdate(int variable, const std::set<int>& support_set, std::set<int>& updated_variables) {
  extended_dependencies_to_compute.insert(variable);
  supports.emplace(variable, std::vector<int>(support_set.begin(), support_set.end()));
  variables_to_update.erase(variable);
  extended_dependencies_map.erase(variable);
  for (const auto& var : undefined_variables) {
    // auto& deps = extended_dependencies_map.at(var);
    if (includedInDependencies(var, support_set)) {
      updated_variables.insert(var);
      variables_to_update[var].push_back(variable);
    }
  }
}

void DependencyContainer_Vector::performUpdate() {
  for (auto& [key, val] : variables_to_update) {
    updateVectorMap(key, val);
  }
  variables_to_update.clear();
}

bool DependencyContainer_Vector::includedInExtendedDependencies(int var, const std::vector<int>& variables) const {
  if (extended_dependencies_to_compute.find(var) != extended_dependencies_to_compute.end()) {
    auto deps = computeDynamicDependencies(var);
    return std::includes(deps.begin(), deps.end(), variables.begin(), variables.end());
  } else {
    auto& deps = extended_dependencies_map.at(var);
    return std::includes(deps.begin(), deps.end(), variables.begin(), variables.end());
  }
}

bool DependencyContainer_Vector::includedInExtendedDependencies(int var, const std::set<int>& variables) const {
  if (extended_dependencies_to_compute.find(var) != extended_dependencies_to_compute.end()) {
    auto deps = computeDynamicDependencies(var);
    return std::includes(deps.begin(), deps.end(), variables.begin(), variables.end());
  } else {
    auto& deps = extended_dependencies_map.at(var);
    return std::includes(deps.begin(), deps.end(), variables.begin(), variables.end());
  } 
}

bool DependencyContainer_Vector::containedInExtendedDependencies(int var1, int var2) const {
  if (extended_dependencies_to_compute.find(var2) != extended_dependencies_to_compute.end()) {
    auto deps = computeDynamicDependencies(var2);
    return deps.find(var1) != deps.end();
  } else {
    auto& deps = extended_dependencies_map.at(var2);
    return std::binary_search(deps.begin(), deps.end(), var1);
  } 
}

bool DependencyContainer_Vector::largerExtendedDependencies(int var1, int var2) const {
  int size1, size2;
  if (extended_dependencies_to_compute.find(var1) != extended_dependencies_to_compute.end()) {
    size1 = computeDynamicDependencies(var1).size();
  } else {
    size1 = extended_dependencies_map.at(var1).size();
  }

  if (extended_dependencies_to_compute.find(var2) != extended_dependencies_to_compute.end()) {
    size2 = computeDynamicDependencies(var2).size();
  } else {
    size2 = extended_dependencies_map.at(var2).size();
  }

  return size1 > size2;
}

std::vector<int> DependencyContainer_Vector::restrictToExtendedDendenciesSorted(const std::vector<int>& literals, int var) const {
  if (literals.empty()) {
    return {};
  }

  if (extended_dependencies_to_compute.find(var) != extended_dependencies_to_compute.end()) {
    auto deps = computeDynamicDependencies(var);
    return restrictTo(literals, deps); 
  } else {
    const auto& deps = extended_dependencies_map.at(var);
    return restrictToVector(literals, deps); 
  }
}

std::vector<int> DependencyContainer_Vector::restrictToExtendedDendencies(const std::vector<int>& literals, int variable) const {
  if (literals.empty()) {
    return {};
  }
  if (extended_dependencies_to_compute.find(variable) != extended_dependencies_to_compute.end()) {
    auto deps = computeDynamicDependencies(variable);
    return restrictTo(literals, deps); 
  } else {
    const auto& deps = extended_dependencies_map.at(variable);
    std::vector<int> literals_range;
    for (const auto& l: literals) {
      auto v = var(l);
      if (std::binary_search(deps.begin(), deps.end(), v)) {
        literals_range.push_back(l);
      }
    }
    return literals_range;
  }
}

std::vector<int> DependencyContainer_Vector::getExtendedDependencies(int var) const {
  if (extended_dependencies_to_compute.find(var) != extended_dependencies_to_compute.end()) {
    auto deps = computeDynamicDependencies(var);
    return std::vector<int> (deps.begin(), deps.end());
  } else {
    return extended_dependencies_map.at(var);
  }
}

std::vector<int> DependencyContainer_Vector::getExistentialDependencies(int var) const {
  auto deps = getExtendedDependencies(var);
  std::vector<int> result;
  for (auto v : deps) {
    if (universal_variables.find(v) == universal_variables.end()) {
      result.push_back(v);
    }
  }
  return result;
}

std::set<int> DependencyContainer_Vector::computeDynamicDependencies(int var) const {
  auto support = supports.at(var);
  std::set<int> result(support.begin(), support.end());
  for (auto var : support) {
    if (universal_variables.find(var) != universal_variables.end()) {
      result.insert(var);
    } else if (extended_dependencies_to_compute.find(var) != extended_dependencies_to_compute.end()) {
      auto deps = computeDynamicDependencies(var);
      result.insert(deps.begin(), deps.end());
    } else {
      const auto& deps = extended_dependencies_map.at(var);
      result.insert(deps.begin(), deps.end());
    }
  }
  return result;
}

void DependencyContainer_Vector::updateVectorMap(int var, const std::vector<int>& variables_to_include) {
  auto& deps = extended_dependencies_map.at(var);
  std::vector<int> new_deps;
  new_deps.reserve(deps.size() + variables_to_include.size());

  int i = 0, j = 0;
  while(i < deps.size() && j < variables_to_include.size()) {
    auto& v1 = deps[i];
    auto& v2 = variables_to_include[j];
    if (v1 == v2) {
      new_deps.push_back(v1);
      i++;
      j++;
    } else if (v1 < v2) {
      new_deps.push_back(v1);
      i++;
    } else {
      new_deps.push_back(v2);
      j++;
    }
  }
  new_deps.insert(new_deps.end(), deps.begin() + i, deps.end());
  new_deps.insert(new_deps.end(), variables_to_include.begin() + j, variables_to_include.end());
  std::swap(deps, new_deps);
}

bool DependencyContainer_Vector::includedInDependencies(int var, const std::set<int>& support_set) const {
  assert(extended_dependencies_to_compute.find(var) == extended_dependencies_to_compute.end());
  const auto& deps = extended_dependencies_map.at(var);
  if (variables_to_update.find(var) == variables_to_update.end()) {
    return std::includes(deps.begin(), deps.end(), support_set.begin(), support_set.end());
  }

  auto dep1_it = deps.begin();
  auto dep1_last = deps.end();
  auto dep2_it = variables_to_update.at(var).begin();
  auto dep2_last = variables_to_update.at(var).end();


  auto supp_it = support_set.begin();
  auto supp_last = support_set.end();

  while (supp_it != supp_last) {
    if (dep1_it == dep1_last) {
      return std::includes(dep2_it, dep2_last, supp_it, supp_last);
    } 
    if (dep2_it == dep2_last) {
      return std::includes(dep1_it, dep1_last, supp_it, supp_last);
    }
    if (*supp_it < *dep1_it && *supp_it < *dep2_it) {
      return false;
    }
    if (*supp_it == *dep1_it || *supp_it == *dep2_it) {
      supp_it++;
    }
    if (*dep1_it < *supp_it) {
      dep1_it++;
    }
    if (*dep2_it < *supp_it) {
      dep2_it++;
    }
  }
  return true;
}

bool DependencyContainer_Vector::isUndefined(int var) const {
  return undefined_variables.find(var) != undefined_variables.end();
}

size_t DependencyContainer_Vector::getNofUndefined() const {
  return undefined_variables.size();
}


}