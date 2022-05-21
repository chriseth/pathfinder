#include "flow.h"

#include <queue>
#include <iostream>
#include <variant>
#include <functional>
#include <algorithm>
#include <cassert>

using namespace std;

using Node = FlowGraphNode;

Node pseudoNode(Edge const& _edge)
{
	return make_tuple(_edge.from, _edge.token);
}

/// Concatenate the contents of a container onto a vector, move variant.
template <class T, class U> vector<T>& operator+=(vector<T>& _a, U&& _b)
{
	std::move(_b.begin(), _b.end(), std::back_inserter(_a));
	return _a;
}

template <class K, class V, class F>
void erase_if(map<K, V>& _container, F const& _fun)
{
	for (auto it = _container.begin(); it != _container.end();)
		 if (_fun(*it))
			it = _container.erase(it);
		 else
			++it;
}

/// Turns the edge set into an adjacency list.
/// At the same time, it generates new pseudo-nodes to cope with the multi-edges.
map<Node, map<Node, Int>> computeAdjacencies(set<Edge> const& _edges)
{
	//Int const minCap("1000000000000000");
	map<Node, map<Node, Int>> adjacencies;
	for (Edge const& edge: _edges)
	{
		//if (edge.capacity < minCap)
		//	continue;
		// One edge from "from" to "from x token" with a capacity as the max over
		// all contributing edges (the balance of the sender)
		adjacencies[edge.from][pseudoNode(edge)] = max(edge.capacity, adjacencies[edge.from][pseudoNode(edge)]);
		// Another edge from "from x token" to "to" with its own capacity (based on the trust)
		adjacencies[pseudoNode(edge)][edge.to] = edge.capacity;
	}
	return adjacencies;
}

vector<pair<Node, Int>> sortedByCapacity(map<Node, Int> const& _capacities)
{
	vector<pair<Node, Int>> r(_capacities.begin(), _capacities.end());
	sort(r.begin(), r.end(), [](pair<Node, Int> const& _a, pair<Node, Int> const& _b) {
		return make_pair(get<1>(_a), get<0>(_a)) > make_pair(get<1>(_b), get<0>(_b));
	});
	return r;
}

