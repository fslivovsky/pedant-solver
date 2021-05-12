#include <algorithm>
#include <assert.h>

#include "buildAIGER.h"

#include <iostream>

namespace pedant {

bool AIGERBuilder::isConflictEntailed(const std::vector<int>& conflict,const std::vector<int>& arbiter_assignment) {
  for (int l:conflict) {
    if (!std::binary_search(arbiter_assignment.begin(),arbiter_assignment.end(),l)) {
      return false;
    }
  }
  return true;
}

std::pair<bool,Clause> AIGERBuilder::removeArbiters(const Clause& clause, const std::vector<int>& arbiter_assignment) {
  Clause result;
  for (int l:clause) {
    if (std::binary_search(arbiter_assignment.begin(),arbiter_assignment.end(),l)) {
      return std::make_pair(false,result);
    }
    if (!std::binary_search(arbiter_assignment.begin(),arbiter_assignment.end(),-l)) {
      result.push_back(l);
    }
  }
  return std::make_pair(true,result);
}

int AIGERBuilder::getAIGERRepresentation(int x) {
  return x>0?2*x:-2*x+1;
}

void AIGERBuilder::addAdderToCircuit(int in1,int in2, int out) {
  aiger_add_and(circuit,getAIGERRepresentation(out),getAIGERRepresentation(in1),getAIGERRepresentation(in2));
}


void AIGERBuilder::addAdderToCircuit(int in, int out) {
  aiger_add_and(circuit,getAIGERRepresentation(out),getAIGERRepresentation(in),aiger_true);
}

void AIGERBuilder::addAdderToCircuit(int out) {
  aiger_add_and(circuit,getAIGERRepresentation(out),aiger_true,aiger_true);
}

void AIGERBuilder::addAdderToCircuitCheckInputs(int in1,int in2, int out,const std::vector<int>& arbiter_assignment) {
  in1=checkVariableAndEliminateArbiters(in1,arbiter_assignment);
  in2=checkVariableAndEliminateArbiters(in2,arbiter_assignment);
  aiger_add_and(circuit,getAIGERRepresentation(out),in1,in2);
}

void AIGERBuilder::addAdderToCircuitCheckInputsUseConstantInput(int in1, int in2, int out,const std::vector<int>& arbiter_assignment) {
  in1=checkVariableAndEliminateArbiters(in1,arbiter_assignment);
  aiger_add_and(circuit,getAIGERRepresentation(out),in1,in2);
}

int AIGERBuilder::checkVariableAndEliminateArbiters(int l,const std::vector<int>& arbiter_assignment) {
  if (std::binary_search(arbiter_assignment.begin(),arbiter_assignment.end(),l)) {
    l=aiger_true;
  } else if (std::binary_search(arbiter_assignment.begin(),arbiter_assignment.end(),-l)) { 
    l=aiger_false;
  } else {
    l=checkVariableInDefinition(l);
    l=getAIGERRepresentation(l);
  }

  return l;
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

void AIGERBuilder::addDefinition(int variable_index, const Def_Circuit& def,const std::vector<int>& arbiter_assignment) {
  if (def.size()==1 && std::get<0>(def[0]).size()==0) {
    int out=std::get<1>(def.back());
    int aiger_out = getAIGERRepresentation(abs(out));
    if (out>0) {
      aiger_add_and(circuit,aiger_out,aiger_true,aiger_true);
    } else {
      aiger_add_and(circuit,aiger_out,aiger_false,aiger_false);
    }
    aiger_add_output(circuit,aiger_out,std::to_string(existential_variables[variable_index]).c_str());
    return;
  }
  for (auto& [adder,variable]:def) {
    assert(adder.size()<=2&&!adder.empty());
    int out=checkVariableInDefinition(variable);
    if(adder.size()<2) {
      addAdderToCircuitCheckInputsUseConstantInput(adder[0],1,out,arbiter_assignment);
    } else {
      addAdderToCircuitCheckInputs(adder[0],adder[1],out,arbiter_assignment);
    }
  }
  int out=std::get<1>(def.back());
  out=getAIGERRepresentation(out);
  aiger_add_output(circuit,out,std::to_string(existential_variables[variable_index]).c_str());
}

void AIGERBuilder::computeCircuitRepresentation(const std::vector<int>& arbiter_assignment) {
  if (already_built) {
    return;
  }
  already_built=true;
  for (int u:universal_variables) {
    aiger_add_input(circuit,getAIGERRepresentation(u),std::to_string(u).c_str());
  }
  for (int e:existential_variables) {
    Def_Circuit circuit_for_variable;
    int index=indices.at(e);
    if (!definitions_circuits[index].empty()) {
      addDefinition(index,definitions_circuits[index],arbiter_assignment);
      continue;
    }
    bool conditional_definition_found=false;
    for(const auto& [conflict,def]:conditional_definitions_circuits[index]) {
      if (isConflictEntailed(conflict,arbiter_assignment)) {
        addDefinition(index,def,arbiter_assignment);
        conditional_definition_found=true;
        break;
      }
    }
    if (conditional_definition_found) {
      continue;
    }
    if (use_default_value[index]) {
      /**
       * forced represents not(not active_1 /\ ... not active_m). This means if forced is true
       * some clause "fires". If the default value is false, then the variable is true iff a clause
       * "fires". This means the variable is true iff forced is true. If the default value is
       * true, then the variable is true iff no clause "fires". This means if forced is false.
       **/
      if (default_values[index]<0) {
        int forced=getIsActiveCircuit(positive_forcing_clauses[index],arbiter_assignment);
        addAdderToCircuit(forced,e);
      } else {
        int forced=getIsActiveCircuit(negative_forcing_clauses[index],arbiter_assignment);
        addAdderToCircuit(-forced,e);
      }
    } else {
      int positive_fires=getIsActiveCircuit(positive_forcing_clauses[index],arbiter_assignment);
      int negative_fires=getIsActiveCircuit(negative_forcing_clauses[index],arbiter_assignment);
      int positive_default_fires=getIsActiveCircuit(positive_default_function_clauses[index],arbiter_assignment);
      int default_applicable=++last_used_intermediate_variable;
      addAdderToCircuit(-negative_fires,positive_default_fires,default_applicable);
      int is_positive=++last_used_intermediate_variable;
      addAdderToCircuit(-positive_fires,-default_applicable,is_positive);
      addAdderToCircuit(-is_positive,e);
    }
    aiger_add_output(circuit,getAIGERRepresentation(e),std::to_string(e).c_str());
  }
}


int AIGERBuilder::getIsActiveCircuit(const std::vector<Clause>& clauses, const std::vector<int>& arbiter_assignment) {
  std::vector<int> clause_fires;
  clause_fires.reserve(clauses.size());
  for (const Clause& cl:clauses) {
    auto [isActive,reduced] = removeArbiters(cl,arbiter_assignment);
    if (isActive) {
      int x=getIsActiveCircuit(reduced);
      clause_fires.push_back(x);
    }
  }
  //The forcing clauses and the clauses for the default functions represent implications like
  //(not c1 /\ ... /\ not cn) => e. Thus we encode (not c1 /\ ... /\ not cn) by a circuit.
  //In contrast to this, subsequently we want to encode if one of the implications "fires" then
  //use the respective value i.e. (not (not active1 /\ ... /\ activem))=>e.
  //Thus we have to use the negation of this value.
  return -getIsActiveCircuit(clause_fires);
}

int AIGERBuilder::getIsActiveCircuit(const Clause& clause) {
  int active=++last_used_intermediate_variable;
  if (clause.size()==0) {
    addAdderToCircuit(active);
  } else if (clause.size()==1) {
    addAdderToCircuit(-clause[0],active);
  } else {
    int x=-clause[0];
    for (int i=1;i<clause.size();i++) {
      addAdderToCircuit(x,-clause[i],active);
      x=active;
      active=++last_used_intermediate_variable;
    }
    active=--last_used_intermediate_variable;//in the last iteration active is not required
  }
  return active;
}


bool AIGERBuilder::writeToFile(const std::string& filename,bool binary_mode) {
  assert (already_built);
  const char* msg=aiger_check(circuit);
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
  // writes either an ASCII or a binary AIGEr depending on the file extension of filename.
  // aiger_open_and_write_to_file(circuit,filename.c_str());
  return true;
}

}