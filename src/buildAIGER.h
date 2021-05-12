#ifndef PEDANT_BUILDAIGER_H_
#define PEDANT_BUILDAIGER_H_

#include <vector>
#include <tuple>
#include <unordered_map>
#include <string>

#include "solvertypes.h"
extern "C" {
    #include "aiger.h"
}

namespace pedant {

typedef std::vector<std::tuple<std::vector<int>,int>> Def_Circuit;

  
class AIGERBuilder {
 public:
  AIGERBuilder(const std::unordered_map<int,int>& indices,const std::vector<int>& existential_variables, const std::vector<int>& universal_variables,
    const std::vector<Def_Circuit>& defs,const std::vector<std::vector<std::tuple<std::vector<int>,Def_Circuit>>>& conf_def,
    const std::vector<std::vector<Clause>>& pos_force, const std::vector<std::vector<Clause>>& neg_force,
    const std::vector<bool>& use_defaults,const std::vector<int>& defaults,
    const std::vector<std::vector<Clause>>& pos_default_fun,
    const std::vector<std::vector<Clause>>& neg_default_fun);
  ~AIGERBuilder();
  void computeCircuitRepresentation(const std::vector<int>& arbiter_assignment);
  bool writeToFile(const std::string& filename,bool binary_mode);
  void init(int max_var_in_matrix);

 private:
  aiger* circuit;
  bool already_built=false;
  std::unordered_map<int,int> renamings;
  const std::unordered_map<int,int>& indices;
  const std::vector<int>& existential_variables;
  const std::vector<int>& universal_variables;
  const std::vector<Def_Circuit>& definitions_circuits;
  const std::vector<std::vector<std::tuple<std::vector<int>,Def_Circuit>>>& conditional_definitions_circuits;
  const std::vector<std::vector<Clause>>& positive_forcing_clauses;
  const std::vector<std::vector<Clause>>& negative_forcing_clauses;
  const std::vector<bool>& use_default_value;
  const std::vector<int>& default_values;
  const std::vector<std::vector<Clause>>& positive_default_function_clauses;
  const std::vector<std::vector<Clause>>& negative_default_function_clauses;

  int intermediate_variables_start;
  int last_used_intermediate_variable;

  void addAdderToCircuit(int in1,int in2, int out);
  void addAdderToCircuit(int in, int out);
  void addAdderToCircuit(int out);

  void addAdderToCircuitCheckInputs(int in1,int in2, int out,const std::vector<int>& arbiter_assignment);
  void addAdderToCircuitCheckInputsUseConstantInput(int in1,int in2, int out,const std::vector<int>& arbiter_assignment);
  int checkVariableAndEliminateArbiters(int l,const std::vector<int>& arbiter_assignment);

  void addDefinition(int variable_index, const Def_Circuit& def,const std::vector<int>& arbiter_assignment);
  int checkVariableInDefinition(int l);

  int getIsActiveCircuit(const std::vector<Clause>& clauses, const std::vector<int>& arbiter_assignment);
  int getIsActiveCircuit(const Clause& clause);

  static bool isConflictEntailed(const std::vector<int>& conflict,const std::vector<int>& arbiter_assignment);
  static std::pair<bool,Clause> removeArbiters(const Clause& clause, const std::vector<int>& arbiter_assignment);
  int getAIGERRepresentation(int x);
};

inline AIGERBuilder::AIGERBuilder(const std::unordered_map<int,int>& indices,
    const std::vector<int>& existential_variables,
    const std::vector<int>& universal_variables,
    const std::vector<Def_Circuit>& defs,const std::vector<std::vector<std::tuple<std::vector<int>,Def_Circuit>>>& conf_def,
    const std::vector<std::vector<Clause>>& pos_force,
    const std::vector<std::vector<Clause>>& neg_force,
    const std::vector<bool>& use_defaults,const std::vector<int>& defaults,
    const std::vector<std::vector<Clause>>& pos_default_fun,
    const std::vector<std::vector<Clause>>& neg_default_fun) : indices(indices), circuit(nullptr),
      existential_variables(existential_variables),universal_variables(universal_variables),
      definitions_circuits(defs), conditional_definitions_circuits(conf_def),positive_forcing_clauses(pos_force),
      negative_forcing_clauses(neg_force),use_default_value(use_defaults),default_values(defaults),
      positive_default_function_clauses(pos_default_fun), negative_default_function_clauses(neg_default_fun) {
}

inline AIGERBuilder::~AIGERBuilder() {
  aiger_reset(circuit);
}

inline void AIGERBuilder::init(int max_var_in_matrix) {
  intermediate_variables_start=max_var_in_matrix+1;
  last_used_intermediate_variable=max_var_in_matrix;
  circuit=aiger_init();
}


}

#endif