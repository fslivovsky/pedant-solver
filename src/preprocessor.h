#ifndef PEDANT_PREPROCESSOR_H
#define PEDANT_PREPROCESSOR_H

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "solvertypes.h"

namespace pedant {

class Preprocessor {
 public:
  Preprocessor(std::vector<int>& universal_variables, std::unordered_map<int, std::vector<int>>& dependency_map, std::vector<Clause>& matrix);
  void preprocess();

 private:
  int applyForallReduction(Clause& clause);

  std::vector<int>& universal_variables;
  std::unordered_map<int, std::vector<int>>& dependency_map;
  std::vector<Clause>& matrix;

  std::unordered_map<int, std::unordered_set<int>> dependency_map_set;
  std::unordered_set<int> universal_variables_set;

};

};

#endif 