#ifndef PEDANT_SOLVER_H_
#define PEDANT_SOLVER_H_

#include <set>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <memory>

#include "solvertypes.h"
#include "cadical.h"
#include "definabilitychecker.h"
#include "simplevaliditychecker.h"
#include "skolemcontainer.h"
#include "utils.h"
#include "unatechecker.h"
#include "configuration.h"
#include "interrupt.h"

namespace pedant {

class Solver {
 public:
  Solver(std::vector<int>& universal_variables, std::unordered_map<int, std::vector<int>>& dependency_map, std::vector<Clause>& matrix, Configuration& config);
  int solve();
  int newVariable();
  void printStatistics();

 private:
  bool checkArbiterAssignment();
  bool analyzeConflict(std::vector<int>& counterexample_universals, std::vector<int>& failed_existential_literals, std::vector<int>& failed_arbiter_literals);
  bool findArbiterAssignment();
  std::tuple<bool, std::vector<int>> checkForced(int literal, std::vector<int>& assignment_fixed, std::vector<int>& assignment_variable);
  std::tuple<Clause, bool> getForcingClause(int literal, std::vector<int>& assignment);
  void addForcingClause(Clause& forcing_clause, bool reduced);
  std::tuple<bool, std::vector<Clause>, std::vector<std::tuple<std::vector<int>,int>>, std::vector<int>> getDefinition(int variable, std::vector<int>& assumptions, bool use_extended_dependencies);
  template<typename T> void checkDefined(T& variables_to_check, std::vector<int>& assumptions, bool use_extended_dependencies);
  void addDefinition(int variable, std::vector<Clause>& definition, const std::vector<std::tuple<std::vector<int>,int>>& definition_circuit, std::vector<int>& conflict);
  int addArbiter(int literal, std::vector<int>& assignment);
  std::tuple<int, Clause, Clause> createArbiter(int variable, std::vector<int>& assignment);
  std::vector<int> restrictToArbiter(std::vector<int>& literals);
  std::vector<int> restrictToExistential(std::vector<int>& literals);

  #ifdef USE_MACHINE_LEARNING
  template<typename T> void setLearnedDefaultFunctions(T& variables_to_check);
  #endif

  void checkUnates();
  template<typename T>
  void setRandomDefaultValues(T& variables_to_set);
  void addArbitersForConstants();
  void minimizeFailedArbiterAssignment(std::vector<int>& failed_arbiter_assignment, std::vector<int>& universal_counterexample);

  int last_used_variable;
  std::vector<int> arbiter_assignment;
  std::vector<int> arbiter_variables;
  std::vector<int> existential_variables;
  std::vector<int> universal_variables;
  std::unordered_set<int> arbiter_variables_set;
  std::set<int> undefined_variables;
  std::set<int> variables_to_check;
  std::set<int> variables_recently_forced;
  std::unordered_set<int> universal_variables_set;
  CadicalSolver arbiter_solver, forced_solver;
  std::unordered_map<int, std::vector<int>> dependency_map, extended_dependency_map;
  std::unordered_map<int, std::unordered_set<int>> dependency_map_set, extended_dependency_map_set;
  std::vector<Clause> matrix;
  DefinabilityChecker definabilitychecker;
  std::unique_ptr<UnateChecker> unate_checker;
  bool use_random_defaults;
  Configuration config;

  struct SolverStats {
    unsigned int arbiters_introduced = 0;
    unsigned int conflicts = 0;
    unsigned int forcing_clauses = 0;
    unsigned int arbiter_clauses = 0;
    unsigned int defined = 0;
    unsigned int unates = 0;
  } solver_stats;

  SimpleValidityChecker validitychecker;
  SkolemContainer skolemcontainer;

};

// Implementation of inline methods

inline int Solver::newVariable() {
  return ++last_used_variable;
}

}

#endif // PEDANT_SOLVER_H_