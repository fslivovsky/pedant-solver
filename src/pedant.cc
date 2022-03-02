#include <iostream>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <iomanip>
#include <string>

#include <csignal> 

#include <docopt.h>

#include <iostream>
#include <limits>

#include "dqdimacs.h"
#include "dqdimacsparser.h"
#include "inputformula.h"
#include "solvertypes.h"
#include "solver.h"
#include "logging.h"
#include "configuration.h"
#include "preprocessor.h"
#include "interrupt.h"
#include "argumentconstraint.h"


static constexpr int VERSION_MAJOR = 2;
static constexpr int VERSION_MINOR = 0;
static constexpr int VERSION_PATCH = 0;


static const char USAGE[] =
R"(Pedant
Usage: 
  pedant [options] [FORMULA]
General Options:
  -h --help                     Print this message.
  -v --version                  Print the version number.
Solver Options:
  --rrs=bool                    Eliminiate dependencies with the RRS dependency scheme [default: true]
  --extended-dependencies=bool  Use extended dependencies [default: true]
  --dynamic-dependencies=bool   If all variables in the representation of a skolemfunction for e1
                                occur in the extended dependencies of e2 then e2 may use e1 [default: true]
  --unates=bool                 Detect unate clauses [default: false]
  --definitions=bool            Compute definitions [default: true]
  --always-add-arbiter=bool     Add arbiters in each iteration [default: false]
  --forcing-clauses=bool        Use forcing clauses (requires extended dependencies) [default: true]
  --fcs-matrix=bool             Extract forcing clauses from matrix [default: false]
  --unate-limit=int             Set the conflict limit for unate clause detection. [default: 2000]
  --no-conflict-limit-unates    Disable the conflict limit for unates.                  
  --definition-limit=int        Set the conflict limit for definability checks. [default: 1000]
Conflict Extraction Options:
  --support-strat=VAL           Strategy for the conflict extraction (core, minsep) 
                                core: Unsat core of falsifying assignment
                                minsep: Based on MaxFlow [default: minsep]
Background Sat Solver Options:  Supported Solvers (cadical, glucose)
  --sat-solver=VAL              Sets the default Sat solver [default: cadical]
  --arbitersolver=VAL           Set the SAT solver for the arbiter solver
  --validitychecker=VAL         Set the SAT solver for the validity checker
  --conflictextractor=VAL       Set the SAT solver for the conflict extractor
  --consistencychecker=VAL      Set the SAT solver for the consistency checker
  --definiabilitychecker=VAL    Set the SAT solver for the definability checks
  --unatechecker=VAL            Set the SAT solver for the unate checker
Default Value Options:          
  --default-strat=VAL           Sets the strategy for default values (values, functions)
                                values: Use default values
                                functions: Use default functions, (requires MLPack) [default: functions]
Default Function Options:       Options for the construction of default functions.
  --maxValsPerNode=VAL          Maximal number of samples in a node in the default tree. 0 for using no limit [default: 20]
  --minValsPerNode=VAL          Minimal number of samples in a node in the default tree. Must not be larger than minValsPerNode [default: 5]
  --checkIntervall=VAL          Check after VAL new samples if a node shall be split. [default: 5]
Certificate Options:
  --cnf FILE                    Write a clausal model to FILE.
  --aag FILE                    Write an ASCII AIGER model to FILE.
  --aig FILE                    Write a binary AIGER model to FILE.
)";



pedant::Configuration setConfigurations(std::map<std::string, docopt::value>& args);
void checkConfiguration(pedant::Configuration& config);
bool checkArguments(std::map<std::string, docopt::value>& args);

int main(int argc, char* argv[]) {
  std::string version_info = "";
  #ifdef USE_MACHINE_LEARNING
    version_info += "\nBuilt with support for MLPack";
  #endif

  std::map<std::string, docopt::value> args = docopt::docopt(USAGE,
    { argv + 1, argv + argc }, true, "Pedant "+std::to_string(VERSION_MAJOR)+"."+std::to_string(VERSION_MINOR)+"."+std::to_string(VERSION_PATCH)+version_info);  


  std::string filename;
  if (args["FORMULA"]) {
    filename = args["FORMULA"].asString();
  } else {
    std::cout<<"Please give a formula to process."<<std::endl;
    return 0;
  }
  if (!checkArguments(args)) {
    return 0;
  }
  pedant::Configuration config = setConfigurations(args);

  checkConfiguration(config);

  switch (config.verbosity) {
    case 1:
      pedant::Logger::get().setOutputLevel(pedant::Loglevel::trace);
      break;
    case 2:
      pedant::Logger::get().setOutputLevel(pedant::Loglevel::info);
      break;
    case 3:
      pedant::Logger::get().setOutputLevel(pedant::Loglevel::error);
      break;
  }

  pedant::DQDIMACSParser parser;
  try {
    signal(SIGABRT, pedant::InterruptHandler::interrupt);
    signal(SIGTERM, pedant::InterruptHandler::interrupt);
    signal(SIGINT, pedant::InterruptHandler::interrupt);
    pedant::DQDIMACS input = parser.parseFormula(filename);
    pedant::Preprocessor preprocessor(input, config);
    pedant::InputFormula formula = preprocessor.preprocess();
    

    auto solver = pedant::Solver(formula, config);
    int status = solver.solve();
    if (config.verbosity>0) {
      solver.printStatistics();
    }
    if (status == 10) {
      std::cout << "SATISFIABLE" << std::endl;
    } else if (status == 20) {
      std::cout << "UNSATISFIABLE" << std::endl;
    } else {
      std::cout << "UNKNOWN" << std::endl;
    }
    return status;
  } catch (pedant::InvalidFileException& e) {
    std::cerr << "Error parsing " << filename << ": " << e.what() << std::endl;
  }
}

