#include "learning.h"
#include "logging.h"
#include <cassert>

#include <iostream>



namespace pedant {

Learner::Learner(int existential_variable,const std::vector<int>& dependencies) :
    existential_to_consider(existential_variable),universal_dependencies(dependencies),
    tree(dependencies.size()), datasetInfo(dependencies.size()), point(dependencies.size()) {
  
  std::sort(universal_dependencies.begin(),universal_dependencies.end());
  for (int i=0;i<dependencies.size();i++) {
    datasetInfo.Type(i) = mlpack::data::Datatype::categorical;
    datasetInfo.MapString<double>("0", i);
    datasetInfo.MapString<double>("1", i);

    point[i]=0;
  }
}

std::vector<Clause> Learner::giveCNFRepresentation(bool discard_changes) {
  if (stored_samples.empty()) {
    return {};
  }
  train();
  if (discard_changes) {
    resetSamples();
  }
  return computePaths(tree);
}




void Learner::train() {
  arma::mat features(universal_dependencies.size(),stored_samples.size());
  arma::Row<size_t> labels(stored_samples.size());
  int column=0;
    for (auto& feature:stored_samples) {
      int row=0;
      for (int l:feature) {
        features(row,column)=l>0?1:0;
        row++;
      }
      labels[column]=stored_labels[column]>0;
      column++;
  }

  //TODO: Maybe increase the minimum number of samples per leaf -> overfitting
  //TODO: Also think about using maxDepth
  double entropy=tree.Train(std::move(features),datasetInfo,std::move(labels),2,1);

  if (entropy==0) {
    correct_classification_in_last_learned_tree=true;
    DLOG(trace) << "Decision tree for variable "<<existential_to_consider<<" correctly classifies all training samples." << std::endl;
  } else {
    correct_classification_in_last_learned_tree=false;
    DLOG(trace) << "Decision tree for variable "<<existential_to_consider<<" does not correctly classify all training samples." << std::endl;
  }
}

std::vector<std::vector<int>> Learner::computePaths(const mlpack::tree::DecisionTree<>& t,int level) {
  if (t.NumChildren()==0) {
    size_t cl=t.Classify(point);
    std::vector<int> clause={cl==1?existential_to_consider:-existential_to_consider};
    clause.reserve(level);
    return {clause};
  } else {
    assert (t.NumChildren()==2);
    level++;
    auto paths1=computePaths(t.Child(0),level);
    auto paths2=computePaths(t.Child(1),level);
    size_t decision_variable_index=t.SplitDimension();
    size_t dir=t.CalculateDirection(givePointWithPositiveIthFeature(decision_variable_index));

    //If dir==0 then the child with index 0 represents the positive decision on the variable given by
    //"decision_variable_index". As we want to compute a clausal representation of the tree we have
    //to negate the decision variable. This means if dir==0 we have to add the respective decision variable
    //negatively to the paths given by the child tree.
    int decision_literal=dir==1?universal_dependencies[decision_variable_index]:-universal_dependencies[decision_variable_index];
    for(auto& path:paths1) {
      path.push_back(decision_literal);
    }
    decision_literal=-decision_literal;
    for(auto& path:paths2) {
      path.push_back(decision_literal);
      paths1.push_back(path);
    }
    return paths1;
  }  
}

bool Learner::checkConsistency(std::vector<Clause>& formula) {
  return correct_classification_in_last_learned_tree;
}


}