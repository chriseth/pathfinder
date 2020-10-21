#include <vector>
#include <optional>
#include <tuple>
#include <map>
#include <queue>
#include <random>
#include <iostream>

using namespace std;

using Node = uint32_t;

pair<uint32_t, map<Node, Node>> augmentingPath(
	Node _source,
	Node _sink,
	map<Node, map<Node, uint32_t>> const& _capacity
)
{
	map<Node, Node> parent;
	queue<pair<Node, uint32_t>> q;
	q.emplace(_source, uint32_t(-1));

	while (!q.empty())
	{
		auto [node, flow] = q.front();
		q.pop();
		if (!_capacity.count(node))
			continue;
		for (auto const& [target, capacity]: _capacity.at(node))
			if (!parent.count(target) && capacity > 0)
			{
				parent[target] = node;
				uint32_t newFlow = min(flow, capacity);
				if (target == _sink)
					return make_pair(newFlow, move(parent));
				q.emplace(target, newFlow);
			}
	}
	return {0, {}};
}

//pair<uint32_t, vector<Node>>
// TODO: Actually also return the edges with capacity.
uint32_t maxFlow(Node _source, Node _sink, map<Node, map<Node, uint32_t>> _capacity)
{
	uint32_t flow = 0;
	while (true)
	{
		auto [newFlow, parents] = augmentingPath(_source, _sink, _capacity);
		if (newFlow == 0)
			return flow;
		flow += newFlow;
		for (Node node = _sink; node != _source;)
		{
			Node prev = parents[node];
			_capacity[prev][node] -= newFlow;
			_capacity[node][prev] += newFlow;
			node = prev;
		}
	}
	return flow;
}


int main(void)
{
	uint32_t nodeCount = 40000;
	uint32_t neighborCount = 10;
	default_random_engine generator;
	uniform_int_distribution<uint32_t> randomNode(0, nodeCount);
	uniform_int_distribution<uint32_t> randomCapacity(0, 10);

	cout << "Building network..." << endl;
	size_t edgeCount = 0;
	map<Node, map<Node, uint32_t>> capacities;
	for (Node n = 0; n < nodeCount; n++)
		for (size_t i = 0; i < neighborCount; i++)
		{
			capacities[n][randomNode(generator)] = randomCapacity(generator);
			edgeCount++;
		}
	cout << "Nodes: " << nodeCount << " Edges: " << edgeCount << endl;

	cout << "Computing flow..." << endl;
	uint32_t flow = maxFlow(0, nodeCount - 1, capacities);
	cout << "Max flow: " << flow << endl;
}
