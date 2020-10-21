#include "types.h"

#include "exceptions.h"

using namespace std;


Int timesTen(Int const& _value)
{
	Int timesTwo = _value + _value;
	Int timesFour = timesTwo + timesTwo;
	Int timesEight = timesFour + timesFour;
	return timesEight + timesTwo;
}

Int::Int(uint64_t _value)
{
	data[0] = _value;
	data[1] = 0;
	data[2] = 0;
	data[3] = 0;
}

Int::Int(string const& _decimal): Int(0)
{
	for (char c: _decimal)
	{
		require('0' <= c && c <= '9');
		*this = timesTen(*this);

		Int v(uint64_t(c - '0'));
		*this += v;
	}
}

Int& Int::operator+=(Int const& _other)
{
	uint64_t carry = 0;
	for (size_t i = 0; i < 4; i++)
	{
		uint64_t newCarry = 0;
		uint64_t x = data[i] + _other.data[i];
		if (x < data[i] || x + carry < x)
			newCarry = 1;
		data[i] = x + carry;
		carry = newCarry;
	}
	// TODO: overflow if carry is nonzero.
	// TOOD but this clashes with subtraction.
	return *this;
}

Int Int::operator-() const
{
	Int x = *this;
	for (size_t i = 0; i < 4; i++)
		x.data[i] = ~x.data[i];
	return x + Int(1);
}

Int Int::max()
{
	Int x;
	for (size_t i = 0; i < 4; i++)
		x.data[i] = uint64_t(-1);
	return x;
}

string to_string(Int _value)
{
	static vector<Int> powersOfTen;
	if (powersOfTen.empty())
	{
		Int x{1};
		while (true)
		{
			powersOfTen.push_back(x);
			Int next = timesTen(x);
			if (next < x)
				break;
			x = move(next);
		}
	}

	string result;
	for (size_t i = 0; i < powersOfTen.size(); i++)
	{
		Int const& pt = powersOfTen[powersOfTen.size() - i - 1];
		size_t digit = 0;
		while (_value >= pt)
		{
			_value -= pt;
			digit++;
		}
		if (digit != 0 || !result.empty())
			result.push_back('0' + digit);
	}
	require(_value == Int(0));
	return result.empty() ? "0" : result;
}

uint8_t fromHex(char _c)
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

char toHex(uint8_t _c)
{
	require(_c <= 0x1f);
	if (_c < 10)
		return '0' + char(_c);
	else
		return 'a' + char(_c - 10);
}

Address::Address(string const& _hex)
{
	require(_hex.size() == 2 + 2 * 20);

	for (size_t i = 2; i < _hex.size(); i += 2)
		address[i / 2 - 1] = (fromHex(_hex[i]) << 4) + fromHex(_hex[i + 1]);
}

string to_string(Address const& _address)
{
	string ret = "0x";
	for (uint8_t c: _address.address)
	{
		ret.push_back(toHex(c >> 4));
		ret.push_back(toHex(c & 0xf));
	}
	return ret;
}
