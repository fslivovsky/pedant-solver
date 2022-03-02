#ifndef DQDIMACS_H_
#define DQDIMACS_H_

#include <vector>
#include <unordered_map>
#include <fstream>
#include <exception>
#include <string>

namespace pedant {

class DQDIMACS {

 public:
  DQDIMACS();
  DQDIMACS(int max_var);


  void addClause(const std::vector<int>& clause);
  void addUniversalBlock(const std::vector<int>& universal_block);
  void addExistentialBlock(const std::vector<int>& existential_block);
  void addExplicitDependencies(int var, const std::vector<int>& dependencies);

  int getMaxVar() const;
  bool firstBlockType() const;
  bool lastBlockType() const;

  int getNofUniversalBlocks() const;
  int getNofExistentialBlocks() const;

  std::vector<std::vector<int>>& getMatrix();
  std::vector<int>& getUniversals();
  /**
   * Get the existentials with implicit dependencies.
   **/
  std::vector<int>& getExistentials();
  std::vector<int> getAllExistentials();

  std::vector<int>& getUniversalBlocks();
  std::vector<int>& getExistentialBlocks();

  std::unordered_map<int, std::vector<int>>& getExplicitDependencies();

 private:
  int max_var;
  bool check_max_var;
  std::vector<std::vector<int>> matrix;
  bool first_block_type; //true: universal block, false: existential block
  std::vector<int> universal_variables;
  std::vector<int> universal_blocks;
  std::vector<int> existential_variables;
  std::vector<int> existential_blocks;

  std::unordered_map<int,std::vector<int>> explicit_dependencies;
  

};



}

#endif