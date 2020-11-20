#pragma once

#include <iostream>
#include <fstream>


#include "types.h"


class BinaryExporter
{
public:
	explicit BinaryExporter(std::string const& _file);

	void write(DB const& _db);
	void write(std::set<Edge> const& _edges);

private:
	template<typename T>
	void write(std::vector<T> const& _vector)
	{
		write(_vector.size());
		for (auto const& element: _vector)
			write(element);
	}
	template<typename T>
	void write(std::set<T> const& _vector)
	{
		write(_vector.size());
		for (auto const& element: _vector)
			write(element);
	}
	template<typename K, typename V>
	void write(std::map<K, V> const& _map)
	{
		write(_map.size());
		for (auto const& [key, value]: _map)
		{
			write(key);
			write(value);
		}
	}
	void write(std::size_t const& _size);
	void write(Address const& _address);
	void write(Int const& _v);
	void write(Safe const& _safe);
	void write(Token const& _token);
	void write(Connection const& _connection);
	void write(Edge const& _edge);

	void writeAddresses(DB const& _db);
	void writeAddresses(std::set<Edge> const& _edges);
	void writeAddresses(std::set<Address> const& _addresses);
	void writeAddresses();

	size_t indexOf(Address const& _address);

	std::ofstream m_file;
	std::vector<Address> m_addresses;
};
