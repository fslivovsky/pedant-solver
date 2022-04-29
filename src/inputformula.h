#ifndef INPUTFORMULA_H_
#define INPUTFORMULA_H_

#include <vector>
#include <unordered_map>
#include <algorithm>

#include "solvertypes.h"

namespace pedant {

struct InputFormula {

 public:
  std::vector<Clause> matrix;
  std::unordered_map<int, std::vector<int>> dependencies;
  std::unordered_map<int, std::vector<int>> extended_dependenices;

  //e.g. tseitin variables
  bool innermost_existential_block_present = false;
  size_t start_index_innermost_existentials = 0;
  size_t end_index_innermost_existentials = 0;

  std::vector<int> universal_variables;
  std::vector<int> existential_variables;

  int max_used_variable;

  /**
   * Variables with a definition are not part of the dependency maps
   **/
  std::unordered_map<int, std::tuple<std::vector<Clause>, Circuit>> definitions;

  // // std::vector<std::tuple<std::vector<int>,int>> the type of the circuits used in the definability checker
  // void addDefinition(int var, std::vector<Clause>& definition_clauses, Circuit& definition_circuit);

  void setMaxVariable();

};

inline void InputFormula::setMaxVariable() {
  int max_universal = universal_variables.empty() ? 0 : *std::max_element(universal_variables.begin(), universal_variables.end());
  int max_existential = existential_variables.empty() ? 0 : *std::max_element(existential_variables.begin(), existential_variables.end());
  max_used_variable = std::max(max_universal, max_existential);
  for (const auto& [var, defs] : definitions) {
    auto& [cnf_def, circuit_def] = defs;
    for (const auto& gate : circuit_def) {
      auto& [input_literals, output_literal] = gate;
      max_used_variable = std::max(max_used_variable, abs(output_literal));
    }
  }
}

}


#endif