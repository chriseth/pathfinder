#include "flow.h"

#include "adjacencies.h"

#include <queue>
#include <iostream>
#include <variant>
#include <functional>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <optional>

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
	Adjacencies& _adjacencies
)
{
	if (_source == _sink)
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
		for (auto const& [target, capacity]: _adjacencies.outgoingEdgesSortedByCapacity(node))
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
	{
		auto next = extractNextTransfers(_usedEdges, nodeBalances);
		if (next.empty())
			// TODO this should actually not happen.
			break;
		transfers += move(next);
	}

	return transfers;
}

map<Node, int> distanceFromSource(Node const& _source, map<Node, map<Node, Int>> const& _usedEdges)
{
	map<Node, int> distances;
	deque<Node> toProcess;
	distances[_source] = 0;
	toProcess.push_back(_source);
	while (!toProcess.empty())
	{
		Node n = toProcess.front();
		toProcess.pop_front();
		if (_usedEdges.count(n))
			for (auto&& [t, weight]: _usedEdges.at(n))
				if (weight != Int(0) && !distances.count(t))
				{
					distances[t] = distances[n] + 1;
					toProcess.push_back(t);
				}
	}
	return distances;
}

map<Node, map<Node, Int>> reverseEdges(map<Node, map<Node, Int>> const& _usedEdges)
{
	map<Node, map<Node, Int>> reverseEdges;
	for (auto&& [n, edges]: _usedEdges)
		for (auto&& [t, weight]: edges)
			reverseEdges[t][n] = weight;
	return reverseEdges;
}

map<Node, int> distanceToSink(Node const& _sink, map<Node, map<Node, Int>> const& _usedEdges)
{
	return distanceFromSource(_sink, reverseEdges(_usedEdges));
}

