#include "importGraph.h"
#include "flow.h"

#include "json.hpp"

#include <iostream>

using namespace std;
using json = nlohmann::json;

//	DB db = importGraph(argv[1]);
//	auto edges = findEdgesInGraphData(db);
//	edgeSetToJson(edges, "edges.json");

int main(int argc, char const** argv)
{
	if (argc != 5)
	{
		cerr << "Usage: " << argv[0] << " <from> <to> <value> <edges.json>\n";
		exit(1);
	}

	Address source = Address(string(argv[1]));
	Address sink = Address(string(argv[2]));
	Int value = Int(string(argv[3]));

	set<Edge> edges = importEdges(argv[4]);
	//cout << "Edges: " << edges.size() << endl;

	auto [flow, transfers] = computeFlow(source, sink, edges, value);
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
