#pragma once

#include "exceptions.h"

#include <variant>
#include <iostream>


template <size_t _bytes>
struct BigEndian
{
	std::variant<uint64_t, uint64_t*> value;

	BigEndian(uint64_t const& _value): value(_value) {}
	BigEndian(uint64_t& _value): value(&_value) {}

	friend std::ostream& operator<<(std::ostream& _os, BigEndian const& _value)
	{
		for (size_t i = 0; i < _bytes; ++i)
			_os.put(char((std::get<uint64_t>(_value.value) >> ((_bytes - 1 - i) * 8)) & 0xff));
		return _os;
	}
	friend std::istream& operator>>(std::istream& _is, BigEndian const& _value)
	{
		uint64_t& v = *std::get<uint64_t*>(_value.value);
		v = 0;
		for (size_t i = 0; i < _bytes; ++i)
			v |= uint64_t(uint8_t(_is.get())) << ((_bytes - i - 1) * 8);
		return _is;
	}
};

inline uint8_t fromHex(char _c)
{
	if ('0' <= _c && _c <= '9')
		return uint8_t(_c - '0');
	else if ('a' <= _c && _c <= 'f')
		return 10 + uint8_t(_c - 'a');
	else if ('A' <= _c && _c <= 'F')
		return 10 + uint8_t(_c - 'A');
	else
		require(false);
	return {};
}

inline std::string fromHexStream(std::string const& _input)
{
	std::string out;
	for (size_t i = 0; i + 1 < _input.size(); i += 2)
		out.push_back(static_cast<char>((fromHex(_input[i]) << 4) | fromHex(_input[i + 1])));
	return out;
}

inline char toHex(uint8_t _c)
{
	require(_c <= 0x1f);
	if (_c < 10)
		return '0' + char(_c);
	else
		return 'a' + char(_c - 10);
}

