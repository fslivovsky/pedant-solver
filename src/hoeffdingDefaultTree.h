#ifndef PEDANT_HOEFFDINGDEFAULTTREES_H_
#define PEDANT_HOEFFDINGDEFAULTTREES_H_

#include <vector>
#include <unordered_map>
#include <memory>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <tuple>

#include <mlpack/methods/hoeffding_trees/hoeffding_tree.hpp>

#include "solvertypes.h"
#include "configuration.h"
#include "dependencycontainer.h"

namespace pedant
{

class HoeffdingDefaultTree {

 public:
  HoeffdingDefaultTree(int variable, 
      const DependencyContainer& dependencies,
      std::vector<int>& assumptions, std::vector<int>& default_assumptions, 
      int& last_used_variable, const Configuration& config);
  std::vector<Clause> insertConflict(int forced_literal,const std::vector<int>& universal_assignment);
  /**
   * Returns the clausal representation of the tree. 
   * For this purpose only clauses with an active selector are considered.
   **/
  std::vector<Clause> retrieveCompleteRepresentation() const;
  /**
   * The samples are inserted in batch mode. Thus the previous content of the tree is overwritten. 
   * Only use this method for empty trees.
   * The elements of samples shall be sorted with respect to the absolute values of the samples.
   **/
  std::vector<Clause> insertSamples(const std::vector<int>& labels, const std::vector<std::vector<int>>& samples);
  bool empty() const;
  void logTree(std::ostream& out) const;
  void visualiseTree() const;
  
  class Tree;

 private:
  void getLeaf(mlpack::tree::HoeffdingTree<>*& tree, Tree*& selector_tree, arma::vec& point);
  static bool isLeaf(const mlpack::tree::HoeffdingTree<>& tree);
  int newSelector(); 
  int getSplitVariable(const mlpack::tree::HoeffdingTree<>& tree) const;

  std::vector<Clause> getClauses(mlpack::tree::HoeffdingTree<>& t, Tree* st, bool empty_base);


  void setupClauses(const mlpack::tree::HoeffdingTree<>& t, Tree& st, const std::vector<int>& path, std::vector<Clause>& clauses);

  void setSwitches(Tree& st, int majority_class);
  void toggleSwitches(Tree& st);

  /**
   * If the insertSamples method was used to insert samples in batch mode into the tree
   * this method sets-up the internal tree representation and retrieves the clauses representing the leafs of the tree. 
   **/
  void getClausesFromBatchTree(const mlpack::tree::HoeffdingTree<>& htree, Tree& st, std::vector<int>& path, std::vector<Clause>& clauses);

  /**
   * Collects all active clauses, representing leafs of the tree.
   **/
  void traverseTree(const Tree& st, std::vector<int>& path, std::vector<Clause>& clauses) const;

  void writeTree(std::ostream& out, const Tree& st, std::vector<int>& path) const;
  void writeRule(std::ostream& out, const std::vector<int>& path, int label) const;

  void writeTreeVisualisation(std::ostream& out, const Tree& st, int& id_counter) const;
  void addEdgeToVisualisation(std::ostream& out, const Tree& parent, const Tree& child, bool left_child, int& id_counter, int parent_id) const;

  int variable;
  int& last_used_variable;
  std::vector<int> dependencies;
  std::vector<int>& assumptions;
  std::vector<int>& default_assumptions;
  mlpack::tree::HoeffdingTree<> htree;
  
  std::unique_ptr<Tree> root_selector_tree;

  const Configuration& config;

  int total_number_of_samples = 0;

 public:
  class Tree {
   public:
    Tree();
    Tree(Tree* parent, bool left_child, int idx1, int idx2);
    Tree(Tree* parent, bool left_child, int idx_left_1, int idx_left_2, int idx_right_1, int idx_right_2);

    std::tuple<Tree&,Tree&> setChildren(HoeffdingDefaultTree& ht, int split);
    std::tuple<Tree&,Tree&> setChildren(HoeffdingDefaultTree& ht, int split, bool left_child_leaf, bool right_child_leaf);
    bool isLeaf() const;
    Tree& subtree(bool left);
    const Tree& subtree(bool left) const;
    void getPath(std::vector<int>& path) const;

    const int selector_assumptions_index;
    const int selector_default_assumptions_index;

    const int selector_assumptions_index2;
    const int selector_default_assumptions_index2;

    int split_variable;

   private:
    bool left_child;
    Tree* parent;
    std::unique_ptr<Tree> left_subtree;
    std::unique_ptr<Tree> right_subtree;
  };

