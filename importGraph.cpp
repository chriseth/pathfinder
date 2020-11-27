#include "importGraph.h"

#include "exceptions.h"
#include "encoding.h"
#include "binaryExporter.h"
#include "binaryImporter.h"

#include "json.hpp"

#include <fstream>
#include <iostream>
#include <set>
#include <variant>

using namespace std;
using json = nlohmann::json;

void edgeSetToJson(set<Edge> const& _edges, char const* _file)
{
	json edges = json::array();
	for (Edge const& edge: _edges)
		edges.push_back(json{
			{"from", to_string(edge.from)},
			{"to", to_string(edge.to)},
			{"token", to_string(edge.token)},
			{"capacity", to_string(edge.capacity)}
		});

	ofstream f(_file);
	f << edges;
	f.close();
}



set<Edge> importEdgesJson(string const& _file)
{
	ifstream f(_file);
	json edgesJson;
	f >> edgesJson;
	f.close();

	if (edgesJson.is_object() && edgesJson["edges"].is_array())
		edgesJson = move(edgesJson["edges"]);

	set<Edge> edges;
	for (auto const& edge: edgesJson)
	{
		Int capacity =
			edge["capacity"].is_number() ?
			Int(uint64_t(edge["capacity"])) :
			Int(string(edge["capacity"]));
		edges.insert(Edge{
			Address(string(edge["from"])),
			Address(string(edge["to"])),
			Address(string(edge["token"])),
			move(capacity)
		});
	}

	return edges;
}


void edgeSetToBinary(set<Edge> const& _edges, string const& _file)
{
	BinaryExporter(_file).write(_edges);
}

set<Edge> importEdgesBinary(istream& _file)
{
	return BinaryImporter(_file).readEdgeSet();
}

set<Edge> importEdgesBinary(string const& _file)
{
	ifstream f(_file);
	return importEdgesBinary(f);
}

