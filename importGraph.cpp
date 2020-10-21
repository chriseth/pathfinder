#include "importGraph.h"

#include "json.hpp"

#include <fstream>
#include <iostream>
#include <set>

using namespace std;
using json = nlohmann::json;

DB importGraph(char const* _file)
{
	DB db;

	ifstream graph(_file);

	string line;
	while (getline(graph, line))
	{
		json data = json::parse(line);

		Safe s{Address(string(data["id"])), {}};
		for (auto const& balance: data["balances"])
		{
			Token t{Address(balance["token"]["id"]), Address(balance["token"]["owner"]["id"])};
			db.tokens.insert(t);
			s.balances[t.address] = Int(string(balance["amount"]));
		}
		db.safes.insert(move(s));

		for (auto const& connections: {data["outgoing"], data["incoming"]})
			for (auto const& connection: connections)
				db.connections.emplace(Connection{
					Address(connection["canSendToAddress"]),
					Address(connection["userAddress"]),
					Int(string(connection["limit"]))
				});
	}

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
				balance
			});

	set<Edge> edges;

	for (Connection const& connection: extendedConnections)
	{
		Safe const* senderSafe = _db.safeMaybe(connection.userAddress);
		if (!senderSafe || senderSafe->address == connection.canSendToAddress)
			continue;

		for (auto const& [tokenAddress, balance]: senderSafe->balances)
		{
			Token const& token = _db.token(tokenAddress);
			// TODO can there be mulitple such connections?
			auto it = extendedConnections.find(Connection{connection.canSendToAddress, token.safeAddress, {}});
			Int capacity = min(balance, it->limit);
			if (it == extendedConnections.end() || capacity == Int(0))
				continue;

			// TODO the js alg divides capacities by 10**18 here.
			Edge edge{
				connection.userAddress,
				connection.canSendToAddress,
				tokenAddress,
				capacity
			};
			edges.emplace(move(edge));
		}
	}

//	ofstream out("/tmp/edges.dat");
//	for (Edge const& e: edges)
//	{
//		out.write(reinterpret_cast<char const*>(&(e.from.address[0])), 20);
//		out.write(reinterpret_cast<char const*>(&(e.to.address[0])), 20);
//		out.write(reinterpret_cast<char const*>(&(e.token.address[0])), 20);
//		out.write(reinterpret_cast<char const*>(&(e.capacity.data[0])), 32);
//	}
//	out.close();

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

set<Edge> importEdges(char const* _file)
{
	ifstream f(_file);
	json edgesJson;
	f >> edgesJson;
	f.close();

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
