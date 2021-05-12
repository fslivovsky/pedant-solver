#ifndef PEDANT_MODELLOGGER_H_
#define PEDANT_MODELLOGGER_H_

#include <vector>
#include <unordered_map>
#include <string>
#include <tuple>

#include "solvertypes.h"
#include "buildAIGER.h"


namespace pedant {

class ModelLogger {

  std::unordered_map<int,int> indices;
  const std::vector<int>& existential_variables;
  const std::vector<int>& universal_variables;
  std::vector<std::vector<Clause>> definitions;
  std::vector<std::vector<std::tuple<std::vector<int>,std::vector<Clause>>>> conditional_definitions;

  std::vector<Def_Circuit> definitions_circuits;
  std::vector<std::vector<std::tuple<std::vector<int>,Def_Circuit>>> conditional_definitions_circuits;

  std::vector<std::vector<Clause>> positive_forcing_clauses;
  std::vector<std::vector<Clause>> negative_forcing_clauses;
  std::vector<bool> use_default_value;
  std::vector<int> default_values;
  std::vector<std::vector<Clause>> positive_default_function_clauses;
  std::vector<std::vector<Clause>> negative_default_function_clauses;
  
  int last_variable_index_in_matrix;
  int unused_variable;
  
  std::unordered_map<int,int> renamings_auxiliary_variables;
  AIGERBuilder aiger_generator;

 public:
  ModelLogger(const std::vector<int>& existential, const std::vector<int>& universals, int nof_existentials);
  void init(int last_variable_in_matrix);
  void addDefinition(int variable,const std::vector<int>& conflict, const std::vector<Clause>& definition, const Def_Circuit& circuit_def);
  void addForcingClause(const Clause& forcing_premise);
  void setDefaultValue(int existential,bool sign);
  void setDefaultFunction(int index,const std::vector<Clause>& clauses);
  void useDefaultValue(int existential,bool use);


  void writeModelAsCNFToFile(const std::string& file_name,const std::vector<int>& arbiter_assignment);
  void writeModelAsAIGToFile(const std::string& file_name,const std::vector<int>& arbiter_assignment, bool binary_AIGER);

 private:
  static constexpr bool initial_default_polarity=false;
  void writeClauseToStream(const Clause& clause,std::ostream& out);
  bool writeClauseToStream(const Clause& clause,std::ostream& out,
      const std::vector<int>& arbiter_assignment);
  void writeFormulaToStream(const std::vector<Clause>& formula,
      std::ostream& out,const std::vector<int>& arbiter_assignment,int& nof_clauses);
  int getIndex(int existential);
  void addDefinition(int variable,const std::vector<Clause>& definition, const Def_Circuit& circuit_def);

  bool writeClausalEncodingConditionalDefinitions(std::ostream& out,int index,const std::vector<int>& arbiter_assignment,int& nof_clauses);

  void writeForcingClausesAndDefaultValue(std::ostream& out,int index,const std::vector<int>& arbiter_assignment,int& nof_clauses);
  void writeForcingClausesAndDefaultFunctions(std::ostream& out,int index,const std::vector<int>& arbiter_assignment,int& nof_clauses);

  void handleForcingClauses(const std::vector<Clause>& clauses,Clause& default_activation,std::ostream& out,
    int forced,const std::vector<int>& arbiter_assignment,int& nof_clauses);

  void handleDefaultFunction(int index, Clause& default_activation,std::ostream& out,int& nof_clauses);

  static bool isConflictEntailed(const std::vector<int>& conflict,const std::vector<int>& arbiter_assignment);



};

/**
 * The reference to the existential variables is initially empty.
 */
inline ModelLogger::ModelLogger(const std::vector<int>& existentials, const std::vector<int>& universals, int nof_exisistentials) :
    existential_variables(existentials),universal_variables(universals),
    definitions(nof_exisistentials), conditional_definitions(nof_exisistentials),
    definitions_circuits(nof_exisistentials), conditional_definitions_circuits(nof_exisistentials),
    positive_forcing_clauses(nof_exisistentials), negative_forcing_clauses(nof_exisistentials),
    use_default_value(nof_exisistentials,true), 
    positive_default_function_clauses(nof_exisistentials),negative_default_function_clauses(nof_exisistentials),
    aiger_generator(indices,existential_variables,universal_variables,definitions_circuits,conditional_definitions_circuits,
      positive_forcing_clauses,negative_forcing_clauses,use_default_value,default_values,
      positive_default_function_clauses,negative_default_function_clauses) {
}


inline int ModelLogger::getIndex(int existential) {
  return indices.at(existential);
}

inline void ModelLogger::useDefaultValue(int existential,bool use) {
  int index=getIndex(existential);
  use_default_value[index]=use;
}

inline void ModelLogger::setDefaultValue(int existential,bool sign) {
  int index=getIndex(existential);
  default_values[index]=sign?existential:-existential;
}



}




#endif