/// Returns a map from the negative shortest path length to the edge.
/// The shortest path length is negative so that it is sorted by
/// longest paths first - those are the ones we want to eliminate first.
map<int, set<pair<Node, Node>>> computeEdgesByPathLength(
	Node const& _source,
	Node const& _sink,
	map<Node, map<Node, Int>> const& _usedEdges
)
{
	map<Node, int> fromSource = distanceFromSource(_source, _usedEdges);
	map<Node, int> toSink = distanceToSink(_sink, _usedEdges);
	map<int, set<pair<Node, Node>>> result;
	for (auto&& [s, edges]: _usedEdges)
		for (auto&& [t, weight]: edges)
		{
			int pathLength = fromSource.at(s) + 1 + toSink.at(t);
			result[-pathLength].emplace(s, t);
		}
	return result;
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

optional<pair<Node, Node>> smallestEdgeInSet(map<Node, map<Node, Int>> const& _usedEdges, set<pair<Node, Node>> const& _edges)
{
	assert(!_usedEdges.empty());
	optional<Int> cap;
	optional<pair<Node, Node>> edge;
	for (auto&& [a, b]: _edges)
		if (_usedEdges.count(a) && _usedEdges.at(a).count(b))
		{
			Int edgeCapacity = _usedEdges.at(a).at(b);
			if (edgeCapacity != Int(0))
				if (!cap || edgeCapacity < *cap)
				{
					cap = move(edgeCapacity);
					edge = {a, b};
				}
		}
	return edge;
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

/// Removes the edge (potentially partially), removing a given amount of flow.
/// Returns the remaining flow to prune if the edge was too small.
Int pruneEdge(map<Node, map<Node, Int>>& _usedEdges, pair<Node, Node> const& _edge, Int _flowToPrune)
{
	Node a = _edge.first;
	Node b = _edge.second;
	Int edgeSize = _usedEdges[a][b];
	if (edgeSize > _flowToPrune)
		edgeSize = _flowToPrune;
//	cerr << "Pruning an edge of size " << edgeSize << endl;
//	cerr << "Flow to prune: " << _flowToPrune << endl;
	reduceCapacity(_usedEdges, a, b, edgeSize);
	prune(_usedEdges, b, edgeSize, true);
	prune(_usedEdges, a, edgeSize, false);
	return _flowToPrune - edgeSize;
}

pair<Int, vector<Edge>> computeFlow(
	Address const& _source,
	Address const& _sink,
#if USE_FLOW
	map<Node, map<Node, Int>> const& adjacencies,
#else
	set<Edge> const& _edges,
#endif
	Int _requestedFlow,
	bool _prune
)
{
	cerr << "Got " << _edges.size() << " edges" << endl;
#if USE_FLOW
	map<Node, map<Node, Int>> capacities = adjacencies;
#else

	Adjacencies adjacencies(_edges);
#endif
	//cerr << "Number of nodes (including pseudo-nodes): " << capacities.size() << endl;

	map<Node, map<Node, Int>> usedEdges;

	cerr << "Computing max flow..." << endl;
	auto t1 = chrono::high_resolution_clock::now();
	// First always compute the max flow.
	Int flow{0};
	while (true)
	{
		auto [newFlow, parents] = augmentingPath(_source, _sink, adjacencies);
		//cerr << "Found augmenting path with flow " << newFlow << endl;
		//cerr << "Parents: " << parents.size() << endl;
		if (newFlow == Int(0))
			break;
		flow += newFlow;
		for (Node node = _sink; node != Node{_source}; )
		{
			Node const& prev = parents[node];
			adjacencies.adjustCapacity(prev, node, -newFlow);
			adjacencies.adjustCapacity(node, prev, newFlow);
			// TODO still not sure about this one.
			if (!adjacencies.isAdjacent(node, prev))
				// real edge
				usedEdges[prev][node] += newFlow;
			else
				// (partial) edge removal
				usedEdges[node][prev] -= newFlow;
			node = prev;
		}
	}
	cerr << "Max flow " << flow << " using " << usedEdges.size() << " nodes/edges " << endl;
	auto t2 = chrono::high_resolution_clock::now();
	cerr << "Took " << chrono::duration_cast<chrono::duration<double>>(t2 - t1).count() << endl;
	cerr << "Pruning..." << endl;
	auto t3 = chrono::high_resolution_clock::now();
	if (_prune && flow > _requestedFlow)
	{
		cerr << "Pruning according to new algorithm..." << endl;
		// Note the path length is negative to sort by longest shortest path first.
		map<int, set<pair<Node, Node>>> edgesByPathLength = computeEdgesByPathLength(_source, _sink, usedEdges);
		Int flowToPrune = flow - _requestedFlow;
		for (auto&& [pathLength, edgesHere]: edgesByPathLength)
		{
			// As long as `edges` contain an edge with smaller weight than the weight still to prune:
			//   take the smallest such edge and prune it.
			while (flowToPrune > Int{0})
			{
				auto smallestEdge = smallestEdgeInSet(usedEdges, edgesHere);
				if (!smallestEdge || usedEdges[smallestEdge->first][smallestEdge->second] > flowToPrune)
					break;
				flowToPrune = pruneEdge(usedEdges, *smallestEdge, flowToPrune);
			}
		}
		// If there is still flow to prune, take the first element in edgesByPathLength
		// and partially prune its path.
		if (flowToPrune > Int{0})
			for (auto&& [pathLength, edges]: edgesByPathLength)
			{
				for (auto&& edge: edges)
				{
					flowToPrune = pruneEdge(usedEdges, edge, flowToPrune);
					if (flowToPrune == Int{0})
						break;
				}
				if (flowToPrune == Int{0})
					break;
			}
		flow = _requestedFlow + flowToPrune;
	}
	else if (flow > _requestedFlow)
	{
		// Now prune edges until the flow is as requested.
		// (old algorithm)
		Int flowToPrune = flow - _requestedFlow;
		while (flowToPrune > Int(0))
			flowToPrune = pruneEdge(usedEdges, smallestEdge(usedEdges), flowToPrune);
		flow = _requestedFlow;
	}

	auto t4 = chrono::high_resolution_clock::now();
	cerr << "Took " << chrono::duration_cast<chrono::duration<double>>(t4 - t3).count() << endl;

	assert(flowToPrune == Int{0});
	cerr << "Computing transfers..." << endl;
	auto t5 = chrono::high_resolution_clock::now();
	auto transfers = extractTransfers(_source, _sink, flow, usedEdges);
	auto t6 = chrono::high_resolution_clock::now();
	cerr << "Took " << chrono::duration_cast<chrono::duration<double>>(t6 - t5).count() << endl;
	return {flow, move(transfers)};
}
