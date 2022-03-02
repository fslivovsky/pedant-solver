#include "supporttracker.h"

#include <algorithm>
#include <assert.h>
#include <memory>

#include "graphSeparator.h"

//visualisation
#include <fstream>
#include <string>


namespace pedant {


SupportTracker::SupportTracker(const std::vector<int>& universal_variables,
    const DependencyContainer& dependencies, 
    int& last_used_variable, const Configuration& config) : 
    universal_variables(universal_variables.begin(), universal_variables.end()), 
    dependencies(dependencies),
    last_used_variable(last_used_variable), config(config) {
}

int SupportTracker::getForcedSource(const std::vector<int>& literals, bool replace_initial) {
  std::vector<int> existential_support;
  std::vector<int> arbiter_support;
  std::set<int> universal_support;
  std::unordered_set<int> seen;
  for (auto l: literals) {
    auto v = var(fromAlias(l));
    if (universal_variables.find(v) != universal_variables.end()) {
      universal_support.insert(v);
    } else if (arbiter_variables.find(v) != arbiter_variables.end()) {
      arbiter_support.push_back(v);
    } else {
      // Existential variable.
      if (replace_initial) { // For use in the consistency checker. There, variables = {e, -e} for some existential variable e, and we first replace these by their active support sets.
        insertSupport(v, existential_support, seen, universal_support, arbiter_support);
      } else {
        existential_support.push_back(v);
      }
    }
  }
  std::vector<int> existential_sources;
  while (!existential_support.empty()) {
    auto last_existential = existential_support.back();
    existential_support.pop_back();
    if (seen.find(last_existential) == seen.end()) {
      seen.insert(last_existential);
      if (isForced(last_existential)) {
        insertSupport(last_existential, existential_support, seen, universal_support, arbiter_support);
      } else {
        existential_sources.push_back(last_existential);
      }
    }
  }
  std::vector<int> universal_support_vector(universal_support.begin(), universal_support.end());
  DLOG(trace) << "Existential sources: " << existential_sources << ", universal: " << universal_support_vector << ", arbiters: " <<  arbiter_support << std::endl;
  auto [has_forcing_clause, _] = hasForcingClause(existential_sources);
  DLOG(trace) << "Forcing clause exists? " << has_forcing_clause << std::endl;
  auto forced_variable = has_forcing_clause ? existential_sources.back() : 0;
  DLOG(trace) << "Forced variable: " << forced_variable << std::endl;
  return forced_variable;
}


std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> SupportTracker::getActiveRuleSupport(int variable) {
  auto activity_variables = variable_to_activity_variables[variable];
  int i;
  for (i = 0; i < activity_variables.size() && solver->val(activity_variables[i]) < 0; i++);
  assert(i < activity_variables.size());
  auto av = activity_variables[i];
  auto& true_activity_variable_universal_support = activity_variable_to_universal_support[av];
  auto& true_activity_variable_existential_support = activity_variable_to_existential_support[av];
  auto& true_activity_variable_arbiter_support = activity_variable_to_arbiter_support[av];
  return std::make_tuple(true_activity_variable_existential_support, true_activity_variable_universal_support, true_activity_variable_arbiter_support);
}

std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> SupportTracker::getAssertingSupport(const std::vector<int>& literals, bool replace_initial) {
  std::vector<int> existential_support;
  std::vector<int> arbiter_support;
  std::set<int> universal_support;
  std::unordered_set<int> seen;
  for (auto l: literals) {
    auto v = var(fromAlias(l));
    if (universal_variables.find(v) != universal_variables.end()) {
      universal_support.insert(v);
    } else if (arbiter_variables.find(v) != arbiter_variables.end()) {
      arbiter_support.push_back(v);
    } else {
      // Existential variable.
      if (replace_initial) { // For use in the consistency checker. There, variables = {e, -e} for some existential variable e, and we first replace these by their active support sets.
        insertSupport(v, existential_support, seen, universal_support, arbiter_support);
      } else {
        existential_support.push_back(v);
      }
    }
  }
  while (!existential_support.empty() && arbiter_support.empty()) {
    auto [has_forcing_clause, other_max_variable_index] = hasForcingClause(existential_support);
    auto max_variable = existential_support.back();
    if (has_forcing_clause) {
      if (!isForced(max_variable)) {
        break;
      }
    } else { 
      // No forcing clause, max_variable and existential_support.back() are incomparable existentials.
      // See if we can get rid of one of these existentials.
      auto other_max_variable = existential_support[other_max_variable_index];
      if (!isForced(max_variable)) {
        // Try the other variable.
        if (isForced(other_max_variable)) {
          existential_support[other_max_variable_index] = max_variable;
          existential_support.back() = other_max_variable;
        } else {
          // Two (at least) incomparable existentials and we cannot get rid of either of them.
          break;
        }
      }
    }
    // Replace existential_support.back() by support.
    auto variable_to_replace = existential_support.back();
    existential_support.pop_back();
    insertSupport(variable_to_replace, existential_support, seen, universal_support, arbiter_support);
  }
  std::vector<int> universal_support_vector(universal_support.begin(), universal_support.end());
  return std::make_tuple(existential_support, universal_support_vector, arbiter_support);
}

void removeDuplicates(std::vector<int>& v) {
  std::unordered_set<int> s;
  auto end = std::remove_if(v.begin(), v.end(),
      [&s](int const &i) { return !s.insert(i).second;});
  v.erase(end, v.end());
}

std::vector<int> SupportTracker::processSupport(int existential_variable, std::queue<int>& existentials_to_process, 
      std::unordered_set<int>& existential_variables_seen, std::set<int>& universals, std::set<int>& arbiters) {
  if (existential_variables_seen.find(existential_variable) != existential_variables_seen.end()) {
    return {};
  }
  std::vector<int> connected;
  existential_variables_seen.insert(existential_variable);
  auto activity_variables = variable_to_activity_variables[existential_variable];
  for (auto av: activity_variables) {
    if (solver->val(av) > 0) {
      // Add universal and arbiter variables to support.
      auto& true_activity_variable_universal_support = activity_variable_to_universal_support[av];
      universals.insert(true_activity_variable_universal_support.begin(),true_activity_variable_universal_support.end());
      connected.insert(connected.end(), true_activity_variable_universal_support.begin(),true_activity_variable_universal_support.end());

      auto& true_activity_variable_arbiter_support = activity_variable_to_arbiter_support[av];
      arbiters.insert(true_activity_variable_arbiter_support.begin(),true_activity_variable_arbiter_support.end());
      connected.insert(connected.end(),true_activity_variable_arbiter_support.begin(),true_activity_variable_arbiter_support.end());

      auto& true_activity_variable_existential_support = activity_variable_to_existential_support[av];
      for (auto e: true_activity_variable_existential_support) {
        if (solver->val(e) > 0 || getAlias(e) == e) {
          connected.push_back(e);
          if (existential_variables_seen.find(e) == existential_variables_seen.end()) {
            existentials_to_process.push(e);
          }
        }
      }
      break;
    }
  }
  return connected;
}

void SupportTracker::addEdges(std::vector<std::pair<int,int>>& edges, int target, const std::vector<int>& sources, int forbidden_target) {
  for (int s : sources) {
    if (s!=forbidden_target && target!=s) {
      edges.push_back(std::make_pair(s,target));
    }
  }
}


std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> SupportTracker::filterLiterals(const std::vector<int>& literals) const {
  std::vector<int> universals, existentials, arbiters;
  for (int l : literals) {
    int v = var(l);
    if (universal_variables.find(v)!=universal_variables.end()) {
      universals.push_back(l);
    } else if (arbiter_variables.find(v)!=arbiter_variables.end()) {
      arbiters.push_back(l);
    } else { //existential
      existentials.push_back(l);
    }
  }
  return std::make_tuple(existentials,universals,arbiters);
}


std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> SupportTracker::getMinimalSeparator(const std::vector<int>& literals, int forced_variable, bool replace_initial) {
  assert (!replace_initial || (literals.size()==2 && literals[0] == -literals[1]));
  std::vector<int> vertices, source_vertices, sink_vertices;
  sink_vertices.reserve(literals.size());
  std::vector<std::pair<int,int>> edges;
  std::set<int> universals, arbiters;
  //by setting the variable to seen we do not follow the outgoing edges.
  std::unordered_set<int> seen {forced_variable};
  //If true, do not allow certain variables in the separator.
  bool forbid_variables = false;
  int variable_to_include = forced_variable;
  if (variable_to_include != 0) {
    forbid_variables = true;
  } else  {
    forbid_variables = false;
  }
  std::vector<int> forbidden_variables;
  std::queue<int> existentials_to_process;
  for (auto l: literals) {
    auto v = var(fromAlias(l));
    if (v == variable_to_include) {
      continue;
    }
    sink_vertices.push_back(v);
    if (universal_variables.find(v) != universal_variables.end()) {
      universals.insert(v);
    } else if (arbiter_variables.find(v) != arbiter_variables.end()) {
      arbiters.insert(v);
    } else { // Existential variable.
      if (!isForced(v)) { 
        source_vertices.push_back(v);
      } else if (replace_initial) { 
        // For use in the consistency checker. There we have variables = {e, -e} for some existential variable e, and we first replace these by their active support sets.
        auto sources = processSupport(v, existentials_to_process, seen, universals, arbiters);
        addEdges(edges,v,sources,variable_to_include);
        vertices.push_back(v);
        if (forbid_variables) {
          forbidden_variables.push_back(v);
        }
      } else {
        existentials_to_process.push(v);
      }
    }
  } 
  while (!existentials_to_process.empty()) {
    int existential = existentials_to_process.front();
    existentials_to_process.pop();
    if (seen.find(existential) != seen.end()) {
      continue;
    }
    
    if (forbid_variables && !dependencies.containedInExtendedDependencies(var(getAlias(existential)), var(getAlias(variable_to_include)))) { 
      forbidden_variables.push_back(existential);
    }
    if (!isForced(existential)) { 
      source_vertices.push_back(existential);
    } else {
      vertices.push_back(existential);
      auto sources = processSupport(existential, existentials_to_process, seen, universals, arbiters);
      addEdges(edges,existential,sources,variable_to_include);
    }
  }


  removeDuplicates(source_vertices);

  source_vertices.insert(source_vertices.end(),universals.begin(),universals.end());
  source_vertices.insert(source_vertices.end(),arbiters.begin(),arbiters.end());

  //If no variable is forced do not compute a separator and just return all the "terminal variables"
  if (forced_variable == 0) {
    return filterLiterals(source_vertices);
  }

  /**
   * If replace_initial is true then literals has the shape x, -x.
   * There can only be an inconsistency in x if there are active rules both for x and -x.
   * This means that x cannot be the forced variable. 
   * Moreover, in such a case we do not want to introduce a forcing clause for x.
   * (Such a forcing clause would not repair the conflict and thus the same conflict could occur in
   * the next iteration). In order to ensure that we do not get a forcing clause for x, x
   * shall not be in the separator. In subsequent steps of the algorithm we may remove variables
   * from the separator before we introduce a forcing clause but we do not add new variables.
   * Thus if x is not part of the separator, we will not get a forcing clause for x.
   * If we use "standard" extended dependencies (i.e. not dynamic extended dependencies) then
   * the variables representing x and -x will be forbidden and thus the will not be part of the separator.
   * The reason for this is the following: The forced variable must be part of some rule implying
   * some literal l. Now ED(forced) < ED(var(l)) otherwise forced could not be part of the rule.
   * As the extended dependencies are increasing along the conflict graph we have ED(forced)<ED(x).
   * Moreover, we have x\notin ED(x). Thus we have x\notin ED(forced).
   * If we use dynamic extended dependencies x\notin DED(x) does not necessary hold.
   * Thus, we add the representations of x and -x to the forbidden variables.
   **/
  if (replace_initial) {
    forbidden_variables.push_back(fromAlias(literals[0]));
    forbidden_variables.push_back(fromAlias(literals[1]));
  }

  vertices.insert(vertices.end(),source_vertices.begin(),source_vertices.end());

  std::vector<int> sep = GraphSeparator::getVertexSeparator(vertices, edges, source_vertices, sink_vertices, forbidden_variables);

  sep.push_back(variable_to_include);

  return filterLiterals(sep);
}


void SupportTracker::insertSupport(int existential_variable, std::vector<int>& existential_support, std::unordered_set<int>& existential_variables_seen, std::set<int>& universal_support, std::vector<int>& arbiter_support) {
  existential_variables_seen.insert(existential_variable);
  auto activity_variables = variable_to_activity_variables[existential_variable];
  for (auto av: activity_variables) {
    if (solver->val(av) > 0) {
      // Add universal and arbiter variables to support.
      auto& true_activity_variable_universal_support = activity_variable_to_universal_support[av];
      universal_support.insert(true_activity_variable_universal_support.begin(), true_activity_variable_universal_support.end());
      arbiter_support.insert(arbiter_support.end(), activity_variable_to_arbiter_support[av].begin(), activity_variable_to_arbiter_support[av].end());
      auto& true_activity_variable_existential_support = activity_variable_to_existential_support[av];
      for (auto e: true_activity_variable_existential_support) {
        if (existential_variables_seen.find(e) == existential_variables_seen.end() && (solver->val(e) > 0 || getAlias(e) == e)) { // The last condition is an ugly hack to ensure this works both in the validity and consistency checker.
          existential_support.push_back(e);
        }
      }
      break;
    }
  }
}

std::tuple<bool, int> SupportTracker::hasForcingClause(std::vector<int>& assigned_existential_variables) {
  if (assigned_existential_variables.empty()) {
    return std::make_tuple(false, 0);
  }
  int max_existential_index = -1;
  for (int i = 0; i < assigned_existential_variables.size(); i++) {
    auto v = var(getAlias(assigned_existential_variables[i]));
    auto v_max = var(getAlias(assigned_existential_variables[max_existential_index]));
    if (max_existential_index == -1 || dependencies.largerExtendedDependencies(v, v_max)) {
      max_existential_index = i;
    }
  }
  auto max_existential = assigned_existential_variables[max_existential_index];
  assigned_existential_variables[max_existential_index] = assigned_existential_variables.back();
  assigned_existential_variables.back() = max_existential;
  for (int i = 0; i < assigned_existential_variables.size() - 1; i++) {
    auto v = var(getAlias(assigned_existential_variables[i]));
    if (!dependencies.containedInExtendedDependencies(v,var(getAlias(max_existential)))) {
      return std::make_tuple(false, i);
    }
  }
  return std::make_tuple(true, assigned_existential_variables.size() - 1);
}

/**
 * Debug Method
 * box arbiter variable
 * diamond universal variable
 * ellipse existential variable
 * green border in separator
 * blue filling forced variable
 * red filling: the existential is not in the extended dependencies of the forced variable
 **/
void SupportTracker::visualiseGraph(const std::vector<int>& literals, const std::vector<int>& mark, int forced_variable, bool replace_initial, bool invert_graph) {
  std::unordered_set<int> to_mark (mark.begin(), mark.end());
  std::string fname = "test.dot";
  std::ofstream out(fname);
  out << "digraph test {\n";
  std::unordered_set<int> existentials;
  std::set<int> existential_terminals;
  std::set<int> universals, arbiters;
  int sink = last_used_variable + 1;
  out << sink << "[label=conflict, shape=hexagon];\n";
  // out << last_used_variable + 2 << "[label=sink, shape=ellipse];\n"
  for (int lit : literals) {
    auto v = var(fromAlias(lit));
    if (invert_graph) {
      out << sink << " -> " << v << ";\n";
    } else {
      out << v << " -> " << sink << ";\n";
    }
    if (universal_variables.find(v) != universal_variables.end()) {
      universals.insert(v);
    } else if (arbiter_variables.find(v) != arbiter_variables.end()) {
      arbiters.insert(v);
    } else {
      addEdgesToVisualisation(v, existentials, existential_terminals, universals, arbiters, out, invert_graph);
    }
  }



  for (auto u : universals) {
    out << u << "[shape=diamond";
    if (to_mark.find(u)!=to_mark.end()) {
      out << ", color=green, penwidth=4";
    } 
    out<<", style=filled, fillcolor=snow3";
    out << "];\n";
  }
  for (auto a : arbiters) {
    out << a << "[shape=box";
    if (to_mark.find(a)!=to_mark.end()) {
      out << ", color=green, penwidth=4";
    } 
    out<<", style=filled, fillcolor=snow3";
    out << "];\n";
  }
  for (auto e : existentials) {
    out << e << "[shape=ellipse";
    if (e==forced_variable) {
      out<<", style=filled, fillcolor=deepskyblue";
      if (to_mark.find(e)!=to_mark.end()) {
        out << ", color=green, penwidth=4";
      }
    //every existential terminal that is different from the forced variable must occur in the extended dependencies of the forced variable
    } else if (existential_terminals.find(e) != existential_terminals.end()) {
      out<<", style=filled, fillcolor=snow3";
      if (to_mark.find(e)!=to_mark.end()) {
        out << ", color=green, penwidth=4";
      }
    }
    else {
      if (forced_variable != 0 && !dependencies.containedInExtendedDependencies(var(getAlias(e)), var(getAlias(forced_variable)))) {
        out << ", style=filled, fillcolor=red";
      } 
      if (to_mark.find(e)!=to_mark.end()) {
        out << ", color=green, penwidth=4";
      }
    }
    out << "];\n";
  }

  out << "}";
  out.close();
}

void SupportTracker::addEdgesToVisualisation(int variable, std::unordered_set<int>& existentials, std::set<int>& existential_terminals, std::set<int>& universals, std::set<int>& arbiters, std::ostream& out, bool invert) {
  std::queue<int> existentials_to_process;
  if (!isForced(variable)) {
    existential_terminals.insert(variable);
  }
  auto targets = processSupport(variable, existentials_to_process, existentials, universals, arbiters);
  for (auto t : targets) {
    if (invert) {
      out << variable << " -> " << t << ";\n";
    } else {
      out << t << " -> " << variable << ";\n";
    }
  }
  while (!existentials_to_process.empty()){
    auto e = existentials_to_process.front();
    existentials_to_process.pop();
    addEdgesToVisualisation(e, existentials, existential_terminals, universals, arbiters, out, invert);
  }
}

}
