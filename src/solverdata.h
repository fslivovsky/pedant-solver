#ifndef SOLVERDATA_H_
#define SOLVERDATA_H_

#include <unordered_map>

namespace pedant {

struct SolverData {
  std::unordered_map<int, int> arbiter_to_existential;
};


}

#endif