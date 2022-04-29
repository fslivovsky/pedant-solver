#include <algorithm>
#include <assert.h>

#include "buildAIGER.h"
#include "utils.h"

#include <iostream>
#include <cassert>

namespace pedant {

AIGERBuilder::AIGERBuilder(const std::unordered_map<int,size_t>& indices, int max_var_in_matrix, 
    const std::vector<int>& existential_variables, const std::vector<int>& universal_variables) : 
    indices(indices), existential_variables(existential_variables),
    universal_variables(universal_variables) {
  intermediate_variables_start=max_var_in_matrix+1;
  last_used_intermediate_variable=max_var_in_matrix;
  circuit=aiger_init();
}

AIGERBuilder::~AIGERBuilder() {
  aiger_reset(circuit);
}

std::tuple<std::vector<Clause>,std::vector<Clause>> AIGERBuilder::filterClauses(const std::vector<Clause>& clauses) {
  std::vector<Clause> positive_clauses;
  std::vector<Clause> negative_clauses;
  for (const auto& cl: clauses) {
    Clause c (cl.begin(),cl.end()-1);
    if (cl.back()>0) {
      positive_clauses.push_back(c);
    } else {
      negative_clauses.push_back(c);
    }
  }
  return std::make_tuple(positive_clauses,negative_clauses);
}

bool AIGERBuilder::isConflictEntailed(const std::vector<int>& conflict,const std::unordered_map<int, bool>& arbiter_assignment) {
  for (int l:conflict) {
    if (arbiter_assignment.find(var(l)) != arbiter_assignment.end() && (l > 0) != arbiter_assignment.at(var(l))) {
      return false;
    }
  }
  return true;
}

std::pair<bool,Clause> AIGERBuilder::removeArbiters(const Clause& clause, const std::unordered_map<int, bool>& arbiter_assignment) {
  Clause result;
  for (int l:clause) {
    if (arbiter_assignment.find(var(l)) != arbiter_assignment.end()) {
      if ((l > 0) == arbiter_assignment.at(var(l))) {
        return std::make_pair(false,result);
      }
    } else {
      result.push_back(l);
    }
  }
  return std::make_pair(true,result);
}

void AIGERBuilder::addAdderToCircuit(int in1,int in2, int out) {
  aiger_add_and(circuit,getAIGERRepresentation(out),getAIGERRepresentation(in1),getAIGERRepresentation(in2));
}

int AIGERBuilder::combineAIGERVariables(int in1, int in2) {
  if (in1 == aiger_false || in2 == aiger_false) {
    return aiger_false;
  } 
  if (in1 == aiger_true) {
    return in2;
  }
  if (in2 == aiger_true) {
    return in1;
  }
  int out = getAIGERRepresentation(++last_used_intermediate_variable);
  aiger_add_and(circuit, out, in1, in2);
  return out;
}


int AIGERBuilder::checkVariableInDefinition(int l) {
  if (abs(l)>=intermediate_variables_start) {
    if(renamings.find(abs(l))==renamings.end()) {
      int aux=++last_used_intermediate_variable;
      renamings[abs(l)]=aux;
      return l>0?aux:-aux;
    } else {
      int aux=renamings[abs(l)];
      return l>0?aux:-aux;
    }
  }
  return l;
}

bool AIGERBuilder::addDefinitionCircuitAdder(int defined_variable, const std::vector<int>& inputs, int gate_output, std::unordered_map<int,int>& gate_renaming, const std::unordered_map<int, bool>& arbiter_assignment) {
  assert (inputs.size()<=2);
  bool is_output_gate = defined_variable == abs(gate_output);
  assert (gate_output > 0 || (is_output_gate && inputs.empty()));
  bool is_falsified = false;
  std::vector<int> gate_inputs;

  for (auto in : inputs) {
    if (arbiter_assignment.find(var(in)) != arbiter_assignment.end()) {
      // -in occurs in the assignment
      if ( (in > 0) != arbiter_assignment.at(var(in))) {
        is_falsified = true;
        break;
      }
    } else { //in does not occur in the assignment
      if (gate_renaming.find(abs(in)) != gate_renaming.end()) {
          in = in>0 ? gate_renaming.at(in) : -gate_renaming.at(in);
          if (in == aiger_true) {
            continue;
          } 
          if (in == aiger_false) {
            is_falsified = true;
            break;
          }
        } else {
          int x = checkVariableInDefinition(in);
          in = getAIGERRepresentation(x);
          gate_inputs.push_back(in);
        }
      }
    }


  if (is_output_gate) {
    if (is_falsified) {
      auto val = gate_output > 0 ? aiger_false : aiger_true;
      aiger_add_and(circuit,getAIGERRepresentation(defined_variable), val, val);
      return true;
    }
    if (gate_inputs.size() == 0) {
      auto val = gate_output > 0 ? aiger_true : aiger_false;
      aiger_add_and(circuit,getAIGERRepresentation(defined_variable), val, val);
    } else if (gate_inputs.size() == 1) {
      aiger_add_and(circuit,getAIGERRepresentation(defined_variable), gate_inputs[0], aiger_true);
    } else if (gate_inputs.size() == 2) {
      aiger_add_and(circuit,getAIGERRepresentation(defined_variable), gate_inputs[0], gate_inputs[1]);
    }
  } else {
    if (is_falsified) {
      auto val = gate_output > 0 ? aiger_false : aiger_true;
      gate_renaming[gate_output] = val;
    }
    else if (gate_inputs.size() == 0) {
      gate_renaming[gate_output] = aiger_true;
    } else if (gate_inputs.size() == 1) {
      gate_renaming[gate_output] = gate_inputs[0];
    } else if (gate_inputs.size() == 2) {
      int x = checkVariableInDefinition(gate_output);
      gate_output = getAIGERRepresentation(x);
      aiger_add_and(circuit, gate_output, gate_inputs[0], gate_inputs[1]);
    }
  }
  return false;
}

void AIGERBuilder::addDefinition(int variable_index, const DefCircuit& def, const std::unordered_map<int, bool>& arbiter_assignment) {
  std::unordered_map<int, int> gate_renaming;
  int defined_variable = existential_variables[variable_index];
  for (auto& [adder,variable]:def) {
    if (addDefinitionCircuitAdder(defined_variable, adder, variable, gate_renaming, arbiter_assignment)) {
      break;
    }
  }
}

void AIGERBuilder::addForcingClauses(int variable, const std::vector<Clause>& positive_forcing_clauses, const std::vector<Clause>& negative_forcing_clauses, 
    std::vector<Clause>& default_clauses, const std::unordered_map<int, bool>& arbiter_assignment) {
  int var_representation = getAIGERRepresentation(variable);
  //If we have no default clause, we set the existential variable to true, whenever no forcing clause fires.
  if (default_clauses.size() <= 1) {
    assert (default_clauses.empty() || default_clauses[0].size() == 1);
    int default_constant = default_clauses.empty() ? variable : default_clauses[0][0];
    if (default_constant>0) {
      int forced = getIsActiveCircuit(negative_forcing_clauses, arbiter_assignment);
      aiger_add_and(circuit,var_representation, getAIGERNegation(forced), aiger_true);
    } else {
      int forced=getIsActiveCircuit(positive_forcing_clauses,arbiter_assignment);
      aiger_add_and(circuit,var_representation, forced, aiger_true);
    }
  } else {
    int positive_fires=getIsActiveCircuit(positive_forcing_clauses,arbiter_assignment);
    if (positive_fires == aiger_true) {
      aiger_add_and(circuit,var_representation, aiger_true, aiger_true);
      return;
    } 
    int negative_fires=getIsActiveCircuit(negative_forcing_clauses,arbiter_assignment);
    if (negative_fires == aiger_true) {
      aiger_add_and(circuit,var_representation, aiger_false, aiger_false);
      return;
    }

    auto [positive_default_clauses, negative_default_clauses] = filterClauses(default_clauses);
    // int default_applicable = getAIGERRepresentation(++last_used_intermediate_variable);
    if (positive_default_clauses.size() > negative_default_clauses.size()) {
      //the generated circuit represents the formula: -(negative_fires \/ (-positive_fires /\ negative_default_fires))
      int negative_default_fires = getIsActiveCircuit(negative_default_clauses,arbiter_assignment);
      int default_applicable = combineAIGERVariables(getAIGERNegation(positive_fires),negative_default_fires);
      int is_positive = combineAIGERVariables(getAIGERNegation(negative_fires), getAIGERNegation(default_applicable));
      aiger_add_and(circuit,var_representation, is_positive, aiger_true);
    } else {
      //the generated circuit represents the formula: positive_fires \/ (-negative_fires /\ positive_default_fires)
      int positive_default_fires = getIsActiveCircuit(positive_default_clauses,arbiter_assignment);
      int default_applicable = combineAIGERVariables(getAIGERNegation(negative_fires),positive_default_fires);
      int is_negative = combineAIGERVariables(getAIGERNegation(positive_fires), getAIGERNegation(default_applicable));
      aiger_add_and(circuit,var_representation, getAIGERNegation(is_negative), aiger_true);
    }
  }
}

void AIGERBuilder::addSkolemFunction(int variable, const std::vector<DefCircuit>& definitions, const std::vector<std::vector<std::vector<int>>>& conditions, 
    const std::vector<std::vector<DefCircuit>>& conditional_def, const std::vector<std::vector<Clause>>& positive_forcing_clauses, 
    const std::vector<std::vector<Clause>>& negative_forcing_clauses, const std::unordered_map<int, std::vector<Clause>>& default_clauses_map, 
    const std::unordered_map<int, bool>& arbiter_assignment) {

  int index=indices.at(variable);
  if (!definitions[index].empty()) {
    addDefinition(index,definitions[index],arbiter_assignment);
    return;
  }
  bool conditional_definition_found=false;
  for (size_t i=0; i<conditions[index].size(); i++) {
    auto& condition = conditions[index][i];
    auto& cdef = conditional_def[index][i];
    if (isConflictEntailed(condition,arbiter_assignment)) {
      addDefinition(index,cdef,arbiter_assignment);
      conditional_definition_found=true;
      break;
    }
  }
  if (conditional_definition_found) {
    return;
  }
  auto default_clauses = default_clauses_map.at(variable);
  addForcingClauses(variable, positive_forcing_clauses[index], negative_forcing_clauses[index], default_clauses, arbiter_assignment);
}

void AIGERBuilder::computeCircuitRepresentation(const std::vector<DefCircuit>& definitions, const std::vector<std::vector<std::vector<int>>>& conditions, 
    const std::vector<std::vector<DefCircuit>>& conditional_def, const std::vector<std::vector<Clause>>& positive_forcing_clauses, 
    const std::vector<std::vector<Clause>>& negative_forcing_clauses, const std::unordered_map<int, std::vector<Clause>>& default_clauses_map, 
    const std::unordered_map<int, bool>& arbiter_assignment) {

  for (int u:universal_variables) {
    aiger_add_input(circuit,getAIGERRepresentation(u),std::to_string(u).c_str());
  }
  for (int e:existential_variables) {
    addSkolemFunction(e, definitions, conditions, conditional_def, positive_forcing_clauses, negative_forcing_clauses, default_clauses_map, arbiter_assignment);
    aiger_add_output(circuit,getAIGERRepresentation(e),std::to_string(e).c_str());
  }
}


int AIGERBuilder::getIsActiveCircuit(const std::vector<Clause>& clauses, const std::unordered_map<int, bool>& arbiter_assignment) {
  std::vector<Clause> reduced_clauses;
  for (const Clause& cl:clauses) {
    auto [isActive,reduced] = removeArbiters(cl,arbiter_assignment);
    if (isActive) {
      if (reduced.empty()) {
        return aiger_true;
      }
      reduced_clauses.push_back(reduced);
    }
  }
  if (reduced_clauses.empty()) {
    return aiger_false;
  }
  if (reduced_clauses.size() == 1) {
    int is_active = getIsActiveCircuit(reduced_clauses.front());;
    return getAIGERRepresentation(is_active);
  }
  std::vector<int> clause_fires;
  clause_fires.reserve(reduced_clauses.size());
  for (const Clause& cl : reduced_clauses) {
    int x=getIsActiveCircuit(cl);
    clause_fires.push_back(x);
  }

  //The forcing clauses and the clauses for the default functions represent implications like
  //(not c1 /\ ... /\ not cn) => e. Thus we encode (not c1 /\ ... /\ not cn) by a circuit.
  //In contrast to this, subsequently we want to encode if one of the implications "fires" then
  //use the respective value i.e. (not (not active1 /\ ... /\ activem))=>e.
  //Thus we have to use the negation of this value.
  int is_active = -getIsActiveCircuit(clause_fires);;
  return getAIGERRepresentation(is_active);
}

int AIGERBuilder::getIsActiveCircuit(const Clause& clause) {
  assert (!clause.empty());
  if (clause.size() == 1) {
    return -clause.front();
  } else {
    int active=++last_used_intermediate_variable;
    int x = -clause.front();
    for (int i=1;i<clause.size();i++) {
      addAdderToCircuit(x,-clause[i],active);
      x=active;
      active=++last_used_intermediate_variable;
    }
    active=--last_used_intermediate_variable;//in the last iteration active is not required
    return active;
  }
}

bool AIGERBuilder::write(const std::string& filename, bool binary_mode) {
  const char* msg=aiger_check(circuit);

  //apparently sorting the ands causes some issue while checking the aig
  // //the ands shall be ordered as in a binary aiger
  // if (!binary_mode) {
  //   std::sort(circuit->ands, circuit->ands + circuit->num_ands,
  //     [] (auto a, auto b) { return a.lhs < b.lhs;});
  // }

  if (msg!=0) {//There is an error
    std::cerr<<msg<<std::endl;
    return false;
  }
  FILE * file;
  file = fopen(filename.c_str(),"w");
  if(file==0) {
    std::cerr<<"File could not be opened."<<std::endl;
    return false;
  }
  int res;
  if (binary_mode) {
    res = aiger_write_to_file(circuit,aiger_binary_mode,file);
  } else {
    res = aiger_write_to_file(circuit,aiger_ascii_mode,file);
  }
  fclose(file);
  if (!res) {
    // unlink (filename.c_str()); We do not delete files
    std::cerr<<"Could not write to file."<<std::endl;
    return false;
  }
  // writes either an ASCII or a binary AIGER depending on the file extension of filename.
  // aiger_open_and_write_to_file(circuit,filename.c_str());
  return true;
}

bool AIGERBuilder::writeToFile(const std::string& filename,bool binary_mode,
      const std::vector<DefCircuit>& defs, const std::vector<std::vector<std::vector<int>>>& conditions, 
      const std::vector<std::vector<DefCircuit>>& conditional_def, const std::vector<std::vector<Clause>>& pos_forcing_clauses, 
      const std::vector<std::vector<Clause>>& neg_forcing_clause, 
      const std::unordered_map<int, std::vector<Clause>>& default_clauses, const std::vector<int>& arbiter_assignment) {
  
  std::unordered_map<int,bool> arbiter_assignment_map;
  for (auto lit : arbiter_assignment) {
    arbiter_assignment_map[var(lit)] = lit > 0;
  }

  computeCircuitRepresentation(defs,conditions, conditional_def, pos_forcing_clauses, 
      neg_forcing_clause, default_clauses, arbiter_assignment_map);
  return write(filename, binary_mode);
}


}