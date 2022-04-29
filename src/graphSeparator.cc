#include <cassert>

#include "graphSeparator.h"


namespace pedant {

std::vector<int> GraphSeparator::getVertexSeparator(const std::vector<int>& vertices, const std::vector<std::pair<int,int>>& edges, 
                                                    const std::vector<int>& vertA, const std::vector<int>& vertB, const std::vector<int>& forbidden,
                                                    const std::vector<int>& to_avoid) {
  GraphSeparator gP(vertices,edges);
  std::vector<int> sep = gP.getVertexSeparator(vertA, vertB, forbidden, to_avoid);
  return sep;
}


GraphSeparator::GraphSeparator(const std::vector<int>& vertices, const std::vector<std::pair<int,int>>& edges) : vertex_names(vertices) {
  auto capacity = get(boost::edge_capacity, g);
  auto rev = get(boost::edge_reverse, g);
  std::vector<Traits::vertex_descriptor> verts;
  verts.reserve(2*vertices.size()+2);
  //we add a source and a sink vertex and each original vertex is represented by an in and an out vertex
  for (long vi = 0; vi < 2*vertices.size()+2; ++vi) {
    verts.push_back(add_vertex(g));
  }

  for (int i=0; i<vertices.size(); i++) {
    int v = vertices[i];
    vertex_indices[v] = 2*i+1;
    //setup "internal" edges
    addEdge(verts[2*i+1], verts[2*i+2], capacity, rev);
  }

  for (auto [source, target] : edges) {
    int idx1 = vertex_indices[source] + 1; //we have to use the out node from the internal edge representation
    int idx2 = vertex_indices[target];
    addEdge(verts[idx1], verts[idx2], capacity, rev);
  } 

  auto source=verts.front(), sink=verts.back();
}


void GraphSeparator::addEdge(Traits::vertex_descriptor source, Traits::vertex_descriptor target, 
    boost::property_map<Graph,boost::edge_capacity_t>::type& capacity, 
    boost::property_map<Graph,boost::edge_reverse_t>::type& reverse, int weight) {
  Traits::edge_descriptor e1, e2;
  bool in1, in2;
  boost::tie(e1, in1) = add_edge(source, target, g);
  boost::tie(e2, in2) = add_edge(target, source, g);
  capacity[e1] = weight;
  capacity[e2] = 0;
  reverse[e1] = e2;
  reverse[e2] = e1;
}

void GraphSeparator::connectToSource(const std::vector<int>& to_connect,
    std::vector<Traits::vertex_descriptor>& verts,
    boost::property_map < Graph, boost::edge_capacity_t >::type& capacity,
    boost::property_map < Graph, boost::edge_reverse_t >::type& rev,
    unsigned int penalty) {
  for (int v : to_connect) {
    int idx = vertex_indices[v];
    addEdge(verts.front(), verts[idx], capacity, rev, penalty);
  }
}


void GraphSeparator::connectToSink(const std::vector<int>& to_connect,
    std::vector<Traits::vertex_descriptor>& verts,
    boost::property_map < Graph, boost::edge_capacity_t >::type& capacity,
    boost::property_map < Graph, boost::edge_reverse_t >::type& rev) {
  for (int v : to_connect) {
    int idx = vertex_indices[v];
    addEdge(verts[idx+1], verts.back(), capacity, rev, external_capacity); //we want to cut internal edges thus we give the actual edges a higher capacity
  }
}

std::vector<int> GraphSeparator::getVertexSeparator(const std::vector<int>& vertA, const std::vector<int>& vertB) {
  return getVertexSeparator(vertA, vertB, {}, {});
}

std::vector<int> GraphSeparator::getVertexSeparator(const std::vector<int>& vertA, const std::vector<int>& vertB, const std::vector<int>& forbidden, const std::vector<int>& unpreferred) {
  auto [vert_beg, vert_end] = vertices(g);
  std::vector<Traits::vertex_descriptor> verts(vert_beg,vert_end);
  auto capacity = get(boost::edge_capacity, g);
  auto rev = get(boost::edge_reverse, g);
  //Assuming unpreferred is empty the max flow is bounded from above by |vertA|*internal_capacity.
  //Every path connecting the source with the sink has to pass through an internal edge of vertA.
  //As these have capacity internal_capacity we get the bound.
  //If we increase this value by one we can thus be sure that no edge with this capacity can be saturated
  //(vertA and forbidden has to be disjoint)
  int penalty = internal_capacity * vertA.size() + 1;
  //If unpreferred is not empty then the value given above is not necessarily a bound.
  //Thus, we have to increase it.
  penalty += unpreferred.size();
  //We never want to have an edge from the original graph (i.e. a non internal edge)
  //in the edge separator. Usually, this is ensured by preceeding internal edges with a lower
  //capacity. But in the case of the outgoing edges of the source there is no such internal edge,
  //thus we have to apply the penalty.
  connectToSource(vertA, verts, capacity, rev, penalty);
  connectToSink(vertB, verts, capacity, rev);
  //We know that every path from the source vertex to the sink vertex has to contain an element of vertB.
  //Morever, we know that in the used representation each vertex is represented by an in and an out vertex.
  //As forbidden and vertB have to be disjoint we also know that the capacities for the edges 
  //connecting the in and out vertices for the elements in forbidden are 1.
  //This means that the maximal flow is at most vertB.size(). 
  //This means by applying a capacity of vertB.size()+1 we ensure that the edge will not be saturated.
  //So this edge will not be part of the edge-separator.
  applyPenalties(forbidden,verts,capacity,penalty);

  //If we can swap an unpreferred vertex in the vertex separator with another
  //vertex we want to do this as long as this does not increase the size of the separator.
  //By using a higher capacity as for the "standard" internal edges the internal
  //edges for the unpreferred are only used if they are really needed
  applyPenalties(unpreferred, verts, capacity, unpreferred_capacity);


  // long flow = push_relabel_max_flow(g, verts.front(), verts.back()); Do not use this algorithm
  long flow = boykov_kolmogorov_max_flow(g, verts.front(), verts.back());
  // long flow = edmonds_karp_max_flow(g, verts.front(), verts.back());

  auto sep_edges = getSeparatingEdges(verts.front());
  // saveGraph();//TEST:
  auto sep_verts = vertexSeparatorFromEdgeSeparator(sep_edges);
  // assert (sep_verts.size() == flow); with unpreferred vertices this does not necessarily hold
  return sep_verts;
}



void GraphSeparator::applyPenalties(const std::vector<int>& forbidden, 
    std::vector<Traits::vertex_descriptor>& verts, 
    boost::property_map < Graph, boost::edge_capacity_t >::type& capacity, int penalty) {
  for (int f : forbidden) {
    applyPenalty(f, verts, capacity, penalty);
  }
}

void GraphSeparator::applyPenalty(int to_apply, std::vector<Traits::vertex_descriptor>& verts,
      boost::property_map < Graph, boost::edge_capacity_t >::type& capacity, int penalty) {
  //We have to apply the penalty both to the internal edges as to all the outgoing edges of the forbidden vertices.
  //If we would not apply it to the outgoing edges we could get a cut in such an edge. 
  int idx=vertex_indices[to_apply];
  auto internal_edge = boost::edge(verts[idx],verts[idx+1],g).first;
  capacity[internal_edge] = penalty;
  boost::graph_traits < Graph >::out_edge_iterator ei, e_end;
  for (boost::tie(ei, e_end) = boost::out_edges(verts[idx+1], g); ei != e_end; ++ei) {
    if (capacity[*ei] > 0) { //it is an outgoing edge
      capacity[*ei] = penalty;
    }
  }
}

void GraphSeparator::removePenalty(int to_remove, std::vector<Traits::vertex_descriptor>& verts,
      boost::property_map < Graph, boost::edge_capacity_t >::type& capacity) {
  int idx=vertex_indices[to_remove];
  auto internal_edge = boost::edge(verts[idx],verts[idx+1],g).first;
  capacity[internal_edge] = 1;
  boost::graph_traits < Graph >::out_edge_iterator ei, e_end;
  for (boost::tie(ei, e_end) = boost::out_edges(verts[idx+1], g); ei != e_end; ++ei) {
    if (capacity[*ei] > 0) { //it is an outgoing edge
      capacity[*ei] = 2;
    }
  }
}

std::vector<int> GraphSeparator::vertexSeparatorFromEdgeSeparator(const std::vector<GraphSeparator::Traits::edge_descriptor>& edge_separator) {
  auto index_map = get(boost::vertex_index, g);
  std::vector<int> vertex_separator;
  vertex_separator.reserve(edge_separator.size());
  for (auto edge : edge_separator) {
    auto s = boost::source(edge,g);
    auto t = boost::target(edge,g);
    auto s_idx = index_map[s];
    auto t_idx = index_map[t];
    assert (t_idx == s_idx+1);
    int vertex_index = t_idx/2;
    //in the internal representation the source has index 0, thus we have to subtract 1
    vertex_separator.push_back(vertex_names[vertex_index-1]);
  }
  return vertex_separator;
}

std::vector<bool> GraphSeparator::reachableInResidualGraph(Traits::vertex_descriptor& source) {
  auto residual_capacity = get(boost::edge_residual_capacity, g);
  auto nof_vertices = num_vertices(g);
  std::vector<bool> vertex_reachable(nof_vertices,false);
  std::queue<Traits::vertex_descriptor> queued_vertices;
  queued_vertices.push(source);
  auto index_map = get(boost::vertex_index, g);
  auto index = index_map[source];
  vertex_reachable[index] = true;
  while(!queued_vertices.empty()) {
    auto vert = queued_vertices.front();
    queued_vertices.pop();
    boost::graph_traits < Graph >::out_edge_iterator ei, e_end;
    for (boost::tie(ei, e_end) = out_edges(vert, g); ei != e_end; ++ei) {
      if (residual_capacity[*ei] > 0) {
        auto v = boost::target(*ei,g);
        auto idx = index_map[v];
        if (!vertex_reachable[idx]) {
          vertex_reachable[idx] = true;
          queued_vertices.push(v);
        }
      }
    }
  }
  return vertex_reachable;
}


/**
 * At first glance the boost::stoer_wagner_min_cut method seems to be capable of getting the required edges.
 * First of all this method does not give the required edges -- the edges have to be computed from a parity map.
 * But more importantly we cannot specify a source and a sink. This means that the method could possibly determine a wrong cut.
 **/
std::vector<GraphSeparator::Traits::edge_descriptor> GraphSeparator::getSeparatingEdges(Traits::vertex_descriptor& source) {
  std::vector<bool> vertex_reachable = reachableInResidualGraph(source);

  auto [vert_beg, vert_end] = vertices(g);
  std::vector<Traits::vertex_descriptor> verts(vert_beg,vert_end);
  auto capacity = get(boost::edge_capacity, g);
  auto rev = get(boost::edge_reverse, g);
  auto index_map = get(boost::vertex_index, g);

  std::vector<Traits::edge_descriptor> result;

  for (int idx = 0; idx<vertex_reachable.size();idx++) {
    if (!vertex_reachable[idx]) {
      for (auto [ei,e_end] = out_edges(verts[idx], g); ei != e_end; ++ei) {
        if (capacity[*ei] == 0) { //a capacity of 0 effectively means that this edge is an input edge
          auto t = boost::target(*ei,g);
          auto idx = index_map[t];
          if (vertex_reachable[idx]) {
            auto rev_edge = rev[*ei];
            result.push_back(rev_edge);
          }
        }
      }
    }
  }
  return result;
}




}