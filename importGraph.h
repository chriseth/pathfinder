#pragma once

#include "types.h"


DB importGraph(char const* _file);
std::set<Edge> findEdgesInGraphData(DB const& _db);
void edgeSetToJson(std::set<Edge> const& _edges, char const* _file);
std::set<Edge> importEdges(char const* _file);
