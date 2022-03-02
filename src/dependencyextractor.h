#ifndef PEDANT_DEPENDENCYEXTRACTOR_H_
#define PEDANT_DEPENDENCYEXTRACTOR_H_

#include <vector>
#include <unordered_map>
#include <tuple>

#include "solvertypes.h"
#include "configuration.h"

#include "dqdimacs.h"

namespace pedant {

class DependencyExtractor {

 public:
  /**
   * Formula remains unchaged with the exception that variable blocks may be reordered.
   */
  DependencyExtractor(DQDIMACS& formula, const Configuration& config);

  std::unordered_map<int, std::vector<int>> getDependencies();
  std::pair<std::unordered_map<int, std::vector<int>>, std::unordered_map<int, std::vector<int>>> getExtendedDependencies();


 private:
  //set original dependencies
  void computeDependencies();
  std::unordered_map<int, std::vector<int>> computeExtendedDependencies();

  //add var to the extended dependencies of all suitable variables with implicit dependencies
  //add all suitable variables with implicit dependencies to the extendeddependencies of var 
  void checkImplicitdependencies(int var, std::unordered_map<int, std::vector<int>>& extended_dependencies);

  int getMaximalSubSet(int var, int start_index, int end_index);
  int getMinimalSuperSet(int var, int start_index, int end_index);

  //add a variable with explicit dependencies to the extended dependencies of variables with implicit dependencies
  void addExplicitToImplicit(int var, int idx, std::unordered_map<int, std::vector<int>>& extended_dependencies);

  //add variables with implicit dependencies to the extended dependencies of a variable with explicit dependencies
  void addImplicitToExplicit(int var, int idx, std::unordered_map<int, std::vector<int>>& extended_dependencies);

  void checkExplicitDependencies(int var, const std::vector<int>& processed, std::unordered_map<int, std::vector<int>>& extended_dependencies);

  /**
   * If there are many variables in the range 1 to max_var that are unused in the matrix this has a negative impact on the memory requirements of this method
   **/
  std::unordered_map<int, std::vector<int>> applyDependencyScheme() const;


  std::unordered_map<int, std::vector<int>> computeInverseDependencies() const;
  std::unordered_map<int,std::vector<int>> getContainingClauses() const;
  void searchPaths(int literal_to_start,std::vector<bool>& toConsider,std::vector<bool>& marked,  std::unordered_map<int, std::vector<int>>& containing_clauses) const;
  void setUpSearchVectors(int universal, std::vector<bool>& to_consider, std::vector<bool>& marked_positive,std::vector<bool>& marked_negative, const std::vector<int>& inverse_deps) const;
  void revertSearchVectors(int universal, std::vector<bool>& to_consider, const std::vector<int>& inverse_deps) const;

  const int max_variable_in_matrix;
  int existential_block_idx_end;
  const Configuration& config;
  DQDIMACS& formula;

  std::unordered_map<int, std::vector<int>> original_dependency_map;

};



}

#endif

