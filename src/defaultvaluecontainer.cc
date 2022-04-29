#include <algorithm>

#include "defaultvaluecontainer.h"
#include "utils.h"


#ifndef USE_MACHINE_LEARNING
#include <cassert>
#endif

#ifdef SAMPLE
#include "sampler.h"
#endif


namespace pedant {

DefaultValueContainer::DefaultValueContainer(const std::vector<int>& universal_variables, 
      const std::vector<int>& existential_variables,
      const DependencyContainer& dependencies, 
      int& last_used_variable, std::vector<int>& assumptions, const Configuration& config) :
      config(config), last_used_variable (last_used_variable), assumptions(assumptions),
      universal_variables(universal_variables), existential_variables(existential_variables),
      dependencies(dependencies) {

  #ifdef USE_MACHINE_LEARNING
    use_ml_trees = config.def_strat == Functions;
  #else
    use_ml_trees = false;
  #endif

  // int nof_existentials = dependency_map.size();
  int nof_existentials = existential_variables.size();

}

void DefaultValueContainer::setFixedDefaultPolarity(int variable, bool polarity) {
  if (useDefault(variable) && !treeActive(variable)) {
    setFixedDefaultPolarityUnchecked(variable, polarity);
  }
}

void DefaultValueContainer::setFixedDefaultPolarityUnchecked(int variable, bool polarity) {
  int positive_fixed_default_index_assumptions = positive_fixed_default_indcies_assumptions.at(variable);
  int negative_fixed_default_index_assumptions = negative_fixed_default_indcies_assumptions.at(variable);
  int positive_fixed_default_index_default_selectors = positive_fixed_default_indcies_default_selectors.at(variable);
  int negative_fixed_default_index_default_selectors = negative_fixed_default_indcies_default_selectors.at(variable);

  if (polarity) {
    assumptions[positive_fixed_default_index_assumptions] = abs(assumptions[positive_fixed_default_index_assumptions]);
    assumptions[negative_fixed_default_index_assumptions] = -abs(assumptions[negative_fixed_default_index_assumptions]);

    default_selectors[positive_fixed_default_index_default_selectors] = abs(default_selectors[positive_fixed_default_index_default_selectors]);
    default_selectors[negative_fixed_default_index_default_selectors] = -abs(default_selectors[negative_fixed_default_index_default_selectors]);
  } else {
    assumptions[positive_fixed_default_index_assumptions] = -abs(assumptions[positive_fixed_default_index_assumptions]);
    assumptions[negative_fixed_default_index_assumptions] = abs(assumptions[negative_fixed_default_index_assumptions]);

    default_selectors[positive_fixed_default_index_default_selectors] = -abs(default_selectors[positive_fixed_default_index_default_selectors]);
    default_selectors[negative_fixed_default_index_default_selectors] = abs(default_selectors[negative_fixed_default_index_default_selectors]);
  }
}

std::vector<std::pair<int,std::vector<Clause>>> DefaultValueContainer::initialize() {
  std::vector<std::pair<int,std::vector<Clause>>> result;
  for (auto var : existential_variables) {
    if (dependencies.isUndefined(var)) {
      variables_with_defaults.insert(var);
      #ifdef USE_MACHINE_LEARNING
        if (use_ml_trees) {
          if (config.use_existentials_in_tree) {
            auto extended_dependencies = dependencies.getExtendedDependencies(var);
            if (!extended_dependencies.empty()) {
              default_trees.emplace(var, HoeffdingDefaultTree(var, extended_dependencies, assumptions, default_selectors, last_used_variable, config));
            }
          } else {
            if (dependencies.hasDependencies(var)) {
              default_trees.emplace(var, HoeffdingDefaultTree(var, dependencies.getDependencies(var), assumptions, default_selectors, last_used_variable, config));
            }
          }
        }
      #endif
      auto [sel1, sel2] = newFixedDefaultSelector(var);
      Clause cl1 {-sel1, var};
      Clause cl2 {-sel2, -var};
      result.push_back(std::make_pair(var,std::vector<Clause> {cl1, cl2}));
    }
  }
  return result;
}


std::vector<Clause> DefaultValueContainer::insertConflict(int forced_literal, const std::vector<int>& counterexample_sample) {
  int variable = var(forced_literal);
  if (!useDefault(variable)) {
    return {};
  }

  //Especially if we do not use existential variabes in samples it can be the case that the sample space is empty.
  //Thus there is no reason to learn a default function for such a variable even if we generally want to use them.
  //For this reason there is the !treeIsAvailable(variable) check.
  if (!use_ml_trees || !treeIsAvailable(variable)) {
    setFixedDefaultPolarityUnchecked(variable,forced_literal>0);
    return {};
  }
  #ifdef USE_MACHINE_LEARNING
    bool tree_empty = default_trees.at(variable).empty();
    if (tree_empty) {
      setFixedDefaultPolarityUnchecked(variable,forced_literal>0);
    }
    nof_insertions[variable]++;
    auto val = nof_insertions[variable];
    std::vector<Clause> clauses = default_trees.at(variable).insertConflict(forced_literal,counterexample_sample);

    if (tree_empty && !clauses.empty()) {
      disableFixedDefaults(variable);
      // Two new clauses are generated, but one old clause is deactivated.
      nof_default_clauses_per_variable[variable] = nof_default_clauses_per_variable[variable]==0 ? 2 : nof_default_clauses_per_variable[variable] + 1;
    }
    return clauses;
  #else
    assert(false);
    return {};
  #endif
}

std::unordered_map<int,std::vector<Clause>> DefaultValueContainer::getCertificate() const {
  std::unordered_map<int,std::vector<Clause>> result;
  for (auto var : existential_variables) {
    if (!useDefault(var)) {
      continue;
    }
    if (!treeActive(var)) {
      int selector_index = positive_fixed_default_indcies_default_selectors.at(var);
      Clause cl;
      if (default_selectors[selector_index]>0) {//the selector is active
        cl.push_back(var);
      } else {
        cl.push_back(-var);
      }
      result[var] = {cl};
    } else {
      #ifdef USE_MACHINE_LEARNING
        result[var] = default_trees.at(var).retrieveCompleteRepresentation();
      #else
        assert(false);
      #endif
    }
  }
  return result;
}

std::vector<std::vector<Clause>> DefaultValueContainer::insertSample(const std::vector<Clause>& matrix, std::vector<int> variables_to_sample) {
  std::vector<std::vector<Clause>> result;
  #ifdef SAMPLE
    Sampler sampler(matrix,variables_to_sample);
    for (int v : variables_to_sample) {
      int variable_index = existential_indices.at(v);
      if (!dependencies.hasDependencies(v)) {
        result.push_back({});
        clauses_learned_from_samples[variable_index] = 0;
        continue;
      }
      auto samples = sampler.sample(v,config.nof_samples);
      //if we can freely assign an existential variable for each universal assignment then the encoding is unsat
      if (samples.empty()) {
        result.push_back({});
        clauses_learned_from_samples[variable_index] = 0;
        continue;
      }
      for (auto s : samples) {
        std::sort(s.begin(), s.end(), [](int i, int j) { return abs(i) < abs(j); });
      }
      size_t label_index=-1;
      for (size_t i=0; i<samples[0].size(); i++) {
        if (abs(samples[0][i]) == v) {
          label_index = i;
          break;
        }
      }
      assert(label_index!=-1);
      std::vector<int> labels;
      labels.reserve(samples.size());
      for (auto s : samples) {
        if (s[label_index]>0) {
          labels.push_back(v);
        } else {
          labels.push_back(-v);
        }
      }
      assert(label_index!=-1);
      std::vector<Clause> clauses = default_trees.at(v).insertSamples(labels,samples);
      if (!clauses.empty()) {
        disableFixedDefaults(variable_index);
        tree_active[variable_index] = true;
        // Two new clauses are generated, but one old clause is deactivated.
        nof_default_clauses_per_variable[variable_index] = nof_default_clauses_per_variable[variable_index]==0 ? 2 : nof_default_clauses_per_variable[variable_index] + 1;
      }
      clauses_learned_from_samples[variable_index] = clauses.size();
      result.push_back(clauses);
    }
  #endif
  return result;
}

std::tuple<int, std::unordered_map<int,int>, int> DefaultValueContainer::getStatistic() const {
  std::unordered_map<int,int> learned_clasues;
  int total_number_of_active_default_clauses=0;
  for (const auto& [var, nof_default_clauses] : nof_default_clauses_per_variable) {
    total_number_of_active_default_clauses += nof_default_clauses_per_variable.at(var);
  }

  
  if (config.log_default_tree) {
    std::vector<int> existentials;
    for (const auto& [var, nof] : nof_insertions) {
      existentials.push_back(var);
    }
    std::sort(existentials.begin(), existentials.end());
    std::ofstream log(config.write_default_tree_log_to);
    log<<"Var, nof insertions, learnt clauses\n";
    for (auto var : existentials) {
      int nof_clauses=0;
      if (nof_default_clauses_per_variable.find(var) != nof_default_clauses_per_variable.end()) {
        nof_clauses = nof_default_clauses_per_variable.at(var);
      }
      log<<var<<","<<nof_insertions.at(var)<<","<<nof_clauses<<"\n";
    }
    log.close();
  }


  int learned_from_samples = 0;
  #ifdef USE_MACHINE_LEARNING
    if (use_ml_trees && config.return_default_tree) {
      std::ofstream file(config.write_default_tree_to);
      std::vector<int> existentials(existential_variables);
      std::sort(existentials.begin(),existentials.end());
      for (int e : existentials) {
        if (treeActive(e)) {
           default_trees.at(e).logTree(file);
        }
      }
      file.close();
    }
    

    if (use_ml_trees && config.visualise_default_trees) {
      for (auto var : existential_variables) {
        if (treeActive(var)) {
          default_trees.at(var).visualiseTree();
        }
      }
    }

    if (config.use_sampling) {
      for (const auto& [var, no] : clauses_learned_from_samples) {
        learned_from_samples += no;
      }
    }
  #endif

  
  return std::make_tuple(total_number_of_active_default_clauses, learned_clasues, learned_from_samples);
}

}