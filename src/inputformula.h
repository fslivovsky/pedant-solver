#ifndef INPUTFORMULA_H_
#define INPUTFORMULA_H_

#include <vector>
#include <unordered_map>

#include "solvertypes.h"

namespace pedant {

struct InputFormula {

 public:
  std::vector<Clause> matrix;
  std::unordered_map<int, std::vector<int>> dependencies;
  std::unordered_map<int, std::vector<int>> extended_dependenices;

  //e.g. tseitin variables
  bool innermost_existential_block_present = false;
  size_t start_index_innnermost_existentials = 0;
  size_t end_index_innnermost_existentials = 0;

  std::vector<int> universal_variables;
  std::vector<int> existential_variables;

};

}


#endif