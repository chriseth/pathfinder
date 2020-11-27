#pragma once

#include <iostream>
#include <fstream>
#include <utility>

#include "types.h"
#include "db.h"

class BinaryImporter
{
public:
	explicit BinaryImporter(std::istream& _input): m_input(_input) {}

	std::pair<size_t, DB> readBlockNumberAndDB();
	std::set<Edge> readEdgeSet();

private:
	size_t readSize();
	Address readAddress();
	Int readInt();
	std::pair<Address, Safe> readSafe();
	Token readToken();
	Connection readConnection();
	Edge readEdge();

	Address const& address(size_t _index);

	void readAddresses();

	std::istream& m_input;
	std::vector<Address> m_addresses;
};
