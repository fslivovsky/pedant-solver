#ifndef PEDANT_UNATECHECKER_H_
#define PEDANT_UNATECHECKER_H_

#include <vector>

#include "solvertypes.h"
#include "cadical.h"
#include "logging.h"

namespace pedant {

class UnateChecker {

 public:
  UnateChecker( std::vector<Clause>& matrix,
                const std::vector<int>& existential_variables,
                int& last_used_variable);


  std::vector<Clause> findUnates(const std::vector<int>& variables_to_consider, std::vector<int>& arbiter_assignment);

  void addClause(Clause& c);
  void addArbiterVariable(int var);

  const std::vector<Clause>& getUnateClauses() {return unate_clauses;}

 private:
  CadicalSolver unate_solver;

  //in the following vectors elements with the same index are associated to the same variable
  std::vector<int> existential_variables;
  std::vector<int> renamed_existentials;
  std::vector<int> switches_for_equalities;

  const std::vector<Clause>& matrix;
  // std::vector<Clause> negated_matrix;
  std::vector<Clause> equalities_for_renamings;
  // std::vector<Clause> arbiter_clauses;
  std::vector<Clause> unate_clauses;

  int& max_variable;


  std::vector<int> arbiter_variables;
  bool checkUnateLiteral( int literal,int copied_literal, std::vector<int>& assumptions);

  inline int getNewVariable() {return ++max_variable;}
              

};

inline void UnateChecker::addArbiterVariable(int var) {
  arbiter_variables.push_back(var);
}

inline void UnateChecker::addClause(Clause& clause) {
  DLOG(trace) << "Adding clause to unate solver: " << clause << std::endl;
  unate_solver.addClause(clause);
  // arbiter_clauses.push_back(clause);
}

}




#endif