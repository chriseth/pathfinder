#include "flow.h"
#include "exceptions.h"
#include "binaryImporter.h"
#include "encoding.h"
#include "json.hpp"
#include <iostream>
#include <sstream>
#include <chrono>

using namespace std;
using json = nlohmann::json;

DB db;

namespace
{
	string debugData(vector<Edge> const& _transfers)
	{
		string out;
		for (Edge const& t: _transfers)
		{
			out +=
					"Transfer " + to_string(t.from) + " -> " + to_string(t.to) +
					" of " + to_string(t.capacity) + " tokens of " +
					to_string(db.token(t.token).safeAddress) + "\n";
			out +=
					"to is org: " + (db.safe(t.to).organization ? "- true"s : "- false"s) +
					" trust perc: " + to_string(db.safe(t.from).sendToPercentage(t.to)) +
					" sender token balance from " + to_string(db.safe(t.from).balance(db.safe(t.from).tokenAddress)) +
					" to " + to_string(db.safe(t.to).balance(db.safe(t.from).tokenAddress)) +
					" receiver token receiver balance " + to_string(db.safe(t.to).balance(db.safe(t.to).tokenAddress)) +
					"\n";
		}
		return out;
	}

	json flowJson(json const& _parameters)
	{
		Address from{string(_parameters["from"])};
		Address to{string(_parameters["to"])};
		Int value{string(_parameters["value"])};
		bool prune = _parameters.contains("prune") && _parameters["prune"];
#if USE_FLOW
		auto [flow, transfers] = computeFlow(from, to, db.flowGraph(), value);
#else
		auto [flow, transfers] = computeFlow(from, to, db.edges(), value, prune);
#endif

		json output;
		output["flow"] = to_string(flow);
		output["transfers"] = json::array();
		for (Edge const& t: transfers)
			output["transfers"].push_back(json{
					{"from", to_string(t.from)},
					{"to", to_string(t.to)},
					{"token", to_string(t.token)},
					{"tokenOwner", to_string(db.token(t.token).safeAddress)},
					{"value", to_string(t.capacity)}
			});
		output["debug"] = debugData(transfers);
		return output;
	}

}

extern "C"
{
	size_t loadDB(char const* _data, size_t _length)
	{
		string data(_data, _length);
		istringstream stream(data);
		size_t blockNumber{};
		tie(blockNumber, db) = BinaryImporter(stream).readBlockNumberAndDB();
		return blockNumber;
	}

	size_t edgeCount()
	{
		return db.edges().size();
	}

	char const* flow(char const* _input)
	{
		static string retVal;
		retVal = flowJson(json::parse(string(_input))).dump();
		return retVal.c_str();
	}
}

void computeFlow(
		Address const& _source,
		Address const& _sink,
		Int const& _value,
		string const& _dbDat
)
{
	ifstream stream(_dbDat);
	size_t blockNumber{};
	DB db;
	tie(blockNumber, db) = BinaryImporter(stream).readBlockNumberAndDB();
	cerr << "Edges: " << db.m_edges.size() << endl;

#if USE_FLOW
	auto [flow, transfers] = computeFlow(_source, _sink, db.flowGraph(), _value);
#else
	auto [flow, transfers] = computeFlow(_source, _sink, db.edges(), _value);
#endif

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

void computeFlowFromEdgesCSV(
		Address const& _source,
		Address const& _sink,
		Int const& _value,
		string const& _edgesCSV
)
{
	cerr << "Importing csv..." << endl;
	auto t1 = chrono::high_resolution_clock::now();
	ifstream stream(_edgesCSV);
	set<Edge> edges;
	string line;
	while (getline(stream, line))
	{
		auto it = line.begin();
		string_view parts[4];
		for (size_t i = 0; i < 4; i++)
		{
			auto partBegin = it;
			while (it != line.end() && *it != ',')
				it++;
			parts[i] = string_view(line).substr(
				static_cast<size_t>(partBegin - line.begin()),
				static_cast<size_t>(it - partBegin)
			);
			if (it != line.end() && *it == ',')
				it++;
		}
		if (parts[0] == "from")
			// ignore header
			continue;
		edges.insert(Edge{Address{parts[0]}, Address{parts[1]}, Address{parts[2]}, Int{parts[3]}});
	}
	auto t2 = chrono::high_resolution_clock::now();
	cerr << "Took " << chrono::duration_cast<chrono::duration<double>>(t2 - t1).count() << endl;

	auto [flow, transfers] = computeFlow(_source, _sink, edges, _value);

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