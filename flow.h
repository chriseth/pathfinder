#pragma once

#include "types.h"

#define USE_FLOW 0

std::pair<Int, std::vector<Edge>> computeFlow(
	Address const& _source,
	Address const& _sink,
#if USE_FLOW
	std::map<FlowGraphNode, std::map<FlowGraphNode, Int>> const& _adjacencies,
#else
	std::set<Edge> const& _edges,
#endif
	Int _requestedFlow = Int::max(),
	bool _prune = false
);
