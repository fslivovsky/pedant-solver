#include "AigerCnfConverter.h"
#include <algorithm> 
#include <fstream>
#include <sstream>

namespace pedant {

CNFConverter::CNFConverter(const std::string& filename) {
  circuit=aiger_init();
  aiger_open_and_read_from_file(circuit,filename.c_str());
  for (int i=0;i<circuit->num_inputs;i++) {
    handleSymbol(circuit->inputs[i]);
  }
  for (int i=0;i<circuit->num_outputs;i++) {
    int variable = std::stoi(circuit->outputs[i].name);
    fresh_variable_start=std::max(fresh_variable_start,variable);
  }
}

CNFConverter::~CNFConverter() {
  aiger_reset(circuit);
}

void CNFConverter::handleSymbol(const aiger_symbol& sym) {
  int variable = std::stoi(sym.name);
  renamings[sym.lit]=variable;
  renamings[(sym.lit)+1]=-variable;
  fresh_variable_start=std::max(fresh_variable_start,variable);
}

void CNFConverter::handleAnd(const aiger_and& a,std::ostream& out) {
  int lhs=getRenaming(a.lhs);
  if (a.rhs0==0||a.rhs1==0) {
    out<<-lhs<<" 0"<<std::endl;
    nof_clauses++;
  } else if (a.rhs0==1) {
    if (a.rhs1==1) {
      out<<lhs<<" 0"<<std::endl;
      nof_clauses++;
    }
    int rhs=getRenaming(a.rhs1);
    out<<lhs<<" "<<-rhs<<" 0"<<std::endl;
    out<<-lhs<<" "<<rhs<<" 0"<<std::endl;
    nof_clauses+=2;
  } else if (a.rhs1==1) {
    int rhs=getRenaming(a.rhs0);
    out<<lhs<<" "<<-rhs<<" 0"<<std::endl;
    out<<-lhs<<" "<<rhs<<" 0"<<std::endl;
    nof_clauses+=2;
  } else {
    int rhs0 = getRenaming(a.rhs0);
    int rhs1 = getRenaming(a.rhs1);
    out<<lhs<<" "<<-rhs0<<" "<<-rhs1<<" 0"<<std::endl;
    out<<-lhs<<" "<<rhs0<<" 0"<<std::endl;
    out<<-lhs<<" "<<rhs1<<" 0"<<std::endl;
    nof_clauses+=3;
  }
}

int CNFConverter::getRenaming(int variable) {
  int var;
  if (renamings.find(aiger_strip(variable))==renamings.end()) {
    var=++fresh_variable_start;
    renamings[aiger_strip(variable)]=var;
  } else {
    var = renamings[aiger_strip(variable)];
  }

  return (variable&1)==0?var:-var;
}

void CNFConverter::writeCNFModel(const std::string& filename) {
  std::stringstream output;
  for (int i=0;i<circuit->num_ands;i++) {
    handleAnd(circuit->ands[i],output);
  }
  for (int i=0;i<circuit->num_outputs;i++) {
    int variable = std::stoi(circuit->outputs[i].name);
    int circuit_out = circuit->outputs[i].lit;
    if (circuit_out==0) {
      output<<-variable<<" 0"<<std::endl;
      nof_clauses++;
    } else if (circuit_out==1) {
      output<<variable<<" 0"<<std::endl;
      nof_clauses++;
    } else {
      int x =getRenaming(circuit_out);
      output<<variable<<" "<<-x<<" 0"<<std::endl;
      output<<-variable<<" "<<x<<" 0"<<std::endl;
      nof_clauses+=2;
    }
  }
  std::ofstream writer(filename);
  writer<<"p cnf "<<fresh_variable_start<<" "<<nof_clauses<<std::endl;
  writer<<output.str();
  writer.close();
}

}

int main(int argc, char* argv[]) {
  if (argc<3) {
    return -1;
  }
  std::string filename_in(argv[1]);
  std::string filename_out(argv[2]);
  pedant::CNFConverter converter(filename_in);
  converter.writeCNFModel(filename_out);
}