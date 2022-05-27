#include "adjacencies.h"

#include <algorithm>

using namespace std;

using Node = FlowGraphNode;

namespace
{

Node pseudoNode(Edge const& _edge)
{
	return make_tuple(_edge.from, _edge.token);
}

bool isRealNode(Node const& _node)
{
	return holds_alternative<Address>(_node);
}

Address sourceAddressOf(Node const& _node)
{
	if (holds_alternative<Address>(_node))
		return std::get<Address>(_node);
	else
		return std::get<0>(std::get<tuple<Address, Address>>(_node));
}

}

Adjacencies::Adjacencies(set<Edge> const& _edges)
{
	for (Edge const& edge: _edges)
		m_edgesFrom[edge.from].insert(edge);
//		//if (edge.capacity < minCap)
//		//	continue;
//		// One edge from "from" to "from x token" with a capacity as the max over
//		// all contributing edges (the balance of the sender)
//		m_adjacencies[edge.from][pseudoNode(edge)] = max(edge.capacity, m_adjacencies[edge.from][pseudoNode(edge)]);
//		// Another edge from "from x token" to "to" with its own capacity (based on the trust)
//		m_adjacencies[pseudoNode(edge)][edge.to] = edge.capacity;
//	}
//	cout << m_adjacencies.size() << endl;
}

vector<pair<Node, Int>> Adjacencies::outgoingEdgesSortedByCapacity(Node const& _from)
{
	vector<pair<Node, Int>> r;
	map<Node, Int> adjacencies = adjacenciesFrom(_from);
	if (m_capacityAdjustments.count(_from))
		for (auto const& [node, adj]: m_capacityAdjustments.at(_from))
			adjacencies[node] += adj;
	r = {adjacencies.begin(), adjacencies.end()};
	sort(r.begin(), r.end(), [](pair<Node, Int> const& _a, pair<Node, Int> const& _b) {
		return make_pair(get<1>(_a), get<0>(_a)) > make_pair(get<1>(_b), get<0>(_b));
	});
	return r;
}

bool Adjacencies::isAdjacent(Node const& _from, Node const& _to)
{
//	return m_adjacencies.count(_from) && m_adjacencies.at(_from).count(_to) && m_adjacencies.at(_from).at(_to) != Int(0);
	auto adj = adjacenciesFrom(_from);
	return adj.count(_to) && adj.at(_to) != Int(0);
}

void Adjacencies::adjustCapacity(Node const& _from, Node const& _to, Int const& _adjustment)
{
	m_capacityAdjustments[_from][_to] += _adjustment;
}

map<Adjacencies::Node, Int> Adjacencies::adjacenciesFrom(Node const& _from)
{
	if (m_lazyAdjacencies.count(_from))
		return m_lazyAdjacencies[_from];

	map<Adjacencies::Node, Int> adj;
	Address addrFrom = sourceAddressOf(_from);
	if (isRealNode(_from))
	{
		for (Edge const& edge: m_edgesFrom[addrFrom])
		{
			// One edge from "from" to "from x token" with a capacity as the max over
			// all contributing edges (the balance of the sender)
			adj[pseudoNode(edge)] = max(edge.capacity, adj[pseudoNode(edge)]);
		}
	}
	else
	{
		for (Edge const& edge: m_edgesFrom[addrFrom])
		{
			// Another edge from "from x token" to "to" with its own capacity (based on the trust)
			if (pseudoNode(edge) == _from)
				adj[edge.to] = edge.capacity;
		}
	}
	m_lazyAdjacencies[_from] = adj;
	return adj;
}
