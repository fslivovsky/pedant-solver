#include <string>
#include <vector>
// #include <set>
#include <unordered_map>
#include <queue>

#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_utility.hpp>

#include <boost/graph/boykov_kolmogorov_max_flow.hpp>
// #include <boost/graph/push_relabel_max_flow.hpp>
#include <boost/graph/edmonds_karp_max_flow.hpp>

//Needed for the visualisation
#include <boost/graph/graphviz.hpp>
#include <fstream>
#include <string>

namespace pedant {


class GraphSeparator {

  typedef boost::adjacency_list_traits < boost::vecS, boost::vecS, boost::directedS > Traits;
  typedef boost::adjacency_list < boost::vecS, boost::vecS, boost::directedS,
    boost::property < boost::vertex_name_t, std::string,
    boost::property < boost::vertex_index_t, long,
    boost::property < boost::vertex_color_t, boost::default_color_type,
    boost::property < boost::vertex_distance_t, long,
    boost::property < boost::vertex_predecessor_t, Traits::edge_descriptor > > > > >,
    boost::property < boost::edge_capacity_t, long,
    boost::property < boost::edge_residual_capacity_t, long,
    boost::property < boost::edge_reverse_t, Traits::edge_descriptor > > > > Graph;


 public:


  static std::vector<int> getVertexSeparator( const std::vector<int>& vertices, const std::vector<std::pair<int,int>>& edges, 
                                              const std::vector<int>& vertA, const std::vector<int>& vertB, const std::vector<int>& forbidden);

  GraphSeparator() = delete;
  
  private:
  
  /**
   * @param vertices The vertices of the graph
   * @param edges The edges of the graph
   **/
  GraphSeparator(const std::vector<int>& vertices, const std::vector<std::pair<int,int>>& edges);


  /**
   * Computes a vertex separator for vertA and vertB
   * 
   * @param vertA A set of vertices. Must be vertices in the represented graph.
   * @param vertB A set of vertices. Must be vertices in the represented graph and must be disjoint to vertA. 
   * 
   * This method alters the internal representation of the graph, 
   * so be aware when you reuse this class after the application of this method.
   **/
  std::vector<int> getVertexSeparator(const std::vector<int>& vertA, const std::vector<int>& vertB);

  /**
   * Computes a vertex separator for vertA and vertB
   * 
   * @param vertA A set of vertices. Must be vertices in the represented graph.
   * @param vertB A set of vertices. Must be vertices in the represented graph and must be disjoint to vertA. 
   * @param forbidden A set of vertices. The elements of forbidden shall not be contained in the computed vertex separator.
   *      forbidden and vertB shall be disjoint -- without applying any restriction forbiding vertices could imply that there is no separator.
   *      Must be contained in the set of vertices of the current graph.
   * 
   * This method alters the internal representation of the graph, 
   * so be aware when you reuse this class after the application of this method.
   **/
  std::vector<int> getVertexSeparator(const std::vector<int>& vertA, const std::vector<int>& vertB, const std::vector<int>& forbidden);

  /**
   * Computes a vertex separator for vertA and vertB
   * 
   * @param vertA A set of vertices. Must be vertices in the represented graph.
   * @param vertB A set of vertices. Must be vertices in the represented graph and must be disjoint to vertA. 
   * @param forbidden A set of vertices. The elements of forbidden shall not be contained in the computed vertex separator.
   *      forbidden and vertB shall be disjoint -- without applying any restriction forbiding vertices could imply that there is no separator.
   *      Must be contained in the set of vertices of the current graph.
   * @param mutually_exclusive_vertices The computed separator shall contain maximally one element of this vector. 
   *      It is the responsibility of the caller to ensure that such a separator exists.
   *      forbidden and mutually_exclusive_vertices shall be disjoint
   * 
   * This method alters the internal representation of the graph, 
   * so be aware when you reuse this class after the application of this method.
   **/
  std::vector<int> getVertexSeparator(const std::vector<int>& vertA, const std::vector<int>& vertB, const std::vector<int>& forbidden, const std::vector<int>& mutually_exclusive_vertices);

  Graph g;
  std::unordered_map<int,int> vertex_indices;
  const std::vector<int> vertex_names;
  
