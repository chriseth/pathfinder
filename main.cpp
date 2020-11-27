#include "importGraph.h"
#include "flow.h"
#include "exceptions.h"
#include "binaryExporter.h"
#include "binaryImporter.h"
#include "dbUpdates.h"

#include "json.hpp"

#include <iostream>
#include <sstream>

using namespace std;
using json = nlohmann::json;

DB db;

extern "C"
{
size_t loadDB(char const* _data, size_t _length)
{
	string data(_data, _length);
	istringstream stream(data);
	db = BinaryImporter(stream).readDB();
	return db.safes.size();
}

size_t edgeCount()
{
	return db.edges().size();
}

void delayEdgeUpdates()
{
	cout << "Delaying edge updates." << endl;
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

char const* flow(char const* _input)
{
	static string retVal;

	json parameters = json::parse(string(_input));
	Address from{string(parameters["from"])};
	Address to{string(parameters["to"])};
	Int value{string(parameters["value"])};
	auto [flow, transfers] = computeFlow(from, to, db.edges(), value);

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
}

void computeFlow(
	Address const& _source,
	Address const& _sink,
	Int const& _value,
	string const& _dbDat
)
{
	ifstream stream(_dbDat);
	DB db = BinaryImporter(stream).readDB();
	cout << "Edges: " << db.m_edges.size() << endl;

	auto [flow, transfers] = computeFlow(_source, _sink, db.edges(), _value);
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
	DB db;
	db.importFromTheGraph(_safesJson);
	BinaryExporter(_dbDat).write(db);
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

int main(int argc, char const** argv)
{
	if (argc == 4 && argv[1] == string{"--importDB"})
		importDB(argv[2], argv[3]);
//	else if (argc == 4 && argv[1] == string{"--dbToEdges"})
//		dbToEdges(argv[2], argv[3]);
//	else if (argc == 5 && argv[1] == string{"--computeDiff"})
//		computeDiff(argv[2], argv[3], argv[4]);
//	else if (argc == 5 && argv[1] == string{"--applyDiff"})
//		applyDiff(argv[2], argv[3], argv[4]);
	else if (
		(argc == 6 && argv[1] == string{"--flow"}) ||
		(argc == 5 && string(argv[1]).substr(0, 2) != "--")
	)
		computeFlow(Address(string(argv[1])), Address(string(argv[2])), Int(string(argv[3])), argv[4]);
	else
	{
		cerr << "Usage: " << argv[0] << " <from> <to> <value> <edges.dat>" << endl;
		cerr << "Options: " << endl;
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
