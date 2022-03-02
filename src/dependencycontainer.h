#ifndef DEPENDENCY_CONTAINER_H_
#define DEPENDENCY_CONTAINER_H_

#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <set>

#include "configuration.h"

namespace pedant {


class DependencyContainer_Vector;

using DependencyContainer = DependencyContainer_Vector;

class BaseDependencyContainer {

 public:

  BaseDependencyContainer( const Configuration& config, std::unordered_map<int, std::vector<int>>&& dependencies,
                          const std::set<int>& undefined_variables, const std::unordered_set<int>& universal_variables);

  bool hasDependencies(int var) const;
  std::vector<int> restrictToDendencies(const std::vector<int>& literals, int var) const;
  void setDependencies(int var, const std::vector<int>& dependencies);//dependencies shall be sorted
  const std::vector<int>& getDependencies(int var) const;

 private:
  const Configuration& config;
  std::unordered_map<int, std::vector<int>> dependencies;

 protected:
  std::vector<int> restrictToVector(const std::vector<int>& literals, const std::vector<int>& range) const;
  const std::set<int>& undefined_variables;
  const std::unordered_set<int>& universal_variables;

};

// DependencyContainer_Vector

class DependencyContainer_Vector : public BaseDependencyContainer {

 public:
  DependencyContainer_Vector(const Configuration& config, std::unordered_map<int, std::vector<int>>&& dependencies, 
                    std::unordered_map<int, std::vector<int>>&& extended_dependencies,
                    const std::set<int>& undefined_variables, const std::unordered_set<int>& universal_variables);

  void updateExtendedDependencies(int var, const std::set<int>& support_set, std::set<int>& updated_variables);   
  void scheduleUpdate(int var, const std::set<int>& support_set, std::set<int>& updated_variables);      
  void performUpdate(); 
  void setExtendedDependencies(int var, const std::vector<int>& dependencies);
  void setExtendedDependencies(int var, const std::set<int>& dependencies);
  bool includedInExtendedDependencies(int var, const std::vector<int>& variables) const;
  bool includedInExtendedDependencies(int var, const std::set<int>& variables) const;
  bool containedInExtendedDependencies(int var1, int var2) const;
  bool largerExtendedDependencies(int var1, int var2) const;
  std::vector<int> getExtendedDependencies(int var) const;
  std::vector<int> getExistentialDependencies(int var) const;
  std::vector<int> restrictToExtendedDendencies(const std::vector<int>& literals, int var) const;
  std::vector<int> restrictToExtendedDendenciesSorted(const std::vector<int>& literals, int var) const;

 private:
  std::set<int> computeDynamicDependencies(int var) const;
  void updateVectorMap(int var, const std::vector<int>& variables_to_include);
  bool includedInDependencies(int var, const std::set<int>& support_set) const;

  std::unordered_set<int> extended_dependencies_to_compute;
  std::unordered_map<int, std::vector<int>> supports;
  std::unordered_map<int, std::vector<int>> variables_to_update;

  std::unordered_map<int, std::vector<int>> extended_dependencies_map;

};


}

#endif