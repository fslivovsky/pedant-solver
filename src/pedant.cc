#include <iostream>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <iomanip>
#include <string>

#include <csignal> 

#include "dqdimacs_parser.h"
#include "solvertypes.h"
#include "solver.h"
#include "logging.h"
#include "configuration.h"
#include "preprocessor.h"
#include "interrupt.h"


static constexpr int VERSION_MAJOR = 1;
static constexpr int VERSION_MINOR = 0;
static constexpr int VERSION_PATCH = 0;
void version();

void help();

int main(int argc, char* argv[]) {
  pedant::Logger::get().setOutputLevel(pedant::Loglevel::info);
  if (argc==1) {
    std::cout<<"Please give a formula to process."<<std::endl;
    help();
    return 0;
  }
  for (int i=1;i<argc;i++) {
    std::string arg(argv[i]);
    if ((arg=="-h") || (arg=="--help")) {
      help();
      return 0;
    } else if ((arg=="-v") || (arg=="--version")) {
      version();
      return 0;
    }
  }
  pedant::Configuration config;
  std::string filename(argv[1]);
  for (int i=2;i<argc;i++) {
    std::string s(argv[i]);
    if (s=="--cnf") {
      if (i+1<argc) {
        config.extract_cnf_model=true;
        config.cnf_model_filename=argv[i+1];
        i++;
      }
    } else if (s=="--aag") {
      if (i+1<argc) {
        config.extract_aag_model=true;
        config.aag_model_filename=argv[i+1];
        i++;
      }
    } else if (s=="--aig") {
      if (i+1<argc) {
        config.extract_aig_model=true;
        config.aig_model_filename=argv[i+1];
        i++;
      }
    }
  }
  pedant::DQDIMACS_Parser parser;
  try {
    signal(SIGABRT, pedant::InterruptHandler::interrupt);
    signal(SIGTERM, pedant::InterruptHandler::interrupt);
    signal(SIGINT, pedant::InterruptHandler::interrupt);

    auto [nr_variables, universal_variables, dependency_map, matrix] = parser.ParseFormula(filename);
    pedant::Preprocessor preprocessor(universal_variables, dependency_map, matrix);
    preprocessor.preprocess();
    auto solver = pedant::Solver(universal_variables, dependency_map, matrix, config);
    int status = solver.solve();
    solver.printStatistics();
    if (status == 10) {
      std::cout << "SATISFIABLE" << std::endl;
    } else if (status == 20) {
      std::cout << "UNSATISFIABLE" << std::endl;
    } else {
      std::cout << "UNKNOWN" << std::endl;
    }
    return status;
  } catch (pedant::InvalidFormatException& e) {
    std::cerr << "Error parsing " << filename << ": " << e.what() << std::endl;
    help();
  }
}

void version() {
  std::cout<<"Pedant "<<VERSION_MAJOR<<"."<<VERSION_MINOR<<"."<<VERSION_PATCH<<std::endl;
}

void help() {
  std::cout<<"Pedant is a solver for dependency quantified boolean formulas (DQBF) in DQDIMACS format."<<std::endl;
  std::cout<<std::endl;
  std::cout<<"Usage:"<<std::endl;
  std::cout<<"pedant <Input> [Options]"<<std::endl;
  std::cout<<std::endl;
  std::cout<<"Arguments:"<<std::endl;
  std::cout<<std::setw(2)<<""<<std::setw(11)<<std::left<<"Input"<<"A file containing a DQDIMACS formula."<<std::endl;
  std::cout<<std::endl;
  std::cout<<"Optional arguments:"<<std::endl;
  std::cout<<std::setw(2)<<""<<std::setw(11)<<std::left<<"-h, --help"<<"Prints this message and terminates."<<std::endl;
  std::cout<<std::endl;
  std::cout<<std::setw(2)<<""<<std::setw(11)<<std::left<<"If the solver returns SAT a certificate can be generated. "<<std::endl;
  std::cout<<std::setw(2)<<""<<std::setw(11)<<std::left<<"--cnf file"<<"Represents the model by a set of clauses. Writes the certificate to file."<<std::endl;
  std::cout<<std::setw(2)<<""<<std::setw(11)<<std::left<<"--aag file"<<"Represents the model as an AIGER in ASCII format. Writes the certificate to file."<<std::endl;
  std::cout<<std::setw(2)<<""<<std::setw(11)<<std::left<<"--aig file"<<"Represents the model as an AIGER in binary format. Writes the certificate to file."<<std::endl;
}