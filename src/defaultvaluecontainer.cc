#include <algorithm>

#include "defaultvaluecontainer.h"
#include "utils.h"


#ifndef USE_MACHINE_LEARNING
#include <cassert>
#endif




namespace pedant {

DefaultValueContainer::DefaultValueContainer(const std::vector<int>& universal_variables, 
      const std::vector<int>& existential_variables,
      // const std::unordered_map<int,std::vector<int>>& dependencies, 
      const DependencyContainer& dependencies, 
      int& last_used_variable, std::vector<int>& assumptions, const Configuration& config) :
      config(config), last_used_variable (last_used_variable), assumptions(assumptions),
      universal_variables(universal_variables), existential_variables(existential_variables),
      // dependency_map(dependencies), 
      dependencies(dependencies),
      tree_active(existential_variables.size(),false), 
      nof_default_clauses_per_variable(existential_variables.size(), 0),
      clauses_learned_from_samples(existential_variables.size(), 0),
      nof_insertions(existential_variables.size(), 0),
      use_tree(existential_variables.size(),false) {

  #ifdef USE_MACHINE_LEARNING
    use_ml_trees = config.def_strat == Functions;
  #else
    use_ml_trees = false;
  #endif
  int nof_existentials = existential_variables.size();

}

void DefaultValueContainer::setFixedDefaultPolarity(int variable, bool polarity) {
  int index = existential_indices.at(variable);
  if ( ! tree_active[index]) {
    setFixedDefaultPolarityIndex(index, polarity);
  }
}

void DefaultValueContainer::setFixedDefaultPolarityIndex(int variable_index, bool polarity) {
  if (polarity) {
    int idx = positive_fixed_default_indcies_assumptions[variable_index];
    assumptions[idx] = abs(assumptions[idx]);
    idx = positive_fixed_default_indcies_default_selectors[variable_index];
    default_selectors[idx] = abs(default_selectors[idx]);

    idx = negative_fixed_default_indcies_assumptions[variable_index];
    assumptions[idx] = -abs(assumptions[idx]);
    idx = negative_fixed_default_indcies_default_selectors[variable_index];
    default_selectors[idx] = -abs(default_selectors[idx]);
  } else {
    int idx = positive_fixed_default_indcies_assumptions[variable_index];
    assumptions[idx] = -abs(assumptions[idx]);
    idx = positive_fixed_default_indcies_default_selectors[variable_index];
    default_selectors[idx] = -abs(default_selectors[idx]);

    idx = negative_fixed_default_indcies_assumptions[variable_index];
    assumptions[idx] = abs(assumptions[idx]);
    idx = negative_fixed_default_indcies_default_selectors[variable_index];
    default_selectors[idx] = abs(default_selectors[idx]);
  }
}

std::vector<std::pair<int,std::vector<Clause>>> DefaultValueContainer::initialize() {

  for (size_t i = 0; i < existential_variables.size(); i++) {
    auto var = existential_variables[i];
    existential_indices[var] = i;
    if (use_ml_trees) {
      //TODO: check especially check: what happens if config.use_existentials_in_tree
      if (dependencies.hasDependencies(var)) {
        use_tree[i] = true;
        #ifdef USE_MACHINE_LEARNING
          default_trees.emplace(var, HoeffdingDefaultTree(var, dependencies, assumptions, default_selectors, last_used_variable, config));
        #endif
      }
    }
  }

  std::vector<std::pair<int,std::vector<Clause>>> result;
  for (const auto& var : existential_variables) {
    int idx = existential_indices.at(var);
    auto [sel1, sel2] = newFixedDefaultSelector(idx);
    Clause cl1 {-sel1, var};
    Clause cl2 {-sel2, -var};
    result.push_back(std::make_pair(var,std::vector<Clause> {cl1, cl2}));
  }
  return result;
}


std::vector<Clause> DefaultValueContainer::insertConflict(int forced_literal, const std::vector<int>& universal_assignment) {
  int variable = var(forced_literal);
  int variable_index = existential_indices.at(variable);
  if (!use_ml_trees || !use_tree[variable_index]) {
    setFixedDefaultPolarityIndex(variable_index,forced_literal>0);
    return {};
  }
  #ifdef USE_MACHINE_LEARNING
    nof_insertions[variable_index]++;
    auto val = nof_insertions[variable_index];
    std::vector<Clause> clauses = default_trees.at(variable).insertConflict(forced_literal,universal_assignment);

    if (!clauses.empty()) {
      disableFixedDefaults(variable_index);
      tree_active[variable_index] = true;
      nof_default_clauses_per_variable[variable_index] = nof_default_clauses_per_variable[variable_index]==0 ? 2 : nof_default_clauses_per_variable[variable_index] + 1;
    }
    return clauses;
  #else
    assert(false);
    return {};
  #endif
}

std::unordered_map<int,std::vector<Clause>> DefaultValueContainer::getCertificate() const {
  std::unordered_map<int,std::vector<Clause>> result;
  for (auto [ex,idx] : existential_indices) {
    if (!tree_active[idx]) { 
      int selector_index = positive_fixed_default_indcies_default_selectors[idx];
      Clause cl;
      if (default_selectors[selector_index]>0) {//the selector is active
        cl.push_back(ex);
      } else {
        cl.push_back(-ex);
      }
      result[ex] = {cl};
    } else {
      #ifdef USE_MACHINE_LEARNING
        result[ex] = default_trees.at(ex).retrieveCompleteRepresentation();
      #else
        assert(false);
      #endif
    }
  }
  return result;
}

std::vector<std::vector<Clause>> DefaultValueContainer::insertSample(const std::vector<Clause>& matrix, std::vector<int> variables_to_sample) {
  std::vector<std::vector<Clause>> result;
  return result;
}

std::tuple<int, std::unordered_map<int,int>, int> DefaultValueContainer::getStatistic() const {
  std::unordered_map<int,int> learned_clasues;
  int total_number_of_active_default_clauses=0;
  for (const auto& var : existential_variables) {
    int idx = existential_indices.at(var);
    learned_clasues[var] = nof_default_clauses_per_variable[idx];
    total_number_of_active_default_clauses += nof_default_clauses_per_variable[idx];
  }
  
  if (config.log_default_tree) {
    std::vector<int> existentials;
    existentials.reserve(existential_indices.size());
    for (const auto& var : existential_variables) {
      existentials.push_back(var);
    } 
    std::sort(existentials.begin(),existentials.end());
    std::ofstream log(config.write_default_tree_log_to);
    log<<"Var, nof insertions, learnt clauses\n";
    for (auto var : existentials) {
      int idx = existential_indices.at(var);
      log<<var<<","<<nof_insertions[idx]<<","<<nof_default_clauses_per_variable[idx]<<"\n";
    }
    log.close();
  }

  std::ofstream file(config.write_default_tree_to);

  int learned_from_samples = 0;
  #ifdef USE_MACHINE_LEARNING
    if (use_ml_trees && config.return_default_tree) {
      std::vector<int> existentials; //for a better readability the output shall be ordered
      for (auto [var, idx] : existential_indices) {
        existentials.push_back(var);
      }
      std::sort(existentials.begin(),existentials.end());
      for (int e : existentials) {
        int idx = existential_indices.at(e);
        if (use_tree[idx] && tree_active[idx]) {
          default_trees.at(e).logTree(file);
        }

      }
    }
    

    if (use_ml_trees && config.visualise_default_trees) {
      for (auto [var, idx] : existential_indices) {
        if (use_tree[idx] && tree_active[idx]) {
          default_trees.at(var).visualiseTree();
        }
      }
    }

    if (config.use_sampling) {
      for (int x : clauses_learned_from_samples) {
        learned_from_samples += x;
      }
    }
  #endif

  file.close();
  return std::make_tuple(total_number_of_active_default_clauses, learned_clasues, learned_from_samples);
}

}