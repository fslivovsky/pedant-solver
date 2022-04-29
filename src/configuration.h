#ifndef PEDANT_CONFIGURATION_H_
#define PEDANT_CONFIGURATION_H_

#include <string>
#include <vector>

namespace pedant {

enum SatSolverType {Cadical, Glucose};
enum ConflictStrategy {Core, MinSeparator};
enum DefaultStrategy {Values, Functions};

struct Configuration {
  bool apply_dependency_schemes = true;
  bool apply_forall_reduction = true;
  bool ignore_innermost_existentials = false;
  bool check_definability_of_innermost_existentials = false;
  bool extended_dependencies = true;
  bool dynamic_dependencies = true;
  bool always_add_arbiter_clause = false;
  bool definitions = true;
  bool conditional_definitions = false;
  bool check_for_unates = false;
  bool use_forcing_clauses = true;
  bool check_for_fcs_matrix = false;
  bool allow_arbiters_in_forcing_clauses = false;
  bool replace_arbiters_in_separators = true;


  bool use_conflict_limit_unate_solver = false;
  int conflict_limit_unate_solver = 1000;

  int conflict_limit_definability_checker = 1000;
  int incremental_definability_max_iterations = 2;

  int def_limit = 1;

  int verbosity = 1;

  bool random_seed_set=false;
  int random_seed;

  bool extract_cnf_model = false;
  std::string cnf_model_filename = "";

  bool extract_aig_model = false;
  std::string aig_model_filename = "";

  bool extract_aag_model = false;
  std::string aag_model_filename = "";

  ConflictStrategy sup_strat = MinSeparator;

  // SatSolverType background_solver = Cadical;

  SatSolverType arbiter_solver = Cadical;
  SatSolverType validity_solver = Cadical;
  SatSolverType conflict_solver = Cadical;
  SatSolverType consistency_solver = Cadical;
  SatSolverType definability_solver = Cadical;
  SatSolverType unate_solver = Cadical;

  //Configuration for Hoeffdingtrees in the default_container

  DefaultStrategy def_strat = Functions;

  bool use_existentials_in_tree = false;
  bool add_samples_for_arbiters = false;
  int min_number_samples_in_node = 5;
  bool use_max_number=true;
  int max_number_samples_in_node = 20;
  int check_intervall = 5;

  bool return_default_tree = false;
  std::string write_default_tree_to = "";
  bool visualise_default_trees = false;
  std::string write_visualisations_to = "";
  bool log_default_tree=false;
  std::string write_default_tree_log_to = "";

  bool log_conflict_graphs = false;
  std::string conflict_graph_log_dir = "";

  bool batch_learning_for_samples = false;
  bool use_sampling=false;
  int nof_samples;

};

}

#endif