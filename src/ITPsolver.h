#ifndef PEDANT_ITPSOLVER_H_
#define PEDANT_ITPSOLVER_H_

#include <vector>
#include <memory>
#include <tuple>

#include "InterpolatingSolver.h"

#include "solvertypes.h"

namespace pedant {

class ITPSolver {
  
 public:
  ITPSolver(std::vector<Clause>& _first_part, std::vector<Clause>& _second_part);
  void resetSolver();
  bool addClause(Clause& clause, bool add_to_first_part=true);
  bool solve(std::vector<int>& assumptions, int limit=-1);
  std::vector<int> getConflict();
  std::tuple<std::vector<std::tuple<std::vector<int>,int>>, std::vector<int>> getDefinition(const std::vector<int>& input_variable_ids, int output_variable_id, int offset, bool compress=true);

 private:
  std::unique_ptr<InterpolatingSolver> interpolatingsolver;
  int max_var_index;
  std::vector<Clause> first_part, second_part;
};

}

#endif // PEDANT_ITPSOLVER_H_