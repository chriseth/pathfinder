#include "importGraph.h"
#include "flow.h"
#include "exceptions.h"
#include "binaryExporter.h"
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
/// @returns the incoming and outgoing trust edges for a given user with limit percentages.
json adjacenciesJson(string const& _user)
{
	Address user{string(_user)};

	json output = json::array();
	for (auto const& [address, safe]: db.safes)
		for (auto const& [sendTo, percentage]: safe.limitPercentage)
			if (sendTo != address && (user == address || user == sendTo))
				output.push_back({
					{"user", to_string(sendTo)},
					{"percentage", percentage},
					{"trusts", to_string(user == sendTo ? address : user)}
				});
	return output;
}

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

void delayEdgeUpdates()
{
	cerr << "Delaying edge updates." << endl;
	db.delayEdgeUpdates();
}

void performEdgeUpdates()
{
	db.performEdgeUpdates();
}

void signup(char const* _user, char const* _token)
{
	db.signup(Address(string(_user)), Address(string(_token)));
}

void organizationSignup(char const* _organization)
{
	db.organizationSignup(Address(string(_organization)));
}

void trust(char const* _canSendTo, char const* _user, int _limitPercentage)
{
	db.trust(Address(string(_canSendTo)), Address(string(_user)), uint32_t(_limitPercentage));
}

void transfer(char const* _token, char const* _from, char const* _to, char const* _value)
{
	db.transfer(
		Address(string(_token)),
		Address(string(_from)),
		Address(string(_to)),
		Int(string(_value))
	);
}

