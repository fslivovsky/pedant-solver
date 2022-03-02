#ifndef PEDANT_AIGERCNFCONVERTER_H_
#define PEDANT_AIGERCNFCONVERTER_H_

#include <string>
#include <unordered_map>

extern "C" {
    #include "aiger.h"
}

namespace pedant {

class CNFConverter {

 public:
  CNFConverter(const std::string& filename);
  ~CNFConverter();
  void writeCNFModel(const std::string& filename);

 private:
  std::unordered_map<int,int> renamings;
  int fresh_variable_start=0;
  int nof_clauses=0;
  aiger* circuit;
  void handleSymbol(const aiger_symbol& sym);
  void handleAnd(const aiger_and& a,std::ostream& out);
  int getRenaming(int variable);
};
  
}

#endif