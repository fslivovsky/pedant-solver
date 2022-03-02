#include "modellogger.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cassert>


namespace pedant {

ModelLogger::ModelLogger(const std::vector<int>& existential_variables, 
    const std::vector<int>& universal_variables, 
    const DefaultValueContainer& default_values, const Configuration& config) :
    existential_variables(existential_variables), universal_variables(universal_variables),
    default_values(default_values), config(config) {
}


void ModelLogger::init(int last_variable_in_matrix) {
  max_variable_in_matrix=last_variable_in_matrix;
  unused_variable=max_variable_in_matrix+1;
  for (int i=0;i<existential_variables.size();i++) {
    int existential=existential_variables[i];
    indices.insert(std::make_pair(existential,i));
  }
  int nof_existentials = existential_variables.size();
  positive_forcing_clauses.resize(nof_existentials);
  negative_forcing_clauses.resize(nof_existentials);

  definitions_circuit.resize(nof_existentials);
  definitions_cnf.resize(nof_existentials);
  conditions.resize(nof_existentials);
  conditional_definitions_circuit.resize(nof_existentials);
  conditional_definitions_cnf.resize(nof_existentials);
}

void ModelLogger::addForcingClause(const Clause& forcing_clause) {
  int forced_literal = forcing_clause.back();
  int variable = var(forced_literal);
  size_t idx = getIndex(variable);
  Clause fc (forcing_clause.begin(),forcing_clause.end()-1);
  if (forced_literal>0) {
    positive_forcing_clauses[idx].push_back(fc);
  } else {
    negative_forcing_clauses[idx].push_back(fc);
  }  
}


void ModelLogger::addDefinition(int variable, const std::vector<Clause>& def_clauses, const DefCircuit& def_circuit) {
  size_t idx = getIndex(variable);
  definitions_circuit[idx] = def_circuit;
  definitions_cnf[idx] = def_clauses;
}

void ModelLogger::addConditionalDefinition(int variable, const std::vector<int>& condition, 
    const std::vector<Clause>& def_clauses, const DefCircuit& def_circuit) {
  if (condition.size()==0) {
    addDefinition(variable,def_clauses,def_circuit);
    return;
  }
  size_t idx = getIndex(variable);
  conditions[idx].push_back(std::vector<int>(condition));
  conditional_definitions_circuit[idx].push_back(DefCircuit(def_circuit));
  conditional_definitions_cnf[idx].push_back(std::vector<Clause>(def_clauses));
}

void ModelLogger::writeModelAsCNFToFile(const std::string& file_name,const std::vector<int>& arbiter_assignment) {
  std::vector<int> arbs(arbiter_assignment);
  std::sort(arbs.begin(),arbs.end());
  std::stringstream strs;
  int nof_clauses=0;
  auto default_clause_map = default_values.getCertificate();
  for (auto [e,idx] : indices) {
    strs<<"c Model for variable "<<std::to_string(e)<<"."<<std::endl;
    //If we have a definition it suffices to only consider the definition
    auto def =definitions_cnf[idx];
    if (def.empty()) { 
      //If true on of the conditional definitions fires. In this case we have the model for the variable
      if (!writeClausalEncodingConditionalDefinitions(idx,arbs,nof_clauses,strs)) {
        auto& default_clauses = default_clause_map.at(e);
        processForcingClauses(idx,arbs,default_clauses,nof_clauses,strs);
      } 
    } else {
      writeFormulaToStream(def,arbs,nof_clauses,strs);
    }
  }
  std::ofstream output(file_name);
  output<<"p cnf "<<std::to_string(unused_variable-1)<<" "<<std::to_string(nof_clauses)<<std::endl;
  output<<strs.rdbuf();
  output.close();
  unused_variable=max_variable_in_matrix+1;
  renaming_auxiliaries.clear();
}

void ModelLogger::writeModelAsAIGToFile(const std::string& file_name,const std::vector<int>& arbiter_assignment, bool binary_AIGER) {
  AIGERBuilder aiger_generator(indices,max_variable_in_matrix,existential_variables,universal_variables);
  std::vector<int> arbs(arbiter_assignment);
  std::sort(arbs.begin(),arbs.end());
  auto default_clause_map = default_values.getCertificate();
  if (config.restrictModel) {
    aiger_generator.writeToFile(file_name,binary_AIGER, config.restrictModelTo, definitions_circuit,conditions,conditional_definitions_circuit,positive_forcing_clauses,negative_forcing_clauses,default_clause_map, arbs);
  } else {
    aiger_generator.writeToFile(file_name,binary_AIGER, definitions_circuit,conditions,conditional_definitions_circuit,positive_forcing_clauses,negative_forcing_clauses,default_clause_map, arbs);
  }
  
}



//write clausal representation of the model

bool ModelLogger::writeClausalEncodingConditionalDefinitions(size_t variable_index, 
    const std::vector<int>& arbiter_assignment, int& nof_clauses, std::ostream& out) {

  for (size_t i=0; i<conditions[variable_index].size(); i++) {
    auto& condition = conditions[variable_index][i];
    auto& def = conditional_definitions_cnf[variable_index][i];
    if (isConditionEntailed(condition,arbiter_assignment)) {
      writeFormulaToStream(def,arbiter_assignment,nof_clauses,out);
      return true; 
    }
  }
  return false;
}

void ModelLogger::writeForcingClauses(const std::vector<Clause>& clauses, 
      const std::vector<int>& arbiter_assignment, std::vector<int>& active_clauses, 
      int& nof_clauses, std::ostream& out) {
  for (const auto& clause : clauses) {
    Clause cl (clause);
    bool clause_satisfied = applyAssignment(cl,arbiter_assignment);
    if (!clause_satisfied) {
      auto active = writeClauseActive(cl,nof_clauses,out);
      active_clauses.push_back(active);
    }
  }
}

void ModelLogger::writeClausalEncodingDefaults(std::vector<Clause>& default_clauses, 
    int default_activator, int& nof_clauses, std::ostream& out) {
  assert(default_clauses.size()>1);
  for (auto& cl : default_clauses) {
    cl.push_back(default_activator);
    writeClauseToStream(cl,out);
    nof_clauses++;
    cl.pop_back();
  }
}

/**
 * If default_clauses is empty (config.use_default_values == false) we can directly print the forcing clauses
 * If default_clauses has only a single element, then we know that no tree was built (if a tree would have been built
 * then we would have at least two clauses) thus we have a default constant. This allows us to ignore some of the forcing clauses.
 * E.g. if the default constant is positive, we do not have to consider the positive forcing clauses -- we have to assign
 * true to the existential variable iff no negative forcing clause fires.
 * If default clauses has more than one element, then a tree was built. 
 * 
 **/
void ModelLogger::processForcingClauses(size_t variable_index, const std::vector<int>& arbiter_assignment, 
    std::vector<Clause>& default_clauses, int& nof_clauses, std::ostream& out) {
  auto e = existential_variables[variable_index]; 
  if (default_clauses.empty()) {
    writeFormulaToStream(positive_forcing_clauses[variable_index], arbiter_assignment, nof_clauses, out);
    writeFormulaToStream(negative_forcing_clauses[variable_index], arbiter_assignment, nof_clauses, out);
    return;
  }
  if (default_clauses.size() == 1) {
    assert(default_clauses[0].size() == 1);
    std::vector<int> active_clauses;
    auto default_constant = default_clauses[0][0];
    if (default_constant>0) {
      writeForcingClauses(negative_forcing_clauses[variable_index], arbiter_assignment,active_clauses,nof_clauses,out);
    } else {
      writeForcingClauses(positive_forcing_clauses[variable_index], arbiter_assignment,active_clauses,nof_clauses,out);
    }
    if (active_clauses.size() == 0) {
      writeClauseToStream({default_constant},out);
      nof_clauses++;
      return;
    } 
    auto rule_applies = -writeClauseActive(active_clauses, nof_clauses, out);
    writeClauseToStream({rule_applies, default_constant},out);//if no forcing clause fires use the default
    writeClauseToStream({-rule_applies, -default_constant},out);
    nof_clauses +=2;
    return;
  }
  std::vector<int> positive_active_clauses;
  std::vector<int> negative_active_clauses;
  writeForcingClauses(positive_forcing_clauses[variable_index], arbiter_assignment,positive_active_clauses,nof_clauses,out);
  writeForcingClauses(negative_forcing_clauses[variable_index], arbiter_assignment,negative_active_clauses,nof_clauses,out);
  //The following condition should never hold in the current version of the solver.
  if (positive_active_clauses.size() == 0 && negative_active_clauses.size() == 0) {
    writeFormulaToStream(default_clauses,nof_clauses,out);
    return;
  }
  int default_active;
  if (positive_active_clauses.size() == 0) {
    default_active = -writeClauseActive(negative_active_clauses, nof_clauses, out);
    writeClauseToStream({-default_active, -e},out);
    nof_clauses++;
  } else if (negative_active_clauses.size() == 0) {
    default_active = -writeClauseActive(positive_active_clauses, nof_clauses, out);
    writeClauseToStream({-default_active, e},out);
    nof_clauses++;
  } else {
    auto positive_fires = -writeClauseActive(positive_active_clauses, nof_clauses, out);
    auto negative_fires = -writeClauseActive(negative_active_clauses, nof_clauses, out);
    writeClauseToStream({-positive_fires, e},out);
    writeClauseToStream({-negative_fires, -e},out);
    default_active = -writeClauseActive({positive_fires,negative_fires}, nof_clauses, out);
    nof_clauses += 3;
  }
  writeClausalEncodingDefaults(default_clauses,default_active, nof_clauses,out);
}

int ModelLogger::writeClauseActive(Clause&& clause,int& nof_clauses, std::ostream& out) {
  return writeClauseActive(clause, nof_clauses, out);
}

int ModelLogger::writeClauseActive(Clause& clause,int& nof_clauses, std::ostream& out) {
  int active = unused_variable++;
  clause.push_back(active);
  writeClauseToStream(clause,out);
  clause.pop_back();
  for (auto l : clause) {
    Clause cl {-l,-active};
    writeClauseToStream(cl,out);
  }
  nof_clauses += clause.size()+1;
  return active;
}


//auxiliary methods for writing formulae to streams

void ModelLogger::writeFormulaToStream(const std::vector<Clause>& formula, 
    int& nof_clauses, std::ostream& out) {
  nof_clauses += formula.size();
  for (const auto& clause : formula) {
    writeClauseToStream(clause,out);
  }
}


void ModelLogger::writeClauseToStream(const Clause& clause,std::ostream& out) {
  for (int l:clause) {
    out<<std::to_string(l);
    out<<" ";
  }
  out<<"0";
  out<<std::endl;
}

void ModelLogger::writeFormulaToStream(const std::vector<Clause>& formula,
    const std::vector<int>& arbiter_assignment,int& nof_clauses, std::ostream& out) {
  for (const auto& clause:formula) {
    bool clause_added=writeClauseToStream(clause,arbiter_assignment,out);
    if (clause_added) {
      nof_clauses++;
    }
  }
}

bool ModelLogger::writeClauseToStream(const Clause& clause,
    const std::vector<int>& arbiter_assignment, std::ostream& out) {
  for (int l:clause) {
    if (std::binary_search(arbiter_assignment.begin(),arbiter_assignment.end(),l)) {
      return false;
    }
  }
  for (int l:clause) {
    if (std::binary_search(arbiter_assignment.begin(),arbiter_assignment.end(),-l)) {
      continue;
    } 
    writeLiteral(l,out);
    out<<" ";
  }
  out<<"0";
  out<<std::endl;
  return true;
}

void ModelLogger::writeLiteral(int literal, std::ostream& out) {
  int v = var(literal);
  if (v > max_variable_in_matrix) { //the variable is an auxiliary variable
    if (renaming_auxiliaries.find(v) == renaming_auxiliaries.end()) {
      renaming_auxiliaries[v] = unused_variable++;
    }
    out<<std::to_string(literal<0 ? -renaming_auxiliaries[v] : renaming_auxiliaries[v]);
  } else {
    out<<std::to_string(literal);
  }
}



}