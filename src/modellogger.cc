#include "modellogger.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cassert>


namespace pedant {

void ModelLogger::init(int last_variable_in_matrix) {
  last_variable_index_in_matrix=last_variable_in_matrix;
  unused_variable=last_variable_index_in_matrix+1;
  for (int i=0;i<existential_variables.size();i++) {
    int existential=existential_variables[i];
    indices.insert(std::make_pair(existential,i));
  }
  for (int e:existential_variables) {
    int default_val=ModelLogger::initial_default_polarity?e:-e;
    default_values.push_back(default_val);
  }
  aiger_generator.init(last_variable_index_in_matrix);
}

void ModelLogger::addDefinition(int variable,const std::vector<Clause>& def, const Def_Circuit& circuit_def) {
  int index=getIndex(variable);
  definitions[index].clear();
  definitions[index].insert(definitions[index].end(),def.begin(),def.end());
  conditional_definitions[index].clear();
  positive_forcing_clauses[index].clear();
  negative_forcing_clauses[index].clear();
  positive_default_function_clauses[index].clear();
  negative_default_function_clauses[index].clear();

  conditional_definitions_circuits[index].clear();
  definitions_circuits[index]=circuit_def;
}

void ModelLogger::addDefinition(int variable,const std::vector<int>& conflict, const std::vector<Clause>& definition, const Def_Circuit& circuit_def) {
  if (conflict.empty()) {
    addDefinition(variable,definition,circuit_def);
    return;
  }
  int index=getIndex(variable);
  auto x=std::make_tuple(std::vector<int>(conflict),std::vector<Clause>(definition));
  conditional_definitions[index].push_back(x);
  auto y=std::make_tuple(std::vector<int>(conflict),Def_Circuit(circuit_def));
  conditional_definitions_circuits[index].push_back(y);
}

void ModelLogger::addForcingClause(const Clause& forcing_clause) {
  int forced_literal = forcing_clause.back();
  int variable=abs(forced_literal);
  int index=getIndex(variable);
  if (forced_literal>0) {
    positive_forcing_clauses[index].push_back(Clause(forcing_clause.begin(),forcing_clause.end()-1));
  } else {
    negative_forcing_clauses[index].push_back(Clause(forcing_clause.begin(),forcing_clause.end()-1));
  }
}

void ModelLogger::setDefaultFunction(int existential,const std::vector<Clause>& clauses) {
  int index=getIndex(existential);
  positive_default_function_clauses[index].clear();
  negative_default_function_clauses[index].clear();
  for (const auto& clause:clauses) {
    Clause c(clause.begin()+1,clause.end());
    assert(abs(clause.front())==existential);
    if (clause.front()>0) {
      positive_default_function_clauses[index].push_back(c);
    } else {
      negative_default_function_clauses[index].push_back(c);
    }
  }
}

void ModelLogger::writeModelAsCNFToFile(const std::string& file_name,const std::vector<int>& arbiter_assignment) {
  std::stringstream strs;
  std::vector<int> arbs(arbiter_assignment);
  std::sort(arbs.begin(),arbs.end());
  int nof_clauses=0;
  for (int e:existential_variables) {
    int i=getIndex(e);
    strs<<"c Model for variable "<<std::to_string(e)<<"."<<std::endl;
    if (definitions[i].empty()) {
      if (!writeClausalEncodingConditionalDefinitions(strs,i,arbs,nof_clauses)) {
        if (use_default_value[i]) {
          writeForcingClausesAndDefaultValue(strs,i,arbs,nof_clauses);
        } else {
          writeForcingClausesAndDefaultFunctions(strs,i,arbs,nof_clauses);
        }
      }  
    } else {
      writeFormulaToStream(definitions[i],strs,arbs,nof_clauses);
    }
  }
  std::ofstream output(file_name);
  output<<"p cnf "<<std::to_string(unused_variable-1)<<" "<<std::to_string(nof_clauses)<<std::endl;
  output<<strs.str();
  output.close();
}

bool ModelLogger::isConflictEntailed(const std::vector<int>& conflict,const std::vector<int>& arbiter_assignment) {
  for (int l:conflict) {
    if (!std::binary_search(arbiter_assignment.begin(),arbiter_assignment.end(),l)) {
      return false;
    }
  }
  return true;
}

bool ModelLogger::writeClausalEncodingConditionalDefinitions(std::ostream& out,int index,const std::vector<int>& arbiter_assignment,int& nof_clauses) {
  for (auto& pair:conditional_definitions[index]) {
    if (isConflictEntailed(std::get<0>(pair),arbiter_assignment)) {
      positive_forcing_clauses[index].clear();
      negative_forcing_clauses[index].clear();
      positive_default_function_clauses[index].clear();
      negative_default_function_clauses[index].clear();
      writeFormulaToStream(std::get<1>(pair),out,arbiter_assignment,nof_clauses);
      return true;
    }
  }
  return false;
}


std::pair<bool,Clause> isSatisfiedByArbiterAssignment(const Clause& clause,const std::vector<int>& arbiter_assignment) {
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

void ModelLogger::handleForcingClauses(const std::vector<Clause>& clauses,Clause& default_activation,std::ostream& out,
    int forced,const std::vector<int>& arbiter_assignment,int& nof_clauses) {
  for (const Clause& c:clauses) {
    auto [active,reduced_clause] = isSatisfiedByArbiterAssignment(c,arbiter_assignment);
    if (active) {
      reduced_clause.push_back(unused_variable);
      writeClauseToStream(reduced_clause,out);
      reduced_clause.pop_back();
      for (int l:reduced_clause) {
        writeClauseToStream({-l,-unused_variable},out);
      }
      writeClauseToStream({-unused_variable,forced},out);
      default_activation.push_back(unused_variable);
      nof_clauses+=reduced_clause.size()+2;
      unused_variable++;
    }
    // c.pop_back();
  }
}

void ModelLogger::writeForcingClausesAndDefaultValue(std::ostream& out,int index,const std::vector<int>& arbiter_assignment,int& nof_clauses) {
  int existential=existential_variables[index];
  assert (use_default_value[index]);
  std::vector<Clause>& clauses=default_values[index]<0?positive_forcing_clauses[index]:negative_forcing_clauses[index];
  Clause default_activation_clause;
  handleForcingClauses(clauses,default_activation_clause,out,default_values[index]<0?existential:-existential,arbiter_assignment,nof_clauses);
  default_activation_clause.push_back(default_values[index]);
  writeClauseToStream(default_activation_clause,out);
  nof_clauses++;
}

void ModelLogger::handleDefaultFunction(int index, Clause& default_activation,std::ostream& out,int& nof_clauses) {
  int use_default=unused_variable;
  unused_variable++;
  default_activation.push_back(use_default);
  writeClauseToStream(default_activation,out);
  default_activation.pop_back();
  for (auto& c:positive_default_function_clauses[index]) {
    c.push_back(-use_default);
    c.push_back(existential_variables[index]);
    writeClauseToStream(c,out);
    c.pop_back();
    c.pop_back();
  }
  for (auto& c:negative_default_function_clauses[index]) {
    c.push_back(-use_default);
    c.push_back(-existential_variables[index]);
    writeClauseToStream(c,out);
    c.pop_back();
    c.pop_back();
  }
  nof_clauses+=1+positive_default_function_clauses.size()+negative_default_function_clauses.size();
}

void ModelLogger::writeForcingClausesAndDefaultFunctions(std::ostream& out,int index,const std::vector<int>& arbiter_assignment,int& nof_clauses) {
  int existential=existential_variables[index];
  assert (!use_default_value[index]);
  // int default_value;
  // if (positive_default_function_clauses.size()>=negative_default_function_clauses.size()) {
  //   default_value=existential;
  // } else {
  //   default_value=-existential;
  // }  
  Clause default_activation_clause;
  /*
   * The learned clauses do not necessarily correspond to the arbiter clauses under the current arbiter assignment
   * (but to the forcing clauses as long as all assignments that were for building a forcing clauses were also
   * used for sampling). This means that we cannot omit the positive "forcing" (in the sense of this class -- i.e. also arbiter clauses)
   * clauses respectively negative forcing clauses as we did in the case of default values.
   */
  // std::vector<Clause>& clauses=default_value<0?positive_forcing_clauses[index]:negative_forcing_clauses[index];
  // handleForcingClauses(clauses,default_activation_clause,out,-default_value,arbiter_assignment,nof_clauses);

  handleForcingClauses(positive_forcing_clauses[index],default_activation_clause,out,existential,arbiter_assignment,nof_clauses);
  handleForcingClauses(negative_forcing_clauses[index],default_activation_clause,out,-existential,arbiter_assignment,nof_clauses);

  // std::vector<Clause>& default_function_clauses=default_value<0?positive_default_function_clauses[index]:negative_default_function_clauses[index];
  // handleForcingClauses(default_function_clauses,default_activation_clause,out,-default_value,arbiter_assignment,nof_clauses);
  // default_activation_clause.push_back(default_value);
  // writeClauseToStream(default_activation_clause,out);
  // nof_clauses++;

  handleDefaultFunction(index,default_activation_clause,out,nof_clauses);
}





void ModelLogger::writeFormulaToStream(const std::vector<Clause>& formula,
    std::ostream& out,const std::vector<int>& arbiter_assignment,int& nof_clauses) {
  for (const auto& clause:formula) {
    bool clause_added=writeClauseToStream(clause,out,arbiter_assignment);
    if (clause_added) {
      nof_clauses++;
    }
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

bool ModelLogger::writeClauseToStream(const Clause& clause,
    std::ostream& out,const std::vector<int>& arbiter_assignment) {
  for (int l:clause) {
    if (std::binary_search(arbiter_assignment.begin(),arbiter_assignment.end(),l)) {
      return false;
    }
  }
  for (int l:clause) {
    if (std::binary_search(arbiter_assignment.begin(),arbiter_assignment.end(),-l)) {
      continue;
    } 
    if (abs(l)>last_variable_index_in_matrix) {
      if (renamings_auxiliary_variables.end()==renamings_auxiliary_variables.find(abs(l))) {
        renamings_auxiliary_variables[abs(l)]=unused_variable;
        if (l<0) {
          out<<std::to_string(-unused_variable);
        } else {
          out<<std::to_string(unused_variable);
        }
        unused_variable++;
      } else {
        int variable=renamings_auxiliary_variables[abs(l)];
        if (l<0) {
          out<<std::to_string(-variable);
        } else {
          out<<std::to_string(variable);
        }
      }
      out<<" ";
    } else {
      out<<std::to_string(l);
      out<<" ";
    }
  }
  out<<"0";
  out<<std::endl;
  return true;
}


void ModelLogger::writeModelAsAIGToFile(const std::string& file_name,const std::vector<int>& arbiter_assignment,bool binary_AIGER) {
  std::vector<int> arbs(arbiter_assignment);
  std::sort(arbs.begin(),arbs.end());
  aiger_generator.computeCircuitRepresentation(arbs);
  aiger_generator.writeToFile(file_name,binary_AIGER);
}





}