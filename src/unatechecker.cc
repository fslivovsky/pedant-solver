#include "unatechecker.h"
#include "utils.h"
#include <cassert>
#include <unordered_map>
#include "solver_generator.h"

namespace pedant {

UnateChecker::UnateChecker(std::vector<Clause>& matrix,
    const std::vector<int>& existentials,
    int& last_used_variable, const Configuration& config) 
    : max_variable(last_used_variable),matrix(matrix),existential_variables(existentials), config(config) {
  unate_solver = giveSolverInstance(config.unate_solver);
  auto [negated_matrix, _] = negateFormula(matrix,max_variable);

  std::sort(existential_variables.begin(),existential_variables.end());
  renamed_existentials.reserve(existential_variables.size());
  switches_for_equalities.reserve(existential_variables.size());

  for (int i=0;i<existential_variables.size();i++) {
    renamed_existentials.push_back(getNewVariable());
    switches_for_equalities.push_back(getNewVariable());
    auto equalities=clausalEncodingEquality(existential_variables[i],renamed_existentials[i],switches_for_equalities[i]);
    equalities_for_renamings.insert(equalities_for_renamings.end(),equalities.begin(),equalities.end());
  }

  std::unordered_map<int,int> renaming;
  for (int i=0;i<existential_variables.size();i++) {
    renaming.insert(std::make_pair(existential_variables[i],renamed_existentials[i]));
  }
  auto renamed_negated_matrix=renameFormula(negated_matrix,renaming);

  unate_solver->appendFormula(matrix);
  unate_solver->appendFormula(renamed_negated_matrix);
  unate_solver->appendFormula(equalities_for_renamings);

}

std::vector<Clause> UnateChecker::findUnates(const std::vector<int>& variables_to_consider, std::vector<int>& arbiter_assignment) {

  int old_last_index_to_consider;
  int last_index_to_consider=variables_to_consider.size();;
  std::vector<int> indices;
  indices.reserve(variables_to_consider.size());

  std::vector<Clause> new_unate_clauses;
  std::vector<int> found_unate_literals;

  for (int v:variables_to_consider) {
    assert (v<=existential_variables.back());
    int index=std::lower_bound(existential_variables.begin(),existential_variables.end(),v)-existential_variables.begin();
    assert (existential_variables[index]==v);
    indices.push_back(index);
  }

  do {
    old_last_index_to_consider=last_index_to_consider;

    //In each iteration of the for loop the ith equality has to be disabled
    //In order to efficently disable the equality, we swap the switch with the last element and pop it.
    //Finally we push the switch back and move it to its original position
    std::vector<int> assumptions(arbiter_assignment.begin(),arbiter_assignment.end());
    assumptions.insert(assumptions.end(),switches_for_equalities.begin(),switches_for_equalities.end());
    assumptions.insert(assumptions.end(),found_unate_literals.begin(),found_unate_literals.end());

    for (int i=0;i<last_index_to_consider;i++) {
      // int var=variables_to_consider[i];
      int index=indices[i];
      int variable=existential_variables[index];

      int deactivated_switch=switches_for_equalities[index];

      //At this index the vector must contain the value of "deactivated_switch".
      //For this iteration we do not want to have this value in the assumptions.
      assumptions[index+arbiter_assignment.size()]=assumptions.back();
      assumptions.pop_back();

      int unate_literal;
      if (checkUnateLiteral(-variable,renamed_existentials[index],assumptions)) {
        unate_literal=variable;
      } else if (checkUnateLiteral(variable,-renamed_existentials[index],assumptions)) {
        unate_literal=-variable;
      } else {
        assumptions.push_back(assumptions[index+arbiter_assignment.size()]);
        assumptions[index+arbiter_assignment.size()]=deactivated_switch;
        continue;
      }

      auto conflict = unate_solver->getFailed(arbiter_assignment);
      Clause unate_clause {unate_literal};
      unate_clause.reserve(conflict.size()+1);

      for (int l:conflict) {
        unate_clause.push_back(-l);
      }

      DLOG(trace) << "Found unate clause: " << unate_clause << std::endl;

      unate_clauses.push_back(unate_clause);
      new_unate_clauses.push_back(unate_clause);

      //In this case the variable var does not need to be considered in further iterations of the do-while loop
      //By moving the index to the end of indices and reducing the counter that represents the last index of indices
      //we ensure that this variable is not visited again
      indices[i]=indices.back();
      indices.pop_back();

      found_unate_literals.push_back(unate_literal);
      assumptions.push_back(assumptions[index+arbiter_assignment.size()]);
      assumptions[index+arbiter_assignment.size()]=deactivated_switch;
      assumptions.push_back(unate_literal);
      last_index_to_consider--;
      i--;

    }

  } while (old_last_index_to_consider != last_index_to_consider);

  return new_unate_clauses;

}

bool UnateChecker::checkUnateLiteral(int literal,int copied_literal, std::vector<int>& assumptions) {
  assumptions.push_back(literal);
  assumptions.push_back(copied_literal);

  unate_solver->assume(assumptions);
  // bool result = (unate_solver->solve(2000) == 20); // Make conflict limit configurable.
  bool result = (config.use_conflict_limit_unate_solver ? unate_solver->solve(config.conflict_limit_unate_solver) : unate_solver->solve()) == 20;

  assumptions.pop_back();
  assumptions.pop_back();

  return result;
}

}