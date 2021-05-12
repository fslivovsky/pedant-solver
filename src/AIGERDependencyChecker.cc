#include "AIGERDependencyChecker.h"
#include <algorithm>
#include <unordered_map>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>

#include <iostream>

namespace pedant {

DependencyChecker::DependencyChecker(const std::string& filename, std::unordered_map<int,std::vector<int>>& deps) : given_dependencies(deps) {
  circuit=aiger_init();
  // for (int i=0;i<given_dependencies.size();i++) {
  //   given_dependencies[i];
  //   // std::sort(given_dependencies[i].begin(),given_dependencies[i].end());
  // }
  aiger_open_and_read_from_file(circuit,filename.c_str());
  for (int i=0;i<circuit->num_inputs;i++) {
    auto x = circuit->inputs[i];
    int var = x.lit&~1;
    computed_dependencies[var] = {std::stoi(x.name)};
  }
}

DependencyChecker::~DependencyChecker() {
  aiger_reset(circuit);
}

void DependencyChecker::computeDependencies() {
  for (int i=0;i<circuit->num_ands;i++) {
    auto a = circuit->ands[i];
    int lhs = a.lhs&~1;
    int rhs0 = a.rhs0&~1;
    int rhs1 = a.rhs1&~1;
    computed_dependencies[lhs] = {};
    if (rhs0!=0) {
      computed_dependencies[lhs].insert(computed_dependencies[rhs0].begin(),computed_dependencies[rhs0].end());
    }
    if (rhs1!=0) {
      computed_dependencies[lhs].insert(computed_dependencies[rhs1].begin(),computed_dependencies[rhs1].end());
      // computed_dependencies[lhs] = computed_dependencies[rhs1];
    }
  }
}

bool DependencyChecker::check(bool verbose) {
  if (given_dependencies.size()!=circuit->num_outputs) {
    return false;
  }
  computeDependencies();
  for (int i=0;i<circuit->num_outputs;i++) {
    int out=circuit->outputs[i].lit;
    out = out&~1;
    int var = std::stoi(circuit->outputs[i].name);
    if (out!=0) {
      // std::vector<int> deps=computed_dependencies[out];
      // std::sort(deps.begin(),deps.end());
      std::set<int> deps = computed_dependencies[out];


      if(!std::includes(given_dependencies[var].begin(),given_dependencies[var].end(),deps.begin(),deps.end())) {
        if (verbose) {
          std::cout<<"Dependencies violated."<<std::endl;
          std::cout<<"Variable: "<<var<<std::endl;
          std::cout<<"Allowed Dependencies:"<<std::endl;
          for (int l:given_dependencies[var]) {
            std::cout<<l<<" ";
          }
          std::cout<<std::endl<<"Actual Dependencies:"<<std::endl;
          for (int l:deps) {
            std::cout<<l<<" ";
          }
          std::cout<<std::endl;
        }
        return false;
      }
    }    
  }
  return true;
}


}

int main(int argc, char* argv[]) {
  if (argc<3) {
    return -1;
  }
  std::string filename(argv[1]);
  std::string filename_dependencies(argv[2]);
  std::unordered_map<int,std::vector<int>> dependencies;
  std::ifstream in(filename_dependencies);
  if (!in) {
    return -1;
  }
  std::string line;
  while (std::getline(in, line))
  {
    std::stringstream st;    
    st<<line;
    std::string part;
    st>>part;
    int var=std::stoi(part);
    std::vector<int> x;
    while(!st.eof()) {
      st>>part;
      int d=std::stoi(part);
      x.push_back(d);
    }
    std::sort(x.begin(),x.end());
    dependencies[var]=x;
  }

  pedant::DependencyChecker checker(filename,dependencies);
  bool dependencies_ok = checker.check(true);
  if (dependencies_ok) {
    return 0;
  } else {
    return 1;
  }
}