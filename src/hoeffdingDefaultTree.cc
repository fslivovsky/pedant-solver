#include <algorithm>

#include "hoeffdingDefaultTree.h"
#include <string>

namespace pedant {

mlpack::data::DatasetInfo getDatasetInfo(int size) {
  mlpack::data::DatasetInfo dI (size);
  for (int i=0; i<size; i++) {
    dI.Type(i) = mlpack::data::Datatype::categorical;
    dI.MapString<double>("0", i);
    dI.MapString<double>("1", i);
  }
  return dI;
}

HoeffdingDefaultTree::HoeffdingDefaultTree(int variable, 
    const DependencyContainer& dependency_container,
    std::vector<int>& assumptions, std::vector<int>& default_assumptions, int& last_used_variable, const Configuration& config) :
    variable(variable), assumptions(assumptions), default_assumptions(default_assumptions), 
    dependencies(dependency_container.getDependencies(variable)), //TODO: check: what happens if extended deps shall be used fpor learning
    last_used_variable(last_used_variable), config(config), htree(getDatasetInfo(dependencies.size()), 2) {
  std::sort(dependencies.begin(),dependencies.end());
  if (config.use_max_number) {
    htree.MaxSamples(config.max_number_samples_in_node);
  }
  htree.MinSamples(config.min_number_samples_in_node);
  htree.CheckInterval(config.check_intervall);
}

std::vector<Clause> HoeffdingDefaultTree::insertConflict(int forced_literal, const std::vector<int>& universal_assignment) {
  total_number_of_samples++;
  std::vector<int> reduced_assm;
  reduced_assm.reserve(dependencies.size());
  for (int u:universal_assignment) {
    if (std::binary_search(dependencies.begin(),dependencies.end(), abs(u))) {
      reduced_assm.push_back(u);
    }
  }
  std::sort(reduced_assm.begin(), reduced_assm.end(), [](int i, int j) { return abs(i) < abs(j); });
  arma::vec sample(dependencies.size());
  for (int i=0; i<dependencies.size(); i++) {
    sample[i] = reduced_assm[i]>0 ? 1 : 0;
  }

  bool tree_empty = empty();
  mlpack::tree::HoeffdingTree<>* t = &htree;
  Tree* st = root_selector_tree.get();
  
  //we could train the "root"-tree with the sample. 
  //As we need to know which subtree was changed in order to get the respective selector we first classify
  getLeaf(t,st,sample);

  t->Train(sample,forced_literal>0);

  if (!isLeaf(*t)) {
    return getClauses(*t,st,tree_empty);
  } else if (st!=nullptr) {
    //check if we have to flip clause due to a change of the majority class
    toggleSwitches(*st);
  } 
  return {};
}

void HoeffdingDefaultTree::setSwitches(Tree& st, int majority_class) {
  if (majority_class == 1) {
    assumptions[st.selector_assumptions_index] = abs(assumptions[st.selector_assumptions_index]);
    assumptions[st.selector_assumptions_index2] = -abs(assumptions[st.selector_assumptions_index2]);
    default_assumptions[st.selector_default_assumptions_index] = abs(default_assumptions[st.selector_default_assumptions_index]);
    default_assumptions[st.selector_default_assumptions_index2] = -abs(default_assumptions[st.selector_default_assumptions_index2]);
  } else {
    assumptions[st.selector_assumptions_index] = -abs(assumptions[st.selector_assumptions_index]);
    assumptions[st.selector_assumptions_index2] = abs(assumptions[st.selector_assumptions_index2]);
    default_assumptions[st.selector_default_assumptions_index] = -abs(default_assumptions[st.selector_default_assumptions_index]);
    default_assumptions[st.selector_default_assumptions_index2] = abs(default_assumptions[st.selector_default_assumptions_index2]);
  }
}

void HoeffdingDefaultTree::toggleSwitches(Tree& st) {
  assumptions[st.selector_assumptions_index] = -assumptions[st.selector_assumptions_index];
  assumptions[st.selector_assumptions_index2] = -assumptions[st.selector_assumptions_index2];
  default_assumptions[st.selector_default_assumptions_index] = -default_assumptions[st.selector_default_assumptions_index];
  default_assumptions[st.selector_default_assumptions_index2] = -default_assumptions[st.selector_default_assumptions_index2];
}


std::vector<Clause> HoeffdingDefaultTree::getClauses(mlpack::tree::HoeffdingTree<>& t, Tree* st, bool empty_base) {
  int split = getSplitVariable(t);
  std::vector<int> path;
  if (empty_base) {
    root_selector_tree = std::make_unique<Tree>();
    st = root_selector_tree.get();
  } else {
    assumptions[st->selector_assumptions_index] = -abs(assumptions[st->selector_assumptions_index]);
    assumptions[st->selector_assumptions_index2] = -abs(assumptions[st->selector_assumptions_index2]);
    default_assumptions[st->selector_default_assumptions_index] = -abs(default_assumptions[st->selector_default_assumptions_index]);
    default_assumptions[st->selector_default_assumptions_index2] = -abs(default_assumptions[st->selector_default_assumptions_index2]);
    st->getPath(path);
  }
  auto [sub1,sub2] = st->setChildren(*this, split); 
  std::vector<Clause> clauses;
  path.push_back(split);
  setupClauses(t.Child(0),sub1,path,clauses);
  path.pop_back();
  path.push_back(-split);
  setupClauses(t.Child(1),sub2,path,clauses);
  path.pop_back();
  return clauses;
}



void HoeffdingDefaultTree::setupClauses(const mlpack::tree::HoeffdingTree<>& t, Tree& st, const std::vector<int>& path, std::vector<Clause>& clauses) {
  int majority_class = t.MajorityClass();
  Clause positive (path);
  Clause negative (path);

  int positive_selector = abs(default_assumptions[st.selector_default_assumptions_index]);
  int negative_selector = abs(default_assumptions[st.selector_default_assumptions_index2]);

  positive.push_back(-positive_selector);
  positive.push_back(variable);

  negative.push_back(-negative_selector);
  negative.push_back(-variable);

  clauses.push_back(positive);
  clauses.push_back(negative);

  setSwitches(st,majority_class);
}

void HoeffdingDefaultTree::getLeaf(mlpack::tree::HoeffdingTree<>*& tree, Tree*& selector_tree, arma::vec& point) {
  if (isLeaf(*tree)) {
    return;
  } else {
    int direction = tree->CalculateDirection(point);
    tree = &(tree->Child(direction));
    selector_tree = &(selector_tree->subtree(direction==0));
    return getLeaf(tree,selector_tree,point);
  }
}

void HoeffdingDefaultTree::logTree(std::ostream& out) const {
  std::vector<int> path;
  out<<"Tree for: "<<variable<<"\n";
  writeTree(out,*root_selector_tree.get(),path);
  out.flush();
}

void HoeffdingDefaultTree::writeTree(std::ostream& out, const Tree& st, std::vector<int>& path) const {
  if (st.isLeaf()) {
    if (assumptions[st.selector_assumptions_index]<0) { //the negative clause is active
      writeRule(out,path,-variable);
    } else {  //the positive clause is active
      writeRule(out,path,variable);
    }
  } else {
    int split = st.split_variable;
    path.push_back(split);
    writeTree(out,st.subtree(true),path);
    path.back()=-split;
    writeTree(out,st.subtree(false),path);
    path.pop_back();
  }

}

void HoeffdingDefaultTree::writeRule(std::ostream& out, const std::vector<int>& path, int label) const {
  std::vector<int> copy(path);
  std::sort(copy.begin(), copy.end(), [](int i, int j) { return abs(i) < abs(j); });
  for (int l : copy) {
    out<<l<<" ";
  }
  out<<"-> "<<label<<"\n";
}

//the tree must not be empty
void HoeffdingDefaultTree::visualiseTree() const {
  std::string filename = config.write_visualisations_to + "/graph_var_" + std::to_string(variable) + ".dot";
  std::ofstream file(filename);
  file<<"digraph var_" + std::to_string(variable) + " {\n";
  file<<"node[label=\"\" shape=point];\n";
  int id = 1;
  writeTreeVisualisation(file,*root_selector_tree.get(),id);
  file<<"}";
  file.close();
}

void HoeffdingDefaultTree::writeTreeVisualisation(std::ostream& out, const Tree& st, int& id_counter) const {
  const Tree& sub1 = st.subtree(true);
  int node_id = id_counter;
  addEdgeToVisualisation(out,st,sub1, true, id_counter, node_id);
  const Tree& sub2 = st.subtree(false);
  addEdgeToVisualisation(out,st,sub2, false, id_counter, node_id);
}



void HoeffdingDefaultTree::addEdgeToVisualisation(std::ostream& out, const Tree& parent, const Tree& child, 
                                                  bool left_child, int& id_counter, int parent_id) const {
  std::string edge_label = std::to_string(left_child ? parent.split_variable : -parent.split_variable);
  int child_id = ++id_counter;
  if (child.isLeaf()) {
    out<<std::to_string(parent_id) + " -> " + std::to_string(child_id) + "[label=\"" + edge_label + "\"]\n";
    if (assumptions[child.selector_assumptions_index]<0) {
      out<<std::to_string(child_id) + "[label=" + std::to_string(-variable)  + " shape=plaintext];\n";
    } else {
      out<<std::to_string(child_id) + "[label=" + std::to_string(variable)  + " shape=plaintext];\n";
    }
  } else {
    out<<std::to_string(parent_id) + " -> " + std::to_string(child_id) + "[label=\"" + edge_label + "\"]\n";
    writeTreeVisualisation(out,child,id_counter);
  }
}


std::vector<Clause> HoeffdingDefaultTree::insertSamples(const std::vector<int>& existential_value, const std::vector<std::vector<int>>& samples) {
  assert(empty());
  assert(existential_value.size()==samples.size());
  std::vector<size_t> indices_to_consider;
  indices_to_consider.reserve(dependencies.size());
  for (size_t i=0; i<samples[0].size(); i++) {
    auto l = samples[0][i];
    if (std::binary_search(dependencies.begin(),dependencies.end(),abs(l))) {
      indices_to_consider.push_back(i);
    }
  }

  arma::mat features(dependencies.size(),samples.size());
  arma::Row<size_t> labels(existential_value.size());
  for (size_t i=0; i<samples.size(); i++) {
    int idx = 0;
    for (auto j : indices_to_consider) {
      features(idx,i) = samples[i][j]> 0 ? 1 : 0;
      idx++;
    }
    labels[i] = existential_value[i] > 0 ? 1 : 0;
  }
  htree.Train(features,labels,config.batch_learning_for_samples);
  
  std::vector<Clause> clauses;
  if (!isLeaf(htree)) {
    root_selector_tree = std::make_unique<Tree>();
    std::vector<int> path;
    getClausesFromBatchTree(htree,*root_selector_tree,path,clauses);
  }
  return clauses;
}

void HoeffdingDefaultTree::getClausesFromBatchTree(const mlpack::tree::HoeffdingTree<>& ht, Tree& st, std::vector<int>& path, std::vector<Clause>& clauses) {
  if (isLeaf(ht)) {
    setupClauses(ht,st,path,clauses);
  } else {
    int split = getSplitVariable(ht);
    const auto ht1 = ht.Child(0);
    const auto ht2 = ht.Child(1);
    auto [sub1,sub2] = st.setChildren(*this, split, isLeaf(ht1), isLeaf(ht2)); 
    path.push_back(split);
    getClausesFromBatchTree(ht1,sub1,path,clauses);
    path.pop_back();
    path.push_back(-split);
    getClausesFromBatchTree(ht2,sub2,path,clauses);
    path.pop_back();
  }
}

std::vector<Clause> HoeffdingDefaultTree::retrieveCompleteRepresentation() const {
  std::vector<Clause> result;
  std::vector<int> path;
  traverseTree(*root_selector_tree, path, result);
  return result;
}

void HoeffdingDefaultTree::traverseTree(const Tree& st, std::vector<int>& path, std::vector<Clause>& clauses) const {
  if (st.isLeaf()) {
    int selector = st.selector_default_assumptions_index;
    Clause c(path);
    if (default_assumptions[selector]>0) {
      c.push_back(variable);
    } else {
      c.push_back(-variable);
    }
    clauses.push_back(c);
  } else {
    int split = st.split_variable;
    path.push_back(split);
    traverseTree(st.subtree(true),path,clauses);
    path.pop_back();
    path.push_back(-split);
    traverseTree(st.subtree(false),path,clauses);
    path.pop_back();
  }
}

}