  /**
   * @param source The source node.
   * This method requires that on of the subsequent methods was applied to g (boykov_kolmogorov_max_flow, edmonds_karp_max_flow, push_relabel_max_flow).
   * The method finds all vertices which are reachable from source in the residual graph (given by the flow algorithm). 
   * Two vertices are conntected in the residual graph iff there is a path between those vertices in the original graph such that no edge in this path
   * has a residual capacity of 0.
   **/
  std::vector<bool> reachableInResidualGraph(Traits::vertex_descriptor& source);

  /**
   * This method requires that on of the subsequent methods was applied to g (boykov_kolmogorov_max_flow, edmonds_karp_max_flow, push_relabel_max_flow).
   * The computed separator is given by the edges that connect vertices that are reachable in the residualgraph with edges that are not reachable.
   **/
  std::vector<Traits::edge_descriptor> getSeparatingEdges(Traits::vertex_descriptor& source);

  /**
   * It has to be ensured that the  given edge separator only contains internal edges.
   * For each edge (v_in, v_out) the computed separator contains the vertex v.
   **/
  std::vector<int> vertexSeparatorFromEdgeSeparator(const std::vector<GraphSeparator::Traits::edge_descriptor>& edge_separator);

  void applyPenalties(const std::vector<int>& forbidden, 
      std::vector<Traits::vertex_descriptor>& verts,
      boost::property_map < Graph, boost::edge_capacity_t >::type& capacity, int penalty);

  void applyPenalty(int to_remove, std::vector<Traits::vertex_descriptor>& verts,
      boost::property_map < Graph, boost::edge_capacity_t >::type& capacity, int penalty);

  void removePenalty(int to_remove, std::vector<Traits::vertex_descriptor>& verts,
      boost::property_map < Graph, boost::edge_capacity_t >::type& capacity);

  void addEdge(Traits::vertex_descriptor source, Traits::vertex_descriptor target, 
      boost::property_map<Graph,boost::edge_capacity_t>::type& capacity, 
      boost::property_map<Graph,boost::edge_reverse_t>::type& reverse, int weight=1);

  void connectToSource(const std::vector<int>& to_connect, 
      std::vector<Traits::vertex_descriptor>& verts,
      boost::property_map < Graph, boost::edge_capacity_t >::type& capacity,
      boost::property_map < Graph, boost::edge_reverse_t >::type& rev);
  void connectToSink(const std::vector<int>& to_connect, 
      std::vector<Traits::vertex_descriptor>& verts,
      boost::property_map < Graph, boost::edge_capacity_t >::type& capacity,
      boost::property_map < Graph, boost::edge_reverse_t >::type& rev);



  void saveGraph();
  Graph trimGraph() const;

};

inline void GraphSeparator::saveGraph() {
  std::ofstream file("graph.dot");
  std::vector<std::string> name_strs;
  name_strs.push_back("source");
  for (auto x : vertex_names) {
    std::string base_name = std::to_string(x);
    std::string in_vertex = base_name + " in";
    std::string out_vertex = base_name + " out";
    name_strs.push_back(in_vertex);
    name_strs.push_back(out_vertex);
  }
  name_strs.push_back("sink");
  auto sg = trimGraph();
  boost::write_graphviz(file, sg, boost::make_label_writer(&name_strs[0]));
  file.close();
}

inline GraphSeparator::Graph GraphSeparator::trimGraph() const {
  GraphSeparator::Graph simplified_graph;
  std::vector<Traits::vertex_descriptor> verts;
  auto [vertex_begin, vertex_end] = vertices(g);
  for (auto it = vertex_begin; it != vertex_end; it++) {
    verts.push_back(add_vertex(simplified_graph));
  }
  auto [edge_beg, edge_end] = edges(g);
  auto index_map = get(boost::vertex_index, g);
  auto capacity = get(boost::edge_capacity, g);
  for (auto it = edge_beg; it != edge_end; it++) {
    auto edge = *it;
    auto s = boost::source(edge,g);
    auto t = boost::target(edge,g);
    auto s_idx = index_map[s];
    auto t_idx = index_map[t];
    if (capacity[edge] != 0) { //original edge
      add_edge(verts[s_idx], verts[t_idx], simplified_graph);
    }
  }
  return simplified_graph;
}




}