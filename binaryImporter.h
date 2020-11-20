#pragma once

#include <iostream>
#include <fstream>


#include "types.h"


class BinaryImporter
{
public:
	explicit BinaryImporter(std::istream& _input): m_input(_input) {}

	DB readDB();
	std::set<Edge> readEdgeSet();

private:
	size_t readSize();
	Address readAddress();
	Int readInt();
	Safe readSafe();
	Token readToken();
	Connection readConnection();
	Edge readEdge();

	Address const& address(size_t _index);

	void readAddresses();

	std::istream& m_input;
	std::vector<Address> m_addresses;
};
