#ifndef PEDANT_AIGERDEPENDENCYCHECKER_H_
#define PEDANT_AIGERDEPENDENCYCHECKER_H_

#include <vector>
#include <unordered_map>
#include <set>
#include <string>
extern "C" {
    #include "aiger.h"
}

namespace pedant {

class DependencyChecker {

 public:
  DependencyChecker(const std::string& filename, std::unordered_map<int,std::vector<int>>& deps);
  ~DependencyChecker();
  bool check(bool verbose);
 private:
  aiger* circuit;
  void computeDependencies();
  std::vector<int> inputs;
  std::unordered_map<int,std::vector<int>>& given_dependencies;
  std::unordered_map<int,std::set<int>> computed_dependencies;
};



}

#endif