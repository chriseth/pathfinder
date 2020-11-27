#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <set>
#include <iostream>
#include <array>


struct Int
{
	uint64_t data[4] = {};

	Int(): Int(0) {}
	explicit Int(uint64_t _value);
	explicit Int(std::string const& _value);

	Int& operator+=(Int const& _other);
	Int operator+(Int const& _other) const
	{
		Int x = *this;
		x += _other;
		return x;
	}
	Int operator-() const;
	Int& operator-=(Int const& _other)
	{
		return (*this) += -_other;
	}
	Int operator-(Int const& _other) const { return (*this) + (-_other); }
	Int operator*(uint32_t _other) const;
	Int& operator*=(uint32_t _other) { return *this = *this * _other; }
	Int operator/(uint32_t _other) const;
	Int& operator/=(uint32_t _other) { return *this = *this / _other; }

	Int half() const;
	Int timesTwo() const;

	bool operator<(Int const& _other) const
	{
		return
			std::make_tuple(data[3], data[2], data[1], data[0]) <
			std::make_tuple(_other.data[3], _other.data[2], _other.data[1], _other.data[0]);
	}
	bool operator>(Int const& _other) const { return _other < *this; }
	bool operator==(Int const& _other) const
	{
		return
			data[0] == _other.data[0] &&
			data[1] == _other.data[1] &&
			data[2] == _other.data[2] &&
			data[3] == _other.data[3];
	}
	bool operator!=(Int const& _other) const { return !(*this == _other); }
	bool operator>=(Int const& _other) const { return !(*this < _other); }
	bool operator<=(Int const& _other) const { return !(*this > _other); }

	static Int max();
};

std::string to_string(Int _value);
inline std::ostream& operator<<(std::ostream& os, Int const& _value) { return os << to_string(_value); }

struct Address
{
	std::array<uint8_t, 20> address = {};
	Address() {}
	explicit Address(std::string const& _hex);
	bool operator<(Address const& _other) const { return address < _other.address; }
	bool operator==(Address const& _other) const { return address == _other.address; }
	bool operator!=(Address const& _other) const { return address != _other.address; }
};

std::string to_string(Address const& _address);
inline std::ostream& operator<<(std::ostream& os, Address const& _address) { return os << to_string(_address); }

struct Connection
{
	Address canSendToAddress;
	Address userAddress;
	Int limit;
	uint32_t limitPercentage;

	bool operator<(Connection const& _other) const
	{
		return
			std::make_tuple(canSendToAddress, userAddress) <
			std::make_tuple(_other.canSendToAddress, _other.userAddress);
	}
};

struct Edge
{
	Address from;
	Address to;
	Address token;
	Int capacity;

	bool operator<(Edge const& _other) const
	{
		return
			std::make_tuple(from, to, token) <
			std::make_tuple(_other.from, _other.to, _other.token);
	}
};
