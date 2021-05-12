#ifndef PEDANT_SIMPLEVALIDITYCHECKER_H_
#define PEDANT_SIMPLEVALIDITYCHECKER_H_

#include <vector>
#include <unordered_map>
#include <tuple>

#include <assert.h>

#include "skolemcontainer.h"
#include "logging.h"
#include "utils.h"
#include "cadical.h"

namespace pedant {


class SkolemContainer; // Forward declaration.

class SimpleValidityChecker {

 public:
  SimpleValidityChecker(std::vector<int>& universal_variables,
                        std::unordered_map<int, std::vector<int>>& dependency_map,
                        std::vector<Clause>& matrix,
                        int& last_used_variable);
  bool checkArbiterAssignment(std::vector<int>& arbiter_assignment, SkolemContainer& skolem_container);
  void setDefined(int variable);
  void addClauseValidityCheck(int variable, Clause& clause);
  void addClauseConflictExtraction(Clause& clause);
  std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> getConflict();
  void addArbiterVariable(int variable);
  bool checkForcedExtraction(Clause& forcing_clause);

 private:
  int& selector(int variable);

  std::vector<int> selectors;
  std::unordered_map<int, int> variable_to_selector_index;

  std::vector<int> existential_variables;
  std::vector<int> universal_variables;
  std::vector<int> arbiter_variables;

  std::vector<int> failing_existential_assignment;
  std::vector<int> failing_universal_assignment;
  std::vector<int> failing_arbiter_assignment;

  int& last_used_variable;
  CadicalSolver validity_check_solver;
  CadicalSolver conflict_extraction_solver;

};

inline int& SimpleValidityChecker::selector(int variable) {
  return selectors[variable_to_selector_index[variable]];
}

inline void SimpleValidityChecker::addClauseConflictExtraction(Clause& clause) {
  conflict_extraction_solver.addClause(clause);
}

inline void SimpleValidityChecker::addArbiterVariable(int variable) {
  arbiter_variables.push_back(variable);
}

inline bool SimpleValidityChecker::checkForcedExtraction(Clause& forcing_clause) {
  auto forced_literal = forcing_clause.back();
  Clause negated_clause = forcing_clause;
  negateEach(negated_clause);
  return (conflict_extraction_solver.solve(negated_clause) == 20);
}

}


#endif // PEDANT_SIMPLEVALIDITYCHECKER_H_