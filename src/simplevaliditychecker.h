#ifndef PEDANT_SIMPLEVALIDITYCHECKER_H_
#define PEDANT_SIMPLEVALIDITYCHECKER_H_

#include <vector>
#include <unordered_map>
#include <tuple>
#include <memory>
#include <set>

#include <assert.h>

#include "skolemcontainer.h"
#include "logging.h"
#include "utils.h"
#include "satsolver.h"
#include "configuration.h"
#include "supporttracker.h"
#include "dependencycontainer.h"

namespace pedant {


class SkolemContainer; // Forward declaration.

class SimpleValidityChecker {

 public:
  SimpleValidityChecker(const std::vector<int>& existential_variables,
                        const std::vector<int>& universal_variables,
                        const DependencyContainer& dependencies,
                        std::vector<Clause>& matrix,
                        int& last_used_variable,
                        SkolemContainer& skolem_container, const Configuration& config);
  bool checkArbiterAssignment(std::vector<int>& arbiter_assignment);
  void setDefined(int variable);
  void addClauseValidityCheck(int variable, Clause& clause);
  void addClauseConflictExtraction(Clause& clause);
  std::tuple<Clause, Clause, Clause, Clause, Clause> getConflict();
  std::vector<int> getExistentialResponse(const std::vector<int>& universal_assignment, const std::vector<int>& arbiter_assignment);
  void setNoForcingClauseActiveVariable(int existential_variable, int no_forcing_clause_active_variable);
  void addArbiterVariable(int arbiter_variable);
  void addDefiningClause(int variable, const Clause& defining_clause, int activity_variable);

 private:
  void setFailingAssignments(std::vector<int>& arbiter_assignment);
  bool hasConflict(const std::vector<int>& existential_assignment, const std::vector<int>& universal_assignment, const std::vector<int>& arbiter_assignment, int conflict_limit=0);
  void minimizeAssumptions(std::vector<int>& assumptions_to_minimize, std::vector<int>& assumptions_to_keep, std::vector<int>& other_assumptions_to_keep);
  int& selector(int variable);
  std::tuple<std::unordered_set<int>, std::unordered_set<int>> getActiveSupport(int variable);

  std::vector<int> selectors;
  std::vector<int> clause_satisfied_variables;
  std::unordered_map<int, int> variable_to_selector_index;
  std::vector<int> existential_variables;
  const std::vector<int>& universal_variables;
  std::vector<int> failing_existential_assignment, failing_universal_assignment, failing_arbiter_assignment;
  std::vector<int> full_universal_assignment;
  std::vector<int> full_existential_assignment;
  std::vector<Clause>& matrix;
  int& last_used_variable;
  std::shared_ptr<SatSolver> validity_check_solver;
  std::shared_ptr<SatSolver> conflict_extraction_solver;
  SkolemContainer& skolem_container;
  SupportTracker supporttracker;
  const Configuration& config;
};

inline int& SimpleValidityChecker::selector(int variable) {
  return selectors[variable_to_selector_index[variable]];
}

inline void SimpleValidityChecker::addClauseConflictExtraction(Clause& clause) {
  conflict_extraction_solver->addClause(clause);
}

inline bool SimpleValidityChecker::hasConflict(const std::vector<int>& existential_assignment, const std::vector<int>& universal_assignment, const std::vector<int>& arbiter_assignment, int conflict_limit) {
  conflict_extraction_solver->assume(existential_assignment);
  conflict_extraction_solver->assume(universal_assignment);
  conflict_extraction_solver->assume(arbiter_assignment);
  if (conflict_limit == 0) {
    return conflict_extraction_solver->solve() == 20;
  } else {
    return conflict_extraction_solver->solve(conflict_limit) == 20;
  }
}

inline void SimpleValidityChecker::setNoForcingClauseActiveVariable(int existential_variable, int no_forcing_clause_active_variable) {
  supporttracker.setNoForcedVariable(existential_variable, no_forcing_clause_active_variable);
}

inline void SimpleValidityChecker::addArbiterVariable(int arbiter_variable) {
  supporttracker.addArbiterVariable(arbiter_variable);
}

inline void SimpleValidityChecker::addDefiningClause(int variable, const Clause& defining_clause, int activity_variable) {
  supporttracker.addDefiningClause(variable, defining_clause, activity_variable);
}

}


#endif // PEDANT_SIMPLEVALIDITYCHECKER_H_