bool checkArguments(std::map<std::string, docopt::value>& args) {
  vector<unique_ptr<ArgumentConstraint>> argument_constraints;

  argument_constraints.push_back(make_unique<BoolConstraint>("--rrs"));
  argument_constraints.push_back(make_unique<BoolConstraint>("--extended-dependencies"));
  argument_constraints.push_back(make_unique<BoolConstraint>("--dynamic-dependencies"));
  argument_constraints.push_back(make_unique<BoolConstraint>("--unates"));
  argument_constraints.push_back(make_unique<BoolConstraint>("--definitions"));
  argument_constraints.push_back(make_unique<BoolConstraint>("--always-add-arbiter"));
  argument_constraints.push_back(make_unique<BoolConstraint>("--forcing-clauses"));
  argument_constraints.push_back(make_unique<BoolConstraint>("--fcs-matrix"));
   

  argument_constraints.push_back(make_unique<IntRangeConstraint>(1, INT_MAX, "--unate-limit"));
  argument_constraints.push_back(make_unique<IntRangeConstraint>(1, INT_MAX, "--definition-limit"));
  argument_constraints.push_back(make_unique<IntRangeConstraint>(INT_MIN, INT_MAX, "--seed"));

  std::vector<std::string> possible_solvers {"cadical","glucose"};
  argument_constraints.push_back(make_unique<ListConstraint>(possible_solvers, "--sat-solver"));
  argument_constraints.push_back(make_unique<ListConstraint>(possible_solvers, "--arbitersolver"));
  argument_constraints.push_back(make_unique<ListConstraint>(possible_solvers, "--validitychecker"));
  argument_constraints.push_back(make_unique<ListConstraint>(possible_solvers, "--conflictextractor"));
  argument_constraints.push_back(make_unique<ListConstraint>(possible_solvers, "--consistencychecker"));
  argument_constraints.push_back(make_unique<ListConstraint>(possible_solvers, "--definiabilitychecker"));
  argument_constraints.push_back(make_unique<ListConstraint>(possible_solvers, "--unatechecker"));

  std::vector<std::string> support_startegies {"core", "minsep"};
  argument_constraints.push_back(make_unique<ListConstraint>(support_startegies, "--support-strat"));

  std::vector<std::string> default_startegies {"values", "functions"};
  argument_constraints.push_back(make_unique<ListConstraint>(default_startegies, "--default-strat"));

  argument_constraints.push_back(make_unique<IntRangeConstraint>(0, INT_MAX, "--maxValsPerNode"));
  argument_constraints.push_back(make_unique<IntRangeConstraint>(0, INT_MAX, "--minValsPerNode"));
  argument_constraints.push_back(make_unique<IntRangeConstraint>(1, INT_MAX, "--checkIntervall"));

  for (auto& constraint_ptr: argument_constraints) {
    if (!constraint_ptr->check(args)) {
      std::cout << constraint_ptr->message() << "\n\n";
      std::cout << USAGE;
      return false;
    }
  }
  return true;
}

bool isTrue(const std::string& val) {
  return val.compare("true")==0 || val.compare("1")==0;
}

void setSolver(pedant::SatSolverType& solver, const std::string& solver_to_use) {
  if (solver_to_use.compare("cadical")==0) {
    solver = pedant::Cadical;
  } else if (solver_to_use.compare("glucose")==0) {
    solver = pedant::Glucose;
  } 
}

void setAllBackgroundSolvers(pedant::Configuration& config, const std::string& solver_to_use) {
  pedant::SatSolverType solver;
  if (solver_to_use.compare("cadical")==0) {
    solver = pedant::Cadical;
  } else if (solver_to_use.compare("glucose")==0) {
    solver = pedant::Glucose;
  } 

  config.arbiter_solver = solver;
  config.validity_solver = solver;
  config.conflict_solver = solver;
  config.consistency_solver = solver;
  config.definability_solver = solver;
  config.unate_solver = solver;
}

pedant::Configuration setConfigurations(std::map<std::string, docopt::value>& args) {
  pedant::Configuration config;
  if (args["--cnf"]) {
    config.extract_cnf_model=true;
    config.cnf_model_filename=args["--cnf"].asString();
  } else {
    config.extract_cnf_model=false;
  }
  if (args["--aag"]) {
    config.extract_aag_model=true;
    config.aag_model_filename=args["--aag"].asString();
  } else {
    config.extract_aag_model=false;
  }
  if (args["--aig"]) {
    config.extract_aig_model=true;
    config.aig_model_filename=args["--aig"].asString();
  } else {
    config.extract_aig_model=false;
  }


  config.apply_dependency_schemes = isTrue(args["--rrs"].asString());
  config.extended_dependencies = isTrue(args["--extended-dependencies"].asString());
  config.dynamic_dependencies = isTrue(args["--dynamic-dependencies"].asString());
  config.always_add_arbiter_clause = isTrue(args["--always-add-arbiter"].asString());
  config.definitions = isTrue(args["--definitions"].asString());
  config.check_for_unates = isTrue(args["--unates"].asString());
  config.use_forcing_clauses = isTrue(args["--forcing-clauses"].asString());
  config.check_for_fcs_matrix = isTrue(args["--fcs-matrix"].asString());

  config.conflict_limit_definability_checker = args["--definition-limit"].asLong();
  config.conflict_limit_unate_solver = args["--unate-limit"].asLong();

  if((args["--no-conflict-limit-unates"].asBool())) {
    config.use_conflict_limit_unate_solver=false;
  }

  setAllBackgroundSolvers(config,args["--sat-solver"].asString());
  if (args["--arbitersolver"]) {
    setSolver(config.arbiter_solver,args["--arbitersolver"].asString());
  }

  if (args["--validitychecker"]) {
    setSolver(config.validity_solver,args["--validitychecker"].asString());
  }

  if (args["--conflictextractor"]) {
    setSolver(config.conflict_solver,args["--conflictextractor"].asString());
  }

  if (args["--consistencychecker"]) {
    setSolver(config.consistency_solver,args["--consistencychecker"].asString());
  }

  if (args["--definiabilitychecker"]) {
    setSolver(config.definability_solver,args["--definiabilitychecker"].asString());
  }

  if (args["--unatechecker"]) {
    setSolver(config.unate_solver,args["--unatechecker"].asString());
  }

  std::string supp_strat = args["--support-strat"].asString();
  if (supp_strat.compare("core")==0) {
    config.sup_strat = pedant::ConflictStrategy::Core;
  } else if (supp_strat.compare("minsep")==0) {
    config.sup_strat = pedant::ConflictStrategy::MinSeparator;
  }

  std::string def_strat = args["--default-strat"].asString();
  if (def_strat.compare("values")==0) {
    config.def_strat = pedant::DefaultStrategy::Values;
  } else if (def_strat.compare("functions")==0) {
    config.def_strat = pedant::DefaultStrategy::Functions;
  }


  config.max_number_samples_in_node = args["--maxValsPerNode"].asLong();
  config.use_max_number = config.max_number_samples_in_node > 0;
  config.min_number_samples_in_node = args["--minValsPerNode"].asLong();
  config.check_intervall  = args["--checkIntervall"].asLong();


  return config;
}

void checkConfiguration(pedant::Configuration& config) {

  if (!config.extended_dependencies && config.dynamic_dependencies) {
    std::cerr<<"Dynamic dependencies can only be used together with extended dependencies!"<<std::endl;
    config.dynamic_dependencies = false;
  }

  if (!config.extended_dependencies && config.use_forcing_clauses) {
    std::cerr<<"Forcing clauses can only be used together with extended dependencies!"<<std::endl;
    config.use_forcing_clauses = false;
  }

  if (!config.use_forcing_clauses && config.check_for_fcs_matrix) {
    std::cerr<<"The search for forcing clauses in the matrix only works if forcing clauses shall be used!"<<std::endl;
    config.check_for_fcs_matrix = false;
  }
  


  #ifndef USE_MACHINE_LEARNING
    if (config.def_strat == pedant::DefaultStrategy::Functions) {
      std::cerr<<"To use the default functions, the solver must be compiled with support for MLPack!"<<std::endl;
      config.def_strat = pedant::DefaultStrategy::Values;
    }
  #endif

  if (config.min_number_samples_in_node > config.max_number_samples_in_node) {
    std::cerr << "maxValsPerNode must not be smaller than minValsPerNode!" <<std::endl;
    config.min_number_samples_in_node = config.max_number_samples_in_node;
  }

  //Without dynamic dependencies there is no need for a second iteration of the definability check.
  if (!config.dynamic_dependencies) {
    config.incremental_definability_max_iterations = 1;
  }

}
