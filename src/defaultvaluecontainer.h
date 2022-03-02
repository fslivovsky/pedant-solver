#ifndef PEDANT_DEFAULTVALUECONTAINER_H_
#define PEDANT_DEFAULTVALUECONTAINER_H_

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <tuple>
#include <fstream>

#include "solvertypes.h"
#include "configuration.h"
#include "dependencycontainer.h"

#ifdef USE_MACHINE_LEARNING
#include "hoeffdingDefaultTree.h"
#endif

namespace pedant {


class DefaultValueContainer {
 public:
  DefaultValueContainer(const std::vector<int>& universal_variables, 
      const std::vector<int>& existential_variables,
      // const std::unordered_map<int,std::vector<int>>& dependencies, 
      const DependencyContainer& dependencies, 
      int& last_used_variable, std::vector<int>& assumptions, 
      const Configuration& config);
  void setFixedDefaultPolarity(int variable, bool polarity);
  std::vector<std::pair<int,std::vector<Clause>>> initialize();
  std::vector<Clause> insertConflict(int forced_literal,const std::vector<int>& universal_assignment);
  const std::vector<int>& getSelectors() const;

  std::vector<std::vector<Clause>> insertSample(const std::vector<Clause>& matrix, std::vector<int> variables_to_sample);
  std::tuple<int, std::unordered_map<int,int>, int> getStatistic() const;

  /**
   * Retrieves all the active clause from the representations of the default values.
   * Assigns to each existential variable the associated default clauses. 
   * The clause, which are deactivated by the selectors are not considered for this purpose.
   **/
  std::unordered_map<int,std::vector<Clause>> getCertificate() const;


 private:
  const Configuration& config;
  int& last_used_variable;
  bool use_ml_trees;
  std::vector<int>& assumptions;
  const std::vector<int>& universal_variables;
  const std::vector<int>& existential_variables;
  const DependencyContainer& dependencies;
  std::unordered_map<int,int> existential_indices;
  std::vector<Clause> initial_clauses;

  std::vector<bool> use_tree;
  std::vector<bool> tree_active;

  std::vector<int> positive_fixed_default_indcies_assumptions;
  std::vector<int> negative_fixed_default_indcies_assumptions;

  std::vector<int> positive_fixed_default_indcies_default_selectors;
  std::vector<int> negative_fixed_default_indcies_default_selectors;

  #ifdef USE_MACHINE_LEARNING
    std::unordered_map<int,HoeffdingDefaultTree> default_trees;
  #endif


  std::vector<int> default_selectors;

  //Statistics
  std::vector<int> nof_default_clauses_per_variable;
  std::vector<int> nof_insertions;
  std::vector<int> clauses_learned_from_samples;

  std::pair<int,int> newFixedDefaultSelector(int existential_index);
  void disableFixedDefaults(int existential_index);
  void setFixedDefaultPolarityIndex(int variable_index, bool polarity);


};

inline void DefaultValueContainer::disableFixedDefaults(int existential_index) {
  int idx = positive_fixed_default_indcies_assumptions[existential_index];
  assumptions[idx] = -abs(assumptions[idx]);
  idx = negative_fixed_default_indcies_assumptions[existential_index];
  assumptions[idx] = -abs(assumptions[idx]);
  idx = positive_fixed_default_indcies_default_selectors[existential_index];
  default_selectors[idx] = -abs(default_selectors[idx]);
  idx = negative_fixed_default_indcies_default_selectors[existential_index];
  default_selectors[idx] = -abs(default_selectors[idx]);
}

inline std::pair<int,int> DefaultValueContainer::newFixedDefaultSelector(int existential_index) {
  positive_fixed_default_indcies_assumptions.push_back(assumptions.size());
  negative_fixed_default_indcies_assumptions.push_back(assumptions.size() + 1);

  positive_fixed_default_indcies_default_selectors.push_back(default_selectors.size());
  negative_fixed_default_indcies_default_selectors.push_back(default_selectors.size() + 1);

  int positive_selector = ++last_used_variable;
  assumptions.push_back(positive_selector);
  default_selectors.push_back(positive_selector);
  int negative_selector = ++last_used_variable;
  assumptions.push_back(-negative_selector);
  default_selectors.push_back(-negative_selector);
  return std::make_pair(positive_selector,negative_selector);
}



inline const std::vector<int>& DefaultValueContainer::getSelectors() const{
  return default_selectors;
}


}



#endif