#pragma once

#include "types.h"

#include <istream>


DB importGraph(char const* _file);
std::set<Edge> findEdgesInGraphData(DB const& _db);

void edgeSetToJson(std::set<Edge> const& _edges, char const* _file);
std::set<Edge> importEdges(char const* _file);

void edgeSetToBinary(std::set<Edge> const& _edges, char const* _file);
std::set<Edge> importEdgesBinary(char const* _file);
std::set<Edge> importEdgesBinary(std::istream& _stream);