  friend Tree;

};


//HoeffdingDefaultTree

inline bool HoeffdingDefaultTree::isLeaf(const mlpack::tree::HoeffdingTree<>& tree) {
  assert (tree.NumChildren()<=2);
  return tree.NumChildren()==0;
}

inline int HoeffdingDefaultTree::newSelector() {
  int selector = ++last_used_variable;
  assumptions.push_back(last_used_variable);
  default_assumptions.push_back(last_used_variable);
  return selector;
}

inline bool HoeffdingDefaultTree::empty() const {
  return isLeaf(htree);
}

inline int HoeffdingDefaultTree::getSplitVariable(const mlpack::tree::HoeffdingTree<>& tree) const {
  int direction = tree.SplitDimension();
  assert(direction!=-1);//not a leaf
  return dependencies[direction];
}

//Tree

inline HoeffdingDefaultTree::Tree::Tree() : parent(nullptr),
    selector_assumptions_index(0),selector_default_assumptions_index(0), 
    selector_assumptions_index2(0),selector_default_assumptions_index2(0) {
}



inline HoeffdingDefaultTree::Tree::Tree(HoeffdingDefaultTree::Tree* parent, bool left_child, int idx_left_1, int idx_left_2, int idx_right_1, int idx_right_2) :
    selector_assumptions_index(idx_left_1),selector_default_assumptions_index(idx_left_2), 
    selector_assumptions_index2(idx_right_1),selector_default_assumptions_index2(idx_right_2),
    parent(parent), left_child(left_child) {
}



inline std::tuple<HoeffdingDefaultTree::Tree&,HoeffdingDefaultTree::Tree&> HoeffdingDefaultTree::Tree::setChildren(HoeffdingDefaultTree& ht, int split) {
  split_variable = split;
  left_subtree = std::make_unique<HoeffdingDefaultTree::Tree>(this,true,ht.assumptions.size(), ht.default_assumptions.size(), ht.assumptions.size()+1, ht.default_assumptions.size()+1);
  right_subtree = std::make_unique<HoeffdingDefaultTree::Tree>(this,false,ht.assumptions.size()+2, ht.default_assumptions.size()+2, ht.assumptions.size()+3, ht.default_assumptions.size()+3);
  ht.newSelector();
  ht.newSelector();
  ht.newSelector();
  ht.newSelector();
  return std::tie(*left_subtree, *right_subtree);
}

inline std::tuple<HoeffdingDefaultTree::Tree&,HoeffdingDefaultTree::Tree&> HoeffdingDefaultTree::Tree::setChildren(
    HoeffdingDefaultTree& ht, int split, bool left_child_leaf, bool right_child_leaf) {
  split_variable = split;
  if (left_child_leaf) {
    left_subtree = std::make_unique<HoeffdingDefaultTree::Tree>(this,true,ht.assumptions.size(), ht.default_assumptions.size(), ht.assumptions.size()+1, ht.default_assumptions.size()+1);
    ht.newSelector();
    ht.newSelector();
  } else {
    left_subtree = std::make_unique<HoeffdingDefaultTree::Tree>(this,true,0,0,0,0);
  }
  if (right_child_leaf) {
    right_subtree = std::make_unique<HoeffdingDefaultTree::Tree>(this,true,ht.assumptions.size(), ht.default_assumptions.size(), ht.assumptions.size()+1, ht.default_assumptions.size()+1);
    ht.newSelector();
    ht.newSelector();
  } else {
    right_subtree = std::make_unique<HoeffdingDefaultTree::Tree>(this,true,0,0,0,0);
  }
  return std::tie(*left_subtree, *right_subtree);
}


inline bool HoeffdingDefaultTree::Tree::isLeaf() const {
  return !left_subtree; // if the pointer is null then also right_subtree is
}

inline HoeffdingDefaultTree::Tree& HoeffdingDefaultTree::Tree::subtree(bool left) {
  return left ? *left_subtree : *right_subtree;
}

inline const HoeffdingDefaultTree::Tree& HoeffdingDefaultTree::Tree::subtree(bool left) const {
  return left ? *left_subtree : *right_subtree;
}

inline void HoeffdingDefaultTree::Tree::getPath(std::vector<int>& path) const {
  if (parent != nullptr) {
    if (left_child) {
      path.push_back(parent->split_variable);
    } else {  
      path.push_back(-parent->split_variable);
    }
    const HoeffdingDefaultTree::Tree& t = *parent;
    t.getPath(path);   
  }
}

  
} // namespace pedant




#endif