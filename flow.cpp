#include "flow.h"

#include <queue>
#include <iostream>
#include <variant>

using namespace std;

/// Either an actual node, or a newly introduced node on a token edge.
using Node = variant<Address, tuple<Address, Address, Address>>;

Node pseudoNode(Edge const& _edge)
{
	return make_tuple(_edge.from, _edge.to, _edge.token);
}

/// Turns the edge set into an adjacency list.
/// At the same time, it generates new pseudo-nodes to cope with the multi-edges.
map<Node, map<Node, Int>> computeAdjacencies(set<Edge> const& _edges)
{
	map<Node, map<Node, Int>> adjacencies;
	for (Edge const& edge: _edges)
	{
		adjacencies[edge.from][pseudoNode(edge)] = edge.capacity;
		adjacencies[pseudoNode(edge)][edge.to] = edge.capacity;
	}
	return adjacencies;
}

pair<Int, map<Node, Node>> augmentingPath(
	Address const& _source,
	Address const& _sink,
	map<Node, map<Node, Int>> const& _capacity
)
{
	map<Node, Node> parent;
	queue<pair<Node, Int>> q;
	q.emplace(_source, Int::max());

	while (!q.empty())
	{
		//cout << "Queue size: " << q.size() << endl;
		//cout << "Parent relation size: " << parent.size() << endl;
		auto [node, flow] = q.front();
		q.pop();
		if (!_capacity.count(node))
			continue;
		for (auto const& [target, capacity]: _capacity.at(node))
			if (!parent.count(target) && Int(0) < capacity)
			{
				parent[target] = node;
				Int newFlow = min(flow, capacity);
				if (target == Node{_sink})
					return make_pair(move(newFlow), move(parent));
				q.emplace(target, move(newFlow));
			}
	}
	return {Int(0), {}};
}

vector<Edge> extractTransfers(Address const& _source, Address const& _sink, Int _amount, map<Node, map<Node, Int>> _usedEdges)
{
	vector<Edge> transfers;

	map<Address, Int> nodeBalances;
	nodeBalances[_source] = _amount;
	while (true)
	{
		if (
			nodeBalances.size() == 0 ||
			(nodeBalances.size() == 1 && nodeBalances.begin()->first == _sink)
		)
			break;

		auto [node, amount] = *nodeBalances.begin();
		nodeBalances.erase(node);

		for (auto& [toNode, capacity]: _usedEdges[node])
			if (capacity > Int(0))
			{
				auto const& [from, to, token] = std::get<tuple<Address, Address, Address>>(toNode);
				// TOOD the JS code mentions that the same token can be transacted
				// between the same addresses multiple times. This would be a problem here.
				Int transferAmount = min(amount, capacity);
				if (transferAmount == Int(0))
					continue;

				transfers.push_back(Edge{from, to, token, transferAmount});
				amount -= transferAmount;
				capacity -= transferAmount;
				nodeBalances[to] += transferAmount;
			}
		for (auto it = _usedEdges[node].begin(); it != _usedEdges[node].end();)
			if (it->second == Int(0))
				it = _usedEdges[node].erase(it);
			else
				++it;
	}
	return transfers;
}

pair<Int, vector<Edge>> computeFlow(Address const& _source, Address const& _sink, set<Edge> const& _edges, Int _requestedFlow)
{
	//cout << "Computing adjacencies..." << endl;
	map<Node, map<Node, Int>> adjacencies = computeAdjacencies(_edges);
	map<Node, map<Node, Int>> capacities = adjacencies;
	//cout << "Number of nodes (including pseudo-nodes): " << adjacencies.size() << endl;

	map<Node, map<Node, Int>> usedEdges;

	Int flow{0};
	while (flow < _requestedFlow)
	{
		auto [newFlow, parents] = augmentingPath(_source, _sink, capacities);
		//cout << "Found augmenting path with flow " << newFlow << endl;
		if (newFlow == Int(0))
			break;
		if (flow + newFlow > _requestedFlow)
			newFlow = _requestedFlow - flow;
		flow += newFlow;
		for (Node node = _sink; node != Node{_source}; )
		{
			Node const& prev = parents[node];
			capacities[prev][node] -= newFlow;
			capacities[node][prev] += newFlow;
			// TODO still not sure about this one.
			if (adjacencies[node][prev] == Int(0))
				// real edge
				usedEdges[prev][node] += newFlow;
			else
				// (partial) edge removal
				usedEdges[node][prev] -= newFlow;
			node = prev;
		}
	}

	return {flow, extractTransfers(_source, _sink, flow, usedEdges)};
}
