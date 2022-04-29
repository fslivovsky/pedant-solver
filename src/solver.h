#ifndef PEDANT_SOLVER_H_
#define PEDANT_SOLVER_H_

#include <set>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <memory>

#include "solvertypes.h"
#include "inputformula.h"
#include "satsolver.h"
#include "definabilitychecker.h"
#include "simplevaliditychecker.h"
#include "skolemcontainer.h"
#include "utils.h"
#include "unatechecker.h"
#include "configuration.h"
#include "interrupt.h"
#include "dependencycontainer.h"
#include "solverdata.h"


namespace pedant {

class Solver {
 public:
  //No guarantee for the state of formula after the constructor application is given.
  //formula may only contain definitions for innermost existential variables
  Solver( InputFormula& formula, Configuration& config);
  int solve();
  int newVariable();
  void printStatistics();


 private:
  bool checkArbiterAssignment();
  bool analyzeConflict( const std::vector<int>& failed_existentials, const std::vector<int>& failed_universals, 
                        const std::vector<int>& failed_arbiters, const std::vector<int>& complete_universal_assignment, 
                        const std::vector<int>& complete_existential_assignment);
  void analyzeForcingConflict(int forced_literal, const std::vector<int>& failed_existentials, const std::vector<int>& failed_universals, const std::vector<int>& failed_arbiters);
  bool findArbiterAssignment();
  std::tuple<Clause, bool> getForcingClause(int literal, const std::vector<int>& failed_existentials, const std::vector<int>& failed_universals, const std::vector<int>& failed_arbiters);
  void addForcingClause(Clause& forcing_clause, bool reduced);
  template<typename T> void checkDefined(T variables_to_check, const std::vector<int>& assumptions, bool use_extended_dependencies, int conflict_limit);
  void addDefinition(int variable, std::vector<Clause>& definition, const std::vector<std::tuple<std::vector<int>,int>>& definition_circuit, std::vector<int>& conflict, bool reduced = false);
  std::tuple<int, bool> getArbiter(int existential_literal, const std::vector<int>& complete_universal_assignment, bool introduce_clauses);
  void checkUnates();
  std::tuple<bool, int> hasForcingClause(const std::vector<int>& existential_assignment);
  void setRandomDefaultValues();
  void forcingClausesFromMatrix();
  void setDefaultValuesFromResponse(const std::vector<int>& universal_assignment, const std::vector<int>& arbiter_assignment);
  void insertIntoDefaultContainer(int existential_literal, const std::vector<int>& universal_assignment, const std::vector<int>& existential_assignment);
  


  std::vector<Clause> getDefinition(int var, const std::vector<int>& dependencies, std::vector<int>& conflict);

  void updateDynamicDependencies(int var, std::set<int>& support_set, std::set<int>& updated_variables);

  void processInnermostExistentials(int start_index_block, int end_index_block);
  void processGivenDefinitions(std::unordered_map<int, std::tuple<std::vector<Clause>, Circuit>>& definitions);

  int last_used_variable;
  SolverData shared_data;
  std::vector<int> arbiter_assignment;
  std::vector<int> arbiter_variables;
  std::unordered_map<int, int> arbiter_to_index;
  std::vector<int> existential_variables;
  std::vector<int> innermost_existentials;
  std::vector<int> universal_variables;
  std::set<int> undefined_variables;
  std::set<int> variables_to_check;
  std::set<int> variables_recently_forced;
  std::unordered_set<int> universal_variables_set;
  std::shared_ptr<SatSolver> arbiter_solver;
  DependencyContainer dependencies;
  std::vector<Clause> matrix;
  DefinabilityChecker definabilitychecker;
  std::unique_ptr<UnateChecker> unate_checker;
  Configuration config;
  std::unordered_map<int, std::vector<int>> variable_to_forcing_common;
  SimpleValidityChecker validitychecker;
  SkolemContainer skolemcontainer;
  std::unordered_map<int, int> arbiter_counts;
  bool preprocessing_done;
  std::set<int> variables_defined_by_universals;

  struct SolverStats {
    unsigned int arbiters_introduced = 0;
    unsigned int conflicts = 0;
    unsigned int linear_conflicts = 0;
    unsigned int sum_forcing_clause_lengths = 0;
    unsigned int forcing_clauses = 0;
    unsigned int conditional_definitions = 0;
    unsigned int arbiter_clauses = 0;
    unsigned int defined = 0;
    unsigned int unates = 0;
    unsigned int existential_conflict_literals = 0;
    unsigned int universal_conflict_literals = 0; 
    unsigned int arbiter_conflict_literals = 0;
  } solver_stats;

};

}

#endif // PEDANT_SOLVER_H_