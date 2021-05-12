#ifndef PEDANT_CONFIGURATION_H_
#define PEDANT_CONFIGURATION_H_

#include <string>

namespace pedant {

struct Configuration {
  bool extract_cnf_model=false;
  std::string cnf_model_filename="";

  bool extract_aig_model=false;
  std::string aig_model_filename="";

  bool extract_aag_model=false;
  std::string aag_model_filename="";
};

}

#endif