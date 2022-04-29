#ifndef PEDANT_PREPROCESSOR_H_
#define PEDANT_PREPROCESSOR_H_

#include <vector>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#include "solvertypes.h"
#include "dependencyextractor.h"
#include "inputformula.h"

namespace pedant {

class Preprocessor {
 public:
  /**
   * We will consume formula - do not use formula afterwards
   */
  Preprocessor(DQDIMACS& formula, const Configuration& config);
  InputFormula preprocess();


 private:

  int applyForallReduction(Clause& clause, const std::unordered_set<int>& universal_set, const std::unordered_set<int>& innermost_existentials,
      const std::unordered_map<int, std::unordered_set<int>>& dependency_map_set);
  
  const Configuration& config;
  DependencyExtractor dependencies;
  DQDIMACS& formula;

};


};

#endif 