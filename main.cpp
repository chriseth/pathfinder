#include "importGraph.h"
#include "flow.h"

#include "json.hpp"

#include <iostream>
#include <sstream>

using namespace std;
using json = nlohmann::json;

set<Edge> edges;

extern "C" {
size_t loadEdges(char const* _data, size_t _length);
}

size_t loadEdges(char const* _data, size_t _length)
{
	string data(_data, _length);
	istringstream stream(data);
	edges = importEdgesBinary(stream);
	return edges.size();
}

extern "C" {
char const* flow(char const* _input);
}

char const* flow(char const* _input)
{
	static string retVal;

	json parameters = json::parse(string(_input));
	Address from{string(parameters["from"])};
	Address to{string(parameters["to"])};
	Int value{string(parameters["value"])};
	auto [flow, transfers] = computeFlow(from, to, edges, value);

	json output;
	output["flow"] = to_string(flow);
	for (Edge const& t: transfers)
		output["transfers"].push_back(json{
			{"from", to_string(t.from)},
			{"to", to_string(t.to)},
			{"token", to_string(t.token)},
			{"value", to_string(t.capacity)}
		});

	retVal = output.dump();

	return retVal.c_str();
}

void computeFlow(
	Address const& _source,
	Address const& _sink,
	Int const& _value,
	string const& _edgesDat
)
{
	set<Edge> edges;
	if (_edgesDat.size() >= 4 && _edgesDat.substr(_edgesDat.size() - 4) == ".dat")
		edges = importEdgesBinary(_edgesDat);
	else
		edges = importEdgesJson(_edgesDat);
	//cout << "Edges: " << edges.size() << endl;

	auto [flow, transfers] = computeFlow(_source, _sink, edges, _value);
//	cout << "Flow: " << flow << endl;
//	cout << "Transfers: " << endl;
//	for (Edge const& edge: transfers)
//		cout << edge.from << " (" << edge.token << ") -> " << edge.to << " - " << edge.capacity << endl;

	size_t stepNr = 0;
	json transfersJson = json::array();
	for (Edge const& transfer: transfers)
		transfersJson.push_back(nlohmann::json{
			{"step", stepNr++},
			{"from", to_string(transfer.from)},
			{"to", to_string(transfer.to)},
			{"token", to_string(transfer.token)},
			{"value", to_string(transfer.capacity)}
		});
	cout << json{
		{"maxFlowValue", to_string(flow)},
		{"transferSteps", move(transfersJson)}
	} << endl;
}


void importDB(string const& _safesJson, string const& _edgesDat)
{
	edgeSetToBinary(
		findEdgesInGraphData(importGraph(_safesJson)),
		_edgesDat
	);
}

int main(int argc, char const** argv)
{
	if (argc == 4 && argv[1] == string{"--importDB"})
		importDB(argv[2], argv[3]);
	else if (
		(argc == 6 && argv[1] == string{"--flow"}) ||
		(argc == 5 && string(argv[1]).substr(0, 2) != "--")
	)
		computeFlow(Address(string(argv[1])), Address(string(argv[2])), Int(string(argv[3])), argv[4]);
	else
	{
		cerr << "Usage: " << argv[0] << " <from> <to> <value> <edges.dat>" << endl;
		cerr << "Options: " << endl;
		cerr << "  [--flow] <from> <to> <value> <edges.dat>    Compute max flow up to <value> and output transfer steps in json." << endl;
		cerr << "  --importDB <safes.json> <edges.dat>         Import safes with trust edges and generate transfer limit graph." << endl;
		cerr << "  [--help]                                    This help screen." << endl;
		return 1;
	}
	return 0;
}
