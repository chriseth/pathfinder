#pragma once

#include "types.h"

#include <set>

class Adjacencies
{
	using Node = FlowGraphNode;
public:
	Adjacencies(std::set<Edge> const& _edges);

	/// @returns the list of edges from the given node, sorted by capacities.
	std::vector<std::pair<Node, Int>> outgoingEdgesSortedByCapacity(Node const& _from);
	/// @returns true if there is an edge with nonzero capacity from @a _from to @a _to in the
	/// unmodified graph.
	bool isAdjacent(Node const& _from, Node const& _to) const;
	/// Adjusts the capacity of the given edge.
	void adjustCapacity(Node const& _from, Node const& _to, Int const& _adjustment);
private:
	std::set<Edge> const& m_edges;
	std::map<Node, std::map<Node, Int>> m_adjacencies;
	std::map<Node, std::map<Node, Int>> m_capacityAdjustments;
};
