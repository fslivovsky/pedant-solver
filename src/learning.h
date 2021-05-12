#ifndef PEDANT_LEARNING_H_
#define PEDANT_LEARNING_H_

#include <mlpack/methods/decision_tree/decision_tree.hpp>
#include <vector>

#include "solvertypes.h"

namespace pedant {

/**
 * TODO: Currently the parameters of the tree are set such that a high accuracy on the training samples is achieved. 
 * Possible problem overfitting. -> Maybe try other parameters.
 **/
class Learner {

 public:


  /**
   * @param existential_variable The existential variable for which we want to learn clauses
   * @param universal_dependencies The dependencies of the existential variable.
   * //TODO: Add configuration object to set the parameters of the tree (e.g. max depth, min nof samples per leaf)
   **/
  Learner(int existential_variable,const std::vector<int>& universal_dependencies);


  /**
   * Learns a decision tree from the stored samples and represents the learned tree as a CNF. 
   * @param discard_changes If true, then all stored samples and labels are discarded after their usage.
   * @return The learned formula
   **/
  std::vector<Clause> giveCNFRepresentation(bool discard_changes=true);


  /**
   * @param universal_assignment A variable assignment represented as a term. Has to assign each variable in universal_dependencies.
   *    Example: For variables [1,2,3] an assignment can be given by [-1,2,-3]
   * @param existential_assignment Gives an assignment for the variable existential_to_consider.
   *    A positive number indicates a positive assignment and a non-positive number indicates a negative assignmet.
   **/
  void addSample(const std::vector<int>& universal_assignment,int existential_assignment);


  /**
   * This method has the same purpose as addSample. But:
   *    - universal_assignment has to be ordered with respect to the ordering x<y := abs(x)<abs(y)
   *    - all elements x of universal_assignment have to satisfy the condition abs(x) is contained in universal_dependencies
   * 
   * @param universal_assignment A variable assignment represented as a term. Has to assign each variable in universal_dependencies.
   *    Example: For variables [1,2,3] an assignment can be given by [-1,2,-3]
   * @param existential_assignment Gives an assignment for the variable existential_to_consider.
   *    A positive number indicates a positive assignment and a non-positive number indicates a negative assignmet.
   **/
  void addReducedOrderedSample(const std::vector<int>& universal_assignment,int existential_assignment);

  /**
   * Discards all learned samples and labels.
   **/
  void resetSamples();

  /**
   * Returns the current number of samples.
   **/
  size_t nrSamples();

 private:
  int existential_to_consider;
  std::vector<int> universal_dependencies;
  mlpack::tree::DecisionTree<> tree;
  mlpack::data::DatasetInfo datasetInfo;
  arma::vec point;

  std::vector<std::vector<int>> stored_samples;
  std::vector<int> stored_labels;

  /**
   * If no tree was learned yet, then this variable is meaningless
   * If a tree was learned, then the variable is true iff the last learned tree classified all
   * training samples correctly.
   **/
  bool correct_classification_in_last_learned_tree;

  /**
   * Gives a vector that may contain arbitrary elements, except at the i-th position where it has to contain 1
   **/
  const arma::vec& givePointWithPositiveIthFeature(size_t i);

   /**
   * Gives a vector that may contain arbitrary elements, except at the i-th position where it has to contain 0
   **/
  const arma::vec& givePointWithNegativeIthFeature(size_t i);

  
  std::vector<std::vector<int>> computePaths(const mlpack::tree::DecisionTree<>& t,int level=1);

  /**
   * Trains a decision tree from the stored samples and discards all stored data.
   **/
  void train();

  /**
   * Intended to check whether the computed learned "model" is consistent with the training samples.
   * @param formula The formula that shall be checked.
   * @return true if the formula is consistent with the training samples.
   **/
  bool checkConsistency(std::vector<Clause>& formula);

};


inline void Learner::resetSamples() {
  stored_samples.clear();
  stored_labels.clear();
}

inline void Learner::addSample(const std::vector<int>& assignment,int existential_assignment) {
  stored_labels.push_back(existential_assignment);
  std::vector<int> sample;
  sample.reserve(universal_dependencies.size());
  for (int l:assignment) {
    if (std::binary_search(universal_dependencies.begin(),universal_dependencies.end(),abs(l))) {
      sample.push_back(l);
    }
  }
  std::sort(sample.begin(),sample.end(), [](int a,int b) {return abs(a)<abs(b);});
  stored_samples.push_back(sample);
}

inline void Learner::addReducedOrderedSample( const std::vector<int>& assignment,
                                              int existential_assignment) {
  stored_labels.push_back(existential_assignment);
  std::vector<int> sample(assignment.begin(),assignment.end());
  stored_samples.push_back(sample);
}

inline const arma::vec& Learner::givePointWithPositiveIthFeature(size_t i) {
  point[i]=1;
  return point;
}

inline const arma::vec& Learner::givePointWithNegativeIthFeature(size_t i) {
  point[i]=0;
  return point;
}

inline size_t Learner::nrSamples() {
  return stored_labels.size();
}

}


#endif