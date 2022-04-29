#ifndef PEDANT_BUILDAIGER_H_
#define PEDANT_BUILDAIGER_H_

#include <vector>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <string>

#include "solvertypes.h"
extern "C" {
    #include "aiger.h"
}

namespace pedant {


  
class AIGERBuilder {
 public:
  /**
   * The second tuple component can only be negative in the last vector element if the first component is empty
   **/
  using DefCircuit = std::vector<std::tuple<std::vector<int>,int>>;

  AIGERBuilder(const std::unordered_map<int,size_t>& indices, int max_var_in_matrix, 
      const std::vector<int>& existential_variables, const std::vector<int>& universal_variables);
  ~AIGERBuilder();
  /**
   * @param arbiter_assignment must be sorted
   **/
  bool writeToFile(const std::string& filename, bool binary_mode,
      const std::vector<DefCircuit>& defs, const std::vector<std::vector<std::vector<int>>>& conditions, 
      const std::vector<std::vector<DefCircuit>>& conditional_def, const std::vector<std::vector<Clause>>& pos_forcing_clauses, 
      const std::vector<std::vector<Clause>>& neg_forcing_clause, const std::unordered_map<int, std::vector<Clause>>& default_clauses, 
      const std::vector<int>& arbiter_assignment);


 private:

  aiger* circuit;
  std::unordered_map<int,int> renamings;
  const std::unordered_map<int,size_t>& indices;
  const std::vector<int>& existential_variables;
  const std::vector<int>& universal_variables;

  int intermediate_variables_start;
  int last_used_intermediate_variable;

  void computeCircuitRepresentation(const std::vector<DefCircuit>& defs, const std::vector<std::vector<std::vector<int>>>& conditions, 
      const std::vector<std::vector<DefCircuit>>& conditional_def, const std::vector<std::vector<Clause>>& pos_forcing_clauses, 
      const std::vector<std::vector<Clause>>& neg_forcing_clause, const std::unordered_map<int, std::vector<Clause>>& default_clauses, 
      const std::unordered_map<int, bool>& arbiter_assignment);


  //we first have to compute the aiger representations of the arguments
  void addAdderToCircuit(int in1,int in2, int out);

  int combineAIGERVariables(int in1,int in2);

  bool addDefinitionCircuitAdder(int defined_variable, const std::vector<int>& inputs, int gate_output, std::unordered_map<int,int>& gate_renaming, const std::unordered_map<int, bool>& arbiter_assignment);

  void addSkolemFunction(int variable, const std::vector<DefCircuit>& defs, const std::vector<std::vector<std::vector<int>>>& conditions, 
      const std::vector<std::vector<DefCircuit>>& conditional_def, const std::vector<std::vector<Clause>>& pos_forcing_clauses, 
      const std::vector<std::vector<Clause>>& neg_forcing_clause, const std::unordered_map<int, std::vector<Clause>>& default_clauses, 
      const std::unordered_map<int, bool>& arbiter_assignment);
  void addDefinition(int variable_index, const DefCircuit& def,const std::unordered_map<int, bool>& arbiter_assignment);
  void addForcingClauses(int variable, const std::vector<Clause>& positive_forcing_clauses, const std::vector<Clause>& negative_forcing_clauses, 
      std::vector<Clause>& default_clauses, const std::unordered_map<int, bool>& arbiter_assignment);
  int checkVariableInDefinition(int l);

  int getIsActiveCircuit(const std::vector<Clause>& clauses, const std::unordered_map<int, bool>& arbiter_assignment);
  int getIsActiveCircuit(const Clause& clause);
  bool write(const std::string& filename, bool binary_mode);

  /**
   * Separate those clauses with a positive last literal from those with a negative last literal.
   * This method can be used to separate positive and negative default clauses
   **/
  static std::tuple<std::vector<Clause>,std::vector<Clause>> filterClauses(const std::vector<Clause>& clauses);
  /**
   * If there is some element x of arbiter_assignment such that conflict contains -x this method returns false.
   * Otherwise it returns true.
   **/
  static bool isConflictEntailed(const std::vector<int>& conflict,const std::unordered_map<int, bool>& arbiter_assignment);
  static std::pair<bool,Clause> removeArbiters(const Clause& clause, const std::unordered_map<int, bool>& arbiter_assignment);
  int getAIGERRepresentation(int x) const;
  int getAIGERNegation(int x) const;
};

inline int AIGERBuilder::getAIGERRepresentation(int x) const {
  return x > 0 ? 2*x : -2*x+1;
}

inline int AIGERBuilder::getAIGERNegation(int x) const {
  return x^1;
}


}

#endif