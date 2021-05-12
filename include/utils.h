#ifndef PEDANT_UTILS_H_
#define PEDANT_UTILS_H_

#include <cstdlib>
#include <iostream>

#include <vector>
#include <algorithm>
#include <map>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "solvertypes.h"

namespace pedant {

// Declarations

int var(int literal);
int litFromMiniSATLit(int minisat_literal);
int miniSATLiteral(int literal);
std::vector<int> miniSATLiterals(std::vector<int>& literals);
std::vector<Clause> miniSATClauses(std::vector<Clause>& clauses);
int maxVarIndex(std::vector<Clause>& clauses);
std::vector<Clause> clausalEncodingAND(const std::tuple<std::vector<int>,int>& and_gate_definition);
int renameLiteral(int source_literal, int target_variable);
int renameLiteral(int literal, std::unordered_map<int, int>& renaming);
void setLiteralSign(int& literal, bool sign);
Clause renameClause(Clause& clause, std::unordered_map<int, int>& renaming);
std::vector<Clause> renameFormula(std::vector<Clause>& clauses, std::unordered_map<int, int>& renaming);
std::unordered_map<int, int> createRenaming(std::vector<Clause>& clauses, std::vector<int>& shared_variables, int auxiliary_start=0);
void negateEach(std::vector<int>& literals);
std::vector<Clause> negateFormula(std::vector<Clause>& clauses, int& max_used_variable);
std::vector<Clause> clausalEncodingEquality(int first_literal, int second_literal, int switch_literal);
template <class A, class B> std::vector<A> getKeys(std::unordered_map<A, B>& map);
void printVector(std::vector<int>& vec);
std::unordered_map<int, std::vector<int>> ComputeExtendedDependencies(const std::unordered_map<int, std::vector<int>>& dependency_map);
template<class T> std::vector<int> restrictTo(const std::vector<int>& literals, T& range);

// Implementations

inline int var(int literal) {
  return abs(literal);
}

inline int litFromMiniSATLit(int minisat_literal) {
  int variable = minisat_literal >> 1;
  int negated = minisat_literal & 1;
  return (variable ^ -negated) + negated;
}

inline std::vector<int> literalsFromMiniSATLiterals(std::vector<int>& literals) {
  std::vector<int> result;
  for (auto& literal: literals) {
    result.push_back(litFromMiniSATLit(literal));
  }
  return result;
}

inline int miniSATLiteral(int literal) {
  int variable = var(literal);
  return 2 * variable + (literal < 0);
}

inline std::vector<int> miniSATLiterals(std::vector<int>& literals) {
  std::vector<int> result;
  for (auto& literal: literals) {
    result.push_back(miniSATLiteral(literal));
  }
  return result;
}

inline std::vector<Clause> miniSATClauses(std::vector<Clause>& clauses) {
  std::vector<Clause> result;
  for (auto& clause: clauses) {
    result.emplace_back(miniSATLiterals(clause));
  }
  return result;
}

inline int maxVarIndex(std::vector<Clause>& clauses) {
  int maximum = 0;
  for (auto& clause: clauses) {
    for (auto& literal: clause) {
      maximum = std::max(maximum, var(literal));
    }
  }
  return maximum;
}

inline std::vector<Clause> clausalEncodingAND(const std::tuple<std::vector<int>,int>& and_gate_definition) {
  auto& [input_literals, output_literal] = and_gate_definition;
  std::vector<Clause> clauses;
  Clause positive_clause;
  for (auto& literal: input_literals) {
    clauses.push_back({literal, -output_literal});
    positive_clause.push_back(-literal);
  }
  positive_clause.push_back(output_literal);
  clauses.push_back(positive_clause);
  return clauses;
}

inline int renameLiteral(int source_literal, int target_variable) {
  bool negated = (source_literal < 0);
  return (target_variable ^ -negated) + negated;
}

inline int renameLiteral(int literal, std::unordered_map<int, int>& renaming) {
  int variable = var(literal);
  if (renaming.find(variable) == renaming.end()) {
    return literal;
  } else {
    int renamed_variable = renaming[variable];
    return renameLiteral(literal, renamed_variable);
  }
}

inline void setLiteralSign(int& literal, bool sign) {
  bool negated = !sign;
  int variable = var(literal);
  literal = (variable ^ -negated) + negated;
}

inline Clause renameClause(Clause& clause, std::unordered_map<int, int>& renaming) {
  Clause renamed_clause;
  for (auto& literal: clause) {
    renamed_clause.push_back(renameLiteral(literal, renaming));
  }
  return renamed_clause;
}

inline std::vector<Clause> renameFormula(std::vector<Clause>& clauses, std::unordered_map<int, int>& renaming) {
  std::vector<Clause> renamed_clauses;
  for (auto& clause: clauses) {
    renamed_clauses.push_back(renameClause(clause, renaming));
  }
  return renamed_clauses;
}

inline std::unordered_map<int, int> createRenaming(std::vector<Clause>& clauses, std::vector<int>& shared_variables, int auxiliary_start) {
  std::unordered_set<int> shared_variables_set(shared_variables.begin(), shared_variables.end());
  std::vector<int> non_shared_variables;
  for (auto& clause: clauses) {
    for (auto& literal: clause) {
      auto variable = var(literal);
      if (shared_variables_set.find(variable) == shared_variables_set.end()) {
        non_shared_variables.push_back(variable);
      }
    }
  }
  int variable_renamed = auxiliary_start ? auxiliary_start : maxVarIndex(clauses) + 1;
  std::unordered_map<int, int> renaming;
  for (auto& variable: non_shared_variables) {
    renaming[variable] = variable_renamed++;
  }
  return renaming;
}

inline void negateEach(std::vector<int>& literals) {
  for (auto& l: literals) {
    l = -l;
  }
}

inline std::vector<Clause> negateFormula(std::vector<Clause>& formula, int& max_used_variable) {
  std::vector<Clause> formula_negated;
  Clause not_all_clauses_satisfied;
  max_used_variable = (max_used_variable > 0) ? max_used_variable : maxVarIndex(formula);
  int& clause_variable = max_used_variable;
  for (auto& clause: formula) {
    clause_variable++;
    for (auto& literal: clause) {
      formula_negated.push_back({-literal, clause_variable});
    }
    not_all_clauses_satisfied.push_back(-clause_variable);
  }
  formula_negated.push_back(not_all_clauses_satisfied);
  return formula_negated;
}

inline std::vector<Clause> clausalEncodingEquality(int first_literal, int second_literal, int switch_literal) {
  return {{-switch_literal, -first_literal, second_literal},
          {-switch_literal, first_literal, -second_literal}};
}

template <class A, class B> std::vector<A> getKeys(std::unordered_map<A, B>& map) {
  std::vector<A> keys;
  for (auto& [key, _]: map) {
    keys.push_back(key);
  }
  std::sort(keys.begin(), keys.end());
  return keys;
}

inline void printVector(std::vector<int>& vec) {
  std::cerr << "(";
  for (auto& i: vec) {
    std::cerr << i << ",";
  }
  std::cerr << ")" << std::endl;
}

/**
 * If the dependencies of one variable e1 are contained in the dependencies of another variable e2
 * add e1 to the dependencies of e2. If two variables have the same dependencies add the variable
 * that is represented by the smaller integer to the dependencies of the other variable.
 **/
inline std::unordered_map<int, std::vector<int>> ComputeExtendedDependencies(const std::unordered_map<int, std::vector<int>>& dependency_map) {
  std::vector<std::tuple<int,std::vector<int>>> visited;
  visited.reserve(dependency_map.size());
  //The extended dependencies have to contain the original dependencies.
  std::unordered_map<int, std::vector<int>> extended_dependencies(dependency_map);
  //Iterate over the variables and their dependencies and compare the dependencies with the already
  //visited ones.
  for (const auto& [key,value]: std::map<int, std::vector<int>>(dependency_map.begin(), dependency_map.end())) {
    auto dependencies=value;
    std::sort(dependencies.begin(),dependencies.end());
    for (const auto& [var,dep]:visited) {
      if (std::includes(dependencies.begin(),dependencies.end(),dep.begin(),dep.end())) {
        if (dependencies.size()==dep.size()) {
          if (key<var) {
            extended_dependencies.at(var).push_back(key);
          } else {
            extended_dependencies.at(key).push_back(var);
          }
        } else {
          extended_dependencies.at(key).push_back(var);
        }
      } else if (std::includes(dep.begin(),dep.end(),dependencies.begin(),dependencies.end())) {
        extended_dependencies.at(var).push_back(key);
      }
    }
    visited.push_back(std::make_tuple(key,dependencies));
  }
  return extended_dependencies;
}

template<class T> std::vector<int> restrictTo(const std::vector<int>& literals, T& range) {
  std::vector<int> literals_range;
  for (const auto& l: literals) {
    auto variable = var(l);
    if (range.find(variable) != range.end()) {
      literals_range.push_back(l);
    }
  }
  return literals_range;
}

}

#endif // PEDANT_UTILS_H_