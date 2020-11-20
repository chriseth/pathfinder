#pragma once

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
