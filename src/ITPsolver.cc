#include "ITPsolver.h"

#include <algorithm>
#include <memory>
#include <iostream>

#include "utils.h"
#include "logging.h"

namespace pedant {

ITPSolver::ITPSolver(std::vector<Clause>& _first_part, std::vector<Clause>& _second_part): first_part(miniSATClauses(_first_part)), second_part(miniSATClauses(_second_part)) {
  DLOG(trace) << "First part: "  << first_part  << std::endl
              << "Second part: " << second_part << std::endl;
  max_var_index = std::max(maxVarIndex(first_part), maxVarIndex(second_part));
  interpolatingsolver = std::make_unique<InterpolatingSolver>(max_var_index);
  interpolatingsolver->addFormula(first_part, second_part);
}

void ITPSolver::resetSolver() {
  interpolatingsolver->resetSolver(max_var_index);
  interpolatingsolver->addFormula(first_part, second_part);
}

bool ITPSolver::addClause(Clause& clause, bool add_to_first_part) {
  auto clause_max_variable_ptr = std::max_element(clause.begin(), clause.end(),
    [] (int const& l1, int const& l2) {
      return var(l1) < var(l2);
    });
  auto clause_max_variable = clause_max_variable_ptr != clause.end() ? var(*clause_max_variable_ptr) : 0;
  if (clause_max_variable > max_var_index) {
    DLOG(trace) << "ITPSolver new max. variable: " << clause_max_variable << std::endl;
    interpolatingsolver->reserve(clause_max_variable);
    max_var_index = clause_max_variable;
  }
  auto clause_minisat = miniSATLiterals(clause);
  DLOG(trace) << "Adding miniSAT clause to part " << add_to_first_part << " " << clause_minisat << std::endl;
  if (add_to_first_part) {
    first_part.push_back(clause_minisat);
    return interpolatingsolver->addClause(clause_minisat, 1);
  } else {
    second_part.push_back(clause_minisat);
    return interpolatingsolver->addClause(clause_minisat, 2);
  }
}

bool ITPSolver::solve(std::vector<int>& assumptions, int limit) {
  auto minisat_assumptions = miniSATLiterals(assumptions);
  return interpolatingsolver->solve(minisat_assumptions, limit);
}

std::vector<int> ITPSolver::getConflict() {
  auto minisat_literals = interpolatingsolver->getConflict();
  return literalsFromMiniSATLiterals(minisat_literals);
}

std::tuple<std::vector<std::tuple<std::vector<int>,int>>, std::vector<int>> ITPSolver::getDefinition(const std::vector<int>& input_variable_ids, int output_variable_id, int offset, bool compress) {
  return interpolatingsolver->getDefinition(input_variable_ids, output_variable_id, compress, std::max(offset, max_var_index));
}

}