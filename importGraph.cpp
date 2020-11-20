#include "importGraph.h"

#include "exceptions.h"
#include "encoding.h"
#include "binaryExporter.h"
#include "dbUpdates.h"

#include "json.hpp"

#include <fstream>
#include <iostream>
#include <set>
#include <variant>

using namespace std;
using json = nlohmann::json;

DB importGraph(string const& _file)
{
	ifstream graph(_file);
	json safes;
	graph >> safes;
	graph.close();

	DB db;

	for (json const& safe: safes)
	{
		Safe s{Address(string(safe["id"])), {}, {}};
		for (auto const& balance: safe["balances"])
		{
			Int balanceAmount = Int(string(balance["amount"]));
			Token t{Address(balance["token"]["id"]), Address(balance["token"]["owner"]["id"]), {}};
			if (t.safeAddress == s.address)
				s.tokenAddress = t.address;
			// Insert and update total supply.
			// const_cast is needed because set iterators are const.
			const_cast<Int&>(db.tokens.insert(t).first->totalSupply) += balanceAmount;
			s.balances[t.address] = balanceAmount;
		}
		db.safes.insert(move(s));

		for (auto const& connections: {safe["outgoing"], safe["incoming"]})
			for (auto const& connection: connections)
				db.connections.emplace(Connection{
					Address(connection["canSendToAddress"]),
					Address(connection["userAddress"]),
					Int(string(connection["limit"])),
					uint32_t(std::stoi(string(connection["limitPercentage"])))
				});
	}
	cout << "Updating limits..." << endl;
	for (Connection const& conn: db.connections)
		checkLimit(db, const_cast<Connection&>(conn));
	cout << "Imported " << db.safes.size() << " safes." << endl;

	return db;
}

set<Edge> findEdgesInGraphData(DB const& _db)
{
	set<Connection> extendedConnections = _db.connections;

	// Add connections to the original token's safe
	for (Safe const& safe: _db.safes)
		for (auto const& [tokenAddress, balance]: safe.balances)
			// TODO what if the connection is already there?
			extendedConnections.insert(Connection{
				_db.token(tokenAddress).safeAddress,
				safe.address,
				balance,
				100 // fake percentage
			});

	set<Edge> edges;

	for (Connection const& connection: extendedConnections)
	{
		Safe const* senderSafe = _db.safeMaybe(connection.userAddress);
		// TODO self-edges are ignored, so we can probably already filter them earlier.
		if (!senderSafe || senderSafe->address == connection.canSendToAddress)
			continue;

		for (auto const& [tokenAddress, balance]: senderSafe->balances)
		{
			Token const& token = _db.token(tokenAddress);
			// TODO can there be mulitple such connections?
			auto it = extendedConnections.find(Connection{connection.canSendToAddress, token.safeAddress, {}, {}});
			if (it == extendedConnections.end())
				continue;
			Int capacity = min(balance, it->limit);
			if (capacity == Int(0))
				continue;

			Edge edge{
				connection.userAddress,
				connection.canSendToAddress,
				tokenAddress,
				capacity
			};
			edges.emplace(move(edge));
		}
	}

	cout << "Computed edges " << endl;
	return edges;
}

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


Int readCompactInt(istream& _stream)
{
	int bytes = uint8_t(_stream.get());
	require(bytes <= 32 && bytes > 0);
	Int v;
	for (int i = bytes - 1; i >= 0; --i)
		v.data[i / 8] |= uint64_t(uint8_t(_stream.get())) << ((i * 8) % 64);
	return v;
}

void edgeSetToBinary(set<Edge> const& _edges, string const& _file)
{
	BinaryExporter(_file).write(_edges);
}

set<Edge> importEdgesBinary(string const& _file)
{
	ifstream f(_file);
	set<Edge> edges = importEdgesBinary(f);
	f.close();
	return edges;
}

set<Edge> importEdgesBinary(istream& _stream)
{
	vector<Address> addresses;
	set<Edge> edges;

	uint64_t numAddresses{};
	_stream >> BigEndian<4>(numAddresses);
	for (size_t i = 0; i < numAddresses; i++)
	{
		Address address;
		_stream.read(reinterpret_cast<char*>(&(address.address[0])), 20);
		addresses.emplace_back(move(address));
	}

	while (_stream.peek() != EOF)
	{
		Edge edge;
		uint64_t index{};
		_stream >> BigEndian<4>(index);
		edge.from = addresses.at(index);
		_stream >> BigEndian<4>(index);
		edge.to = addresses.at(index);
		_stream >> BigEndian<4>(index);
		edge.token = addresses.at(index);
		edge.capacity = readCompactInt(_stream);
		edges.insert(edge);
	}

	return edges;
}
