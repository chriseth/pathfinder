#include "importGraph.h"

#include "exceptions.h"

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
		Safe s{Address(string(safe["id"])), {}};
		for (auto const& balance: safe["balances"])
		{
			Token t{Address(balance["token"]["id"]), Address(balance["token"]["owner"]["id"])};
			db.tokens.insert(t);
			s.balances[t.address] = Int(string(balance["amount"]));
		}
		db.safes.insert(move(s));

		for (auto const& connections: {safe["outgoing"], safe["incoming"]})
			for (auto const& connection: connections)
				db.connections.emplace(Connection{
					Address(connection["canSendToAddress"]),
					Address(connection["userAddress"]),
					Int(string(connection["limit"]))
				});
	}
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


template <size_t _bytes>
struct BigEndian
{
	variant<uint64_t, uint64_t*> value;
	BigEndian(uint64_t const& _value): value(_value) {}
	BigEndian(uint64_t& _value): value(&_value) {}
	friend ostream& operator<<(ostream& _os, BigEndian const& _value)
	{
		for (size_t i = 0; i < _bytes; ++i)
			_os.put(char((get<uint64_t>(_value.value) >> ((_bytes - 1 - i) * 8)) & 0xff));
		return _os;
	}
	friend istream& operator>>(istream& _is, BigEndian const& _value)
	{
		uint64_t& v = *get<uint64_t*>(_value.value);
		v = 0;
		for (size_t i = 0; i < _bytes; ++i)
			v |= uint64_t(uint8_t(_is.get())) << ((_bytes - i - 1) * 8);
		return _is;
	}
};

template <size_t _bytes>
void writeBigEndian(ofstream& _stream, uint64_t const& _value)
{
	for (size_t i = 0; i < _bytes; ++i)
		_stream.put(uint8_t((_value >> ((_bytes - 1 - i) * 8)) & 0xff));
}

void writeCompactInt(ostream& _stream, Int const& _v)
{
	bool wroteLength = false;
	for (int i = 31; i >= 0; --i)
	{
		uint64_t data = (_v.data[i / 8] >> ((i * 8) % 64)) & 0xff;
		if (!wroteLength && (i == 0 || data != 0))
		{
			_stream.put(char(size_t(i + 1)));
			wroteLength = true;
		}
		if (wroteLength)
			_stream.put(char(data));
	}
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

vector<Address> sortedUniqueAddresses(set<Edge> const& _edges)
{
	set<Address> addresses;
	for (Edge const& edge: _edges)
	{
		addresses.insert(edge.from);
		addresses.insert(edge.to);
		addresses.insert(edge.token);
	}
	return vector<Address>{addresses.begin(), addresses.end()};
}

size_t indexOf(vector<Address> const& _sortedAddresses, Address const& _address)
{
	auto it = lower_bound(_sortedAddresses.begin(), _sortedAddresses.end(), _address);
	require(it != _sortedAddresses.end());
	return size_t(it - _sortedAddresses.begin());
}

void edgeSetToBinary(set<Edge> const& _edges, string const& _file)
{
	vector<Address> addresses = sortedUniqueAddresses(_edges);
	require(addresses.size() < numeric_limits<uint32_t>::max());

	cout << "Exporting " << _edges.size() << " edges and " << addresses.size() << " addresses." << endl;

	ofstream f(_file);
	f << BigEndian<4>(addresses.size());
	for (Address const& address: addresses)
		f.write(reinterpret_cast<char const*>(&(address.address[0])), 20);

	for (Edge const& edge: _edges)
	{
		f <<
			BigEndian<4>(indexOf(addresses, edge.from)) <<
			BigEndian<4>(indexOf(addresses, edge.to)) <<
			BigEndian<4>(indexOf(addresses, edge.token));
		writeCompactInt(f, edge.capacity);
	}

	f.close();
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