pair<Int, map<Node, Node>> augmentingPath(
	Address const& _source,
	Address const& _sink,
	map<Node, map<Node, Int>> const& _capacity
)
{
	if (_source == _sink || !_capacity.count(_source))
		return {Int(0), {}};

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
		for (auto const& [target, capacity]: sortedByCapacity(_capacity.at(node)))
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

/// Extract the next list of transfers until we get to a situation where
/// we cannot transfer the full balance and start over.
vector<Edge> extractNextTransfers(map<Node, map<Node, Int>>& _usedEdges, map<Address, Int>& _nodeBalances)
{
	vector<Edge> transfers;

	for (auto& [node, balance]: _nodeBalances)
		for (auto& edge: _usedEdges[node])
		{
			Node const& intermediate = edge.first;
			for (auto& [toNode, capacity]: _usedEdges[intermediate])
			{
				auto const& [from, token] = std::get<tuple<Address, Address>>(intermediate);
				Address to = std::get<Address>(toNode);
				if (capacity == Int(0))
					continue;
				if (balance < capacity)
				{
					// We do not have enough balance yet, there will be another transfer along this edge.
					if (!transfers.empty())
						return transfers;
					else
						continue;
				}
				transfers.push_back(Edge{from, to, token, capacity});
				balance -= capacity;
				_nodeBalances[to] += capacity;
				capacity = Int(0);
			}
		}

	erase_if(_nodeBalances, [](auto& _a) { return _a.second == Int(0); });

	return transfers;
}


vector<Edge> extractTransfers(Address const& _source, Address const& _sink, Int _amount, map<Node, map<Node, Int>> _usedEdges)
{
	vector<Edge> transfers;

	map<Address, Int> nodeBalances;
	nodeBalances[_source] = _amount;
	while (
		!nodeBalances.empty() &&
		(nodeBalances.size() > 1 || nodeBalances.begin()->first != _sink)
	)
		transfers += extractNextTransfers(_usedEdges, nodeBalances);

	return transfers;
}

pair<Node, Node> smallestEdge(map<Node, map<Node, Int>> const& _usedEdges)
{
	assert(!_usedEdges.empty());
	optional<Int> cap;
	optional<pair<Node, Node>> edge;
	for (auto&& [a, out]: _usedEdges)
		for (auto&& [b, c]: out)
			if (c != Int(0) && (!cap || c < *cap))
			{
				cap = c;
				edge = {a, b};
			}
	assert(edge);
	return *edge;
}

optional<Node> smallestEdgeTo(map<Node, map<Node, Int>> const& _usedEdges, Node const& _dest)
{
	optional<Int> cap;
	optional<Node> src;
	for (auto&& [a, out]: _usedEdges)
		if (out.count(_dest))
		{
			Int c = out.at(_dest);
			if (c != Int(0) && (!cap || c < *cap))
			{
				cap = c;
				src = a;
			}
		}
	return src;
}

optional<Node> smallestEdgeFrom(map<Node, map<Node, Int>> const& _usedEdges, Node const& _src)
{
	if (!_usedEdges.count(_src))
		return nullopt;
	optional<Int> cap;
	optional<Node> dest;
	for (auto&& [b, c]: _usedEdges.at(_src))
		if (c != Int(0) && (!cap || c < *cap))
		{
			cap = c;
			dest = b;
		}
	return dest;
}

void reduceCapacity(map<Node, map<Node, Int>>& _usedEdges, Node _a, Node _b, Int const& _capacity)
{
	_usedEdges[_a][_b] -= _capacity;
	if (_usedEdges[_a][_b] == Int(0))
		_usedEdges[_a].erase(_b);
}

void prune(map<Node, map<Node, Int>>& _usedEdges, Node _n, Int _flowToPrune, bool _forward)
{
	while (true)
	{
		optional<Node> a = _forward ? smallestEdgeFrom(_usedEdges, _n) : smallestEdgeTo(_usedEdges, _n);
		if (!a)
			return; // we reached the source / sink

		Int cap = _forward ? _usedEdges[_n][*a] : _usedEdges[*a][_n];
		if (cap >= _flowToPrune)
			cap = _flowToPrune;
		if (_forward)
			reduceCapacity(_usedEdges, _n, *a, cap);
		else
			reduceCapacity(_usedEdges, *a, _n, cap);
		prune(_usedEdges, *a, cap, _forward);
		_flowToPrune -= cap;
		if (_flowToPrune == Int(0))
			return;
	}
}

pair<Int, vector<Edge>> computeFlow(
	Address const& _source,
	Address const& _sink,
#if USE_FLOW
	map<Node, map<Node, Int>> const& adjacencies,
#else
	set<Edge> const& _edges,
#endif
	Int _requestedFlow
)
{
	cerr << "Computing adjacencies..." << endl;
#if USE_FLOW
	map<Node, map<Node, Int>> capacities = adjacencies;
#else
	map<Node, map<Node, Int>> adjacencies = computeAdjacencies(_edges);
	map<Node, map<Node, Int>> capacities = adjacencies;
#endif
	cerr << "Number of nodes (including pseudo-nodes): " << capacities.size() << endl;

	map<Node, map<Node, Int>> usedEdges;

	// First always compute the max flow.
	Int flow{0};
	while (true)
	{
		auto [newFlow, parents] = augmentingPath(_source, _sink, capacities);
		//cout << "Found augmenting path with flow " << newFlow << endl;
		if (newFlow == Int(0))
			break;
		flow += newFlow;
		for (Node node = _sink; node != Node{_source}; )
		{
			Node const& prev = parents[node];
			capacities[prev][node] -= newFlow;
			capacities[node][prev] += newFlow;
			// TODO still not sure about this one.
			if (!adjacencies.count(node) || !adjacencies.at(node).count(prev) || adjacencies.at(node).at(prev) == Int(0))
				// real edge
				usedEdges[prev][node] += newFlow;
			else
				// (partial) edge removal
				usedEdges[node][prev] -= newFlow;
			node = prev;
		}
	}
	cerr << "Max flow " << flow << " using " << usedEdges.size() << " nodes/edges " << endl;

	// Now prune edges until the flow is as requested.
	while (flow > _requestedFlow)
	{
		auto&& [a, b] = smallestEdge(usedEdges);
		Int edgeSize = usedEdges[a][b];
		if (flow - edgeSize < _requestedFlow)
			edgeSize = flow - _requestedFlow;
		reduceCapacity(usedEdges, a, b, edgeSize);
		prune(usedEdges, b, edgeSize, true);
		prune(usedEdges, a, edgeSize, false);
		flow -= edgeSize;
	}

	return {flow, extractTransfers(_source, _sink, flow, usedEdges)};
}
