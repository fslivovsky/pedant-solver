#ifndef PEDANT_SUPPORTTRACKER_H_
#define PEDANT_SUPPORTTRACKER_H_

#include <memory>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>
#include <queue>

#include <assert.h>

#include "satsolver.h"
#include "solvertypes.h"
#include "utils.h"
#include "logging.h"
#include "configuration.h"
#include "dependencycontainer.h"



namespace pedant {

class SupportTracker {
 public:
  SupportTracker(const std::vector<int>& universal_variables, const DependencyContainer& dependencies, int& last_used_variable, const Configuration& config);
  void setSatSolver(const std::shared_ptr<SatSolver> solver_);
  void addArbiterVariable(int arbiter_variable);
  void setNoForcedVariable(int variable, int no_forcing_clause_active_variable);
  void addDefiningClause(int variable, const Clause& defining_clause, int activity_variable);
  int getForcedSource(const std::vector<int>& literals, bool replace_initial=false);
  std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> getActiveRuleSupport(int variable);
  std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> getAssertingSupport(const std::vector<int>& literals, bool replace_initial=false);
  /**
   * If replace_initial==true, then literals must have the shape {e,-e}
   * If forced_variable!=0 then the last element of the first component of @return is forced_variable
   **/
  std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> getMinimalSeparator(const std::vector<int>& literals, int forced_variable, bool replace_initial=false);
  void setAlias(int variable, int alias);
  std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> computeSupport(const std::vector<int>& literals, int forced_variable, bool replace_initial=false);

  void visualiseGraph(const std::vector<int>& literals, const std::vector<int>& mark, int forced_variable, bool replace_initial=false, bool invert_graph=false);

 private:
  void printSupportEdges(int variable, const std::vector<int>& support);
  int getAlias(int variable) const;
  int fromAlias(int alias) const;
  std::tuple<bool, int> hasForcingClause(std::vector<int>& assigned_existential_variables);
  void insertSupport(int existential_variable, std::vector<int>& existential_support, std::unordered_set<int>& existential_variables_seen, std::set<int>& universal_support, std::vector<int>& arbiter_support);
  bool isForced(int variable);
  int getConnVar(int variable);
  int getSepVar(int variable);
  int getClVar(int variable);

  //Auxiliary functions for computing the separator
  // std::vector<int> processSupport(int existential_variable, std::vector<int>& existentials_to_process, 
  std::vector<int> processSupport(int existential_variable, std::queue<int>& existentials_to_process, 
      std::unordered_set<int>& existential_variables_seen, std::set<int>& universals, std::set<int>& arbiters);
  void addEdges(std::vector<std::pair<int,int>>& edges, int source, const std::vector<int>& targets, int forbidden_target);
  std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> filterLiterals(const std::vector<int>& literals) const;

  void addEdgesToVisualisation(int variable, std::unordered_set<int>& existentials, std::set<int>& existential_terminals, std::set<int>& universals, std::set<int>& arbiters, std::ostream& out, bool invert=false);

  std::shared_ptr<SatSolver> solver;
  std::unordered_set<int> universal_variables;
  std::unordered_set<int> arbiter_variables;
  std::unordered_map<int, int> variable_to_no_forcing_clause_active_variable;
  std::unordered_map<int, std::vector<int>> variable_to_activity_variables;
  std::unordered_map<int, std::vector<int>> activity_variable_to_existential_support;
  std::unordered_map<int, std::vector<int>> activity_variable_to_universal_support;
  std::unordered_map<int, std::vector<int>> activity_variable_to_arbiter_support;

  std::unordered_map<int, int> variable_to_alias;
  std::unordered_map<int, int> alias_to_variable;
  const DependencyContainer& dependencies;
  
  std::unordered_map<int, int> conn_var;
  std::unordered_map<int, int> sep_var;
  std::unordered_map<int, int> cl_var;
  int& last_used_variable;
  int max_sat_encoding_variable = 1;
  const Configuration& config;

};

// Definition of inline methods.

inline void SupportTracker::setSatSolver(const std::shared_ptr<SatSolver> solver_) {
  solver = solver_;
}

inline void SupportTracker::addArbiterVariable(int arbiter_variable) {
  arbiter_variables.insert(arbiter_variable);
}

inline void SupportTracker::setNoForcedVariable(int variable, int no_forcing_clause_active_variable) {
  variable_to_no_forcing_clause_active_variable[variable] = no_forcing_clause_active_variable;
}

inline void SupportTracker::addDefiningClause(int variable, const Clause& defining_clause, int activity_variable) {
  variable_to_activity_variables[variable].push_back(activity_variable);
  for (int l: defining_clause) {
    auto v = var(l);
    if (variable_to_no_forcing_clause_active_variable.find(v) != variable_to_no_forcing_clause_active_variable.end()) {
      activity_variable_to_existential_support[activity_variable].push_back(v);
    } else if (universal_variables.find(v) != universal_variables.end()) {
      activity_variable_to_universal_support[activity_variable].push_back(v);
    } else if (arbiter_variables.find(v) != arbiter_variables.end()) {
      activity_variable_to_arbiter_support[activity_variable].push_back(v);
    }
  }
}

inline void SupportTracker::printSupportEdges(int variable, const std::vector<int>& support) {
  for (int v: support) {
    //DLOG(trace) << getAlias(v) << " -> " << getAlias(variable) << ";" << std::endl;
  }
}

inline void SupportTracker::setAlias(int variable, int alias) {
  variable_to_alias[variable] = alias;
  alias_to_variable[alias] = variable;
}

inline int SupportTracker::getAlias(int variable) const {
  return variable_to_alias.find(variable) != variable_to_alias.end() ? variable_to_alias.at(variable) : variable;
}

inline int SupportTracker::fromAlias(int alias) const {
  return alias_to_variable.find(alias) != alias_to_variable.end() ? alias_to_variable.at(alias) : alias;
}

inline bool SupportTracker::isForced(int variable) {
  return solver->val(variable_to_no_forcing_clause_active_variable[variable]) < 0;
}

inline int SupportTracker::getConnVar(int variable) {
  if (conn_var.find(variable) == conn_var.end()) {
    conn_var[variable] = ++max_sat_encoding_variable;
  }
  return conn_var[variable];
}

inline int SupportTracker::getSepVar(int variable) {
  if (sep_var.find(variable) == sep_var.end()) {
    sep_var[variable] = ++max_sat_encoding_variable;
  }
  return sep_var[variable];
}

inline int SupportTracker::getClVar(int variable) {
  if (cl_var.find(variable) == cl_var.end()) {
    cl_var[variable] = ++max_sat_encoding_variable;
  }
  return cl_var[variable];
}

inline std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> SupportTracker::computeSupport(const std::vector<int>& literals, int forced_variable, bool replace_initial) {
  return getMinimalSeparator(literals, forced_variable, replace_initial);
}

}

#endif // PEDANT_SUPPORTTRACKER_H_