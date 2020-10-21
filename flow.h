#pragma once

#include "types.h"

std::pair<Int, std::vector<Edge>> computeFlow(
	Address const& _source,
	Address const& _sink,
	std::set<Edge> const& _edges,
	Int _requestedFlow = Int::max()
);