/// @returns the incoming and outgoing trust edges for a given user with limit percentages.
char const* adjacencies(char const* _user)
{
	static string retVal;
	retVal = adjacenciesJson(string(_user)).dump();
	return retVal.c_str();
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
		string parts[4];
		for (size_t i = 0; i < 4; i++)
		{
			string& part = parts[i];
			while (it != line.end() && *it != ',')
				part += *(it++);
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

void importDB(string const& _safesJson, string const& _dbDat)
{
	ifstream graph(_safesJson);
	json safesJson;
	graph >> safesJson;
	graph.close();

	string blockNumberStr(safesJson["blockNumber"]);
	size_t blockNumber(size_t(stoi(blockNumberStr, nullptr, 0)));
	require(blockNumber > 0);
	cerr << "Block number: " << blockNumber << endl;

	DB db;
	db.importFromTheGraph(safesJson["safes"]);
	BinaryExporter(_dbDat).write(blockNumber, db);
}

/*
void dbToEdges(string const& _dbDat, string const& _edgesDat)
{
	ifstream in(_dbDat);
	DB db = BinaryImporter(in).readDB();
	edgeSetToBinary(
		db.computeEdges(),
		_edgesDat
	);
}


set<Edge> computeDiff(set<Edge> const& _oldEdges, set<Edge> const& _newEdges)
{
	set<Edge> diff;

	auto itA = _oldEdges.begin();
	auto itB = _newEdges.begin();

	while (itA != _oldEdges.end() || itB != _newEdges.end())
	{
		if (itB == _newEdges.end() || *itA < *itB)
		{
			Edge removed(*itA);
			removed.capacity = Int(0);
			diff.insert(diff.end(), move(removed));
			++itA;
		}
		else if (itA == _oldEdges.end() || *itB < *itA)
		{
			diff.insert(diff.end(), *itB);
			++itB;
		}
		else
		{
			require(itA->from == itB->from && itA->to == itB->to && itA->token == itB->token);
			if (itA->capacity != itB->capacity)
				diff.insert(diff.end(), *itB);
			++itA;
			++itB;
		}
	}
	return diff;
}

void computeDiff(string const& _oldEdges, string const& _newEdges, string const& _diff)
{
	set<Edge> oldEdges = importEdges(_oldEdges);
	cout << "Old edges: " << oldEdges.size() << endl;
	set<Edge> newEdges = importEdges(_newEdges);
	cout << "New edges: " << newEdges.size() << endl;
	set<Edge> diff = computeDiff(oldEdges, newEdges);
	cout << "Size of diff: " << diff.size() << endl;

	edgeSetToBinary(diff, _diff);
}

set<Edge> applyDiff(set<Edge> const& _oldEdges, set<Edge> const& _diff)
{
	set<Edge> newEdges;

	auto itA = _oldEdges.begin();
	auto itB = _diff.begin();

	while (itA != _oldEdges.end() || itB != _diff.end())
	{
		if (itB == _diff.end() || *itA < *itB)
		{
			newEdges.insert(newEdges.end(), *itA);
			++itA;
		}
		else if (itA == _oldEdges.end() || *itB < *itA)
		{
			newEdges.insert(newEdges.end(), *itB);
			++itB;
		}
		else
		{
			require(itA->from == itB->from && itA->to == itB->to && itA->token == itB->token);
			if (itB->capacity != Int(0))
				newEdges.insert(*itB);
			else
				cout << "Skipping " << itB->from << endl;
			++itA;
			++itB;
		}
	}
	return newEdges;
}


void applyDiff(string const& _oldEdges, string const& _diff, string const& _newEdges)
{
	set<Edge> oldEdges = importEdges(_oldEdges);
	cout << "Old edges: " << oldEdges.size() << endl;
	set<Edge> diff = importEdges(_diff);
	cout << "Diff: " << diff.size() << endl;
	set<Edge> newEdges = applyDiff(oldEdges, diff);
	cout << "New edges: " << newEdges.size() << endl;

	edgeSetToBinary(newEdges, _newEdges);
}
*/

void jsonMode()
{
	map<string, function<json(json const&)>> functions{
		{"loaddb", [](json const& _input) {
			ifstream instream{string{_input["file"]}};
			size_t blockNumber;
			tie(blockNumber, db) = BinaryImporter(instream).readBlockNumberAndDB();
			return json{{"blockNumber", blockNumber}};
		}},
		{"loaddbStream", [](json const& _input) {
			string data = fromHexStream(_input["data"]);
			istringstream instream{data};
			size_t blockNumber;
			tie(blockNumber, db) = BinaryImporter(instream).readBlockNumberAndDB();
			return json{{"blockNumber", blockNumber}};
		}},
		{"dumpdb", [](json const& _input) {
			BinaryExporter(string{_input["file"]}).write(size_t(_input["blockNumber"]), db);
			return json{};
		}},
		{"exportJson", [](json const&) { return db.exportToJson(); }},
		{"flow", [](json const& _input) { return flowJson(_input); }},
		{"adjacencies", [](json const& _input) { return json{{"adjacencies", adjacenciesJson(_input["user"])}}; }},
		{"edgeCount", [](json const&) { return json{{"edgeCount", db.edges().size()}}; }},
		{"delayEdgeUpdates", [](json const&) { db.delayEdgeUpdates(); return json{}; }},
		{"performEdgeUpdates", [](json const&) { db.performEdgeUpdates(); return json{}; }},
		{"signup", [](json const& _input) {
			db.signup(Address(_input["user"]), Address(_input["token"]));
			return json{};
		}},
		{"organizationSignup", [](json const& _input) {
			db.organizationSignup(Address(_input["organization"]));
			return json{};
		}},
		{"trust", [](json const& _input) {
			db.trust(Address(_input["canSendTo"]), Address(_input["user"]), uint32_t(_input["limitPercentage"]));
			return json{};
		}},
		{"transfer", [](json const& _input) {
			db.transfer(Address(_input["token"]), Address(_input["from"]), Address(_input["to"]), Int(string(_input["value"])));
			return json{};
		}}
	};
	while (std::cin)
	{
		string line;
		getline(std::cin, line);
		if (line.empty())
			return;
		json input = json::parse(line);
		string cmd = input["cmd"];
		json id = input["id"];
		json output;
		if (functions.count(cmd))
		{
			try
			{
				output = functions.at(cmd)(input);
			}
			catch (...)
			{
				output = json{{"error", "Exception occurred."}};
			}
		}
		else
			output = json{{"error", "Command not found."}};
		output["id"] = id;
		cout << output.dump() << endl;
	}
}

int main(int argc, char const** argv)
{
	if (argc == 4 && argv[1] == string{"--importDB"})
		importDB(argv[2], argv[3]);
	else if (argc == 2 && argv[1] == string{"--json"})
		jsonMode();
//	else if (argc == 4 && argv[1] == string{"--dbToEdges"})
//		dbToEdges(argv[2], argv[3]);
//	else if (argc == 5 && argv[1] == string{"--computeDiff"})
//		computeDiff(argv[2], argv[3], argv[4]);
//	else if (argc == 5 && argv[1] == string{"--applyDiff"})
//		applyDiff(argv[2], argv[3], argv[4]);
	else if ( argc == 6 && argv[1] == string{"--flowcsv"})
		computeFlowFromEdgesCSV(Address(string(argv[2])), Address(string(argv[3])), Int(string(argv[4])), argv[5]);
	else if (
		(argc == 6 && argv[1] == string{"--flow"}) ||
		(argc == 5 && string(argv[1]).substr(0, 2) != "--")
	)
		computeFlow(Address(string(argv[1])), Address(string(argv[2])), Int(string(argv[3])), argv[4]);
	else
	{
		cerr << "Usage: " << argv[0] << " <from> <to> <value> <edges.dat>" << endl;
		cerr << "Options: " << endl;
		cerr << "  --json                                     JSON mode via stdin/stdout." << endl;
		cerr << "  --flowcsv <from> <to> <value> <edges.csv>  Compute max flow up to <value> from edges csv and output transfer steps in json." << endl;
		cerr << "  [--flow] <from> <to> <value> <db.dat>      Compute max flow up to <value> and output transfer steps in json." << endl;
		cerr << "  --importDB <safes.json> <db.dat>           Import safes with trust edges and generate transfer limit graph." << endl;
		cerr << "  --dbToEdges <db.dat> <edges.dat>           Import safes with trust edges and generate transfer limit graph." << endl;
		cerr << "  --computeDiff <old.dat> <new.dat> <diff.dat>  Compute a difference file." << endl;
		cerr << "  --applyDiff <old.dat> <diff.dat> <out.dat>    Apply a previously computed difference file." << endl;
		cerr << "  [--help]                                      This help screen." << endl;
		return 1;
	}
	return 0;
}
