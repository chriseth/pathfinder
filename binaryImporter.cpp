#include "binaryImporter.h"

#include "encoding.h"
#include "exceptions.h"

using namespace std;

DB BinaryImporter::readDB()
{
	readAddresses();

	DB db;
	size_t numSafes = readSize();
	for (size_t i = 0; i < numSafes; ++i)
	{
		Safe s = readSafe();
		db.tokens[s.tokenAddress].safeAddress = s.address;
		for (auto const& [token, balance]: s.balances)
			db.tokens[token].totalSupply += balance;
		db.safes[s.address] = move(s);
	}

	db.computeEdges();

	return db;
}

set<Edge> BinaryImporter::readEdgeSet()
{
	readAddresses();

	set<Edge> edges;

	size_t numEdges = readSize();
	for (size_t i = 0; i < numEdges; ++i)
		edges.insert(readEdge());

	return edges;
}


size_t BinaryImporter::readSize()
{
	uint64_t result{};
	m_input >> BigEndian<4>(result);
	return size_t(result);
}

Address BinaryImporter::readAddress()
{
	uint64_t index{};
	m_input >> BigEndian<4>(index);
	return m_addresses.at(index);
}

Int BinaryImporter::readInt()
{
	int bytes = uint8_t(m_input.get());
	require(bytes <= 32 && bytes > 0);
	Int v;
	for (int i = bytes - 1; i >= 0; --i)
		v.data[i / 8] |= uint64_t(uint8_t(m_input.get())) << ((i * 8) % 64);
	return v;
}

Safe BinaryImporter::readSafe()
{
	Safe s;
	s.address = readAddress();
	s.tokenAddress = readAddress();
	size_t numBalances = readSize();
	for (size_t i = 0; i < numBalances; i++)
	{
		Address token = readAddress();
		Int balance = readInt();
		s.balances[token] = balance;
	}
	size_t numLimits = readSize();
	for (size_t i = 0; i < numLimits; i++)
	{
		Address sendTo = readAddress();
		uint32_t percentage = readSize();
		require(percentage <= 100);
		s.limitPercentage[sendTo] = percentage;
	}
	return s;
}

Token BinaryImporter::readToken()
{
	Token t;
	t.address = readAddress();
	t.safeAddress = readAddress();
	t.totalSupply = readInt();
	return t;
}

Connection BinaryImporter::readConnection()
{
	Connection c;
	c.canSendToAddress = readAddress();
	c.userAddress = readAddress();
	c.limit = readInt();
	c.limitPercentage = readSize();
	return c;
}

Edge BinaryImporter::readEdge()
{
	Edge e;
	e.from = readAddress();
	e.to = readAddress();
	e.token = readAddress();
	e.capacity = readInt();
	return e;
}

void BinaryImporter::readAddresses()
{
	size_t length = readSize();
	for (size_t i = 0; i < length; ++i)
	{
		m_addresses.push_back({});
		m_input.read(reinterpret_cast<char*>(&(m_addresses.back().address[0])), 20);
	}
}
