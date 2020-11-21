#include "types.h"

#include "exceptions.h"
#include "keccak.h"
#include "encoding.h"

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
	uint32_t radix = 10;
	if (_decimal.size() >= 2 && _decimal[0] == '0' && _decimal['1'] == 'x')
		radix = 16;

	for (char c: _decimal)
	{
		// TODO optimize for radix 10 and 16?
		*this *= radix;

		if (radix == 10)
			require('0' <= c && c <= '9');
		*this += Int(fromHex(c));
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

Int Int::half() const
{
	Int h;
	for (size_t i = 0; i < 4; i++)
	{
		if (i > 0)
			h.data[i - 1] |= (data[i] & 1) << 63;
		h.data[i] = data[i] >> 1;
	}
	return h;
}

Int Int::timesTwo() const
{
	Int result;
	for (size_t i = 0; i < 4; i++)
	{
		result.data[i] = data[i] << 1;
		if (i > 0)
			result.data[i] |= data[i - 1] >> 63;
	}
	return result;
}

Int Int::operator*(uint32_t _other) const
{
	Int result;
	Int shifted = *this;
	while (_other > 0)
	{
		if (_other & 1)
			result += shifted;
		shifted = shifted.timesTwo();
		_other >>= 1;
	}
	return result;
}

Int Int::operator/(uint32_t _other) const
{
	require(_other != 0);
	Int n = *this;
	Int quotient{};
	uint64_t remainder = 0;

	for (int i = 255; i >= 0; i--)
	{
		remainder <<= 1;
		if (n.data[i / 64] & (uint64_t(1) << (i % 64)))
			remainder++;
		if (remainder >= _other)
		{
			remainder -= _other;
			quotient.data[i / 64] |= uint64_t(1) << (i % 64);
		}
	}
	return quotient;
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

Address::Address(string const& _hex)
{
	if (_hex.size() >= 2 && _hex[0] == '0' && _hex[1] == 'x')
	{
		require(_hex.size() == 2 + 2 * 20);

		for (size_t i = 2; i < _hex.size(); i += 2)
			address[i / 2 - 1] = (fromHex(_hex[i]) << 4) + fromHex(_hex[i + 1]);
	}
	else
	{
		Int number(_hex);
		for (size_t i = 0; i < 20; i++)
			address[i] = (number.data[3 - (i + 12) / 8] >> (56 - 8 * ((i + 12) % 8))) & 0xff;
	}
}

string to_string(Address const& _address)
{
	string lower;
	for (uint8_t c: _address.address)
	{
		lower.push_back(toHex(c >> 4));
		lower.push_back(toHex(c & 0xf));
	}
	string hash = keccak256(lower);

	string ret = "0x";
	for (unsigned i = 0; i < 40; ++i)
	{
		char addressCharacter = lower[i];
		uint8_t nibble = hash[i / 2u] >> (4u * (1u - (i % 2u))) & 0xf;
		if (nibble >= 8)
			ret += static_cast<char>(toupper(addressCharacter));
		else
			ret += static_cast<char>(tolower(addressCharacter));
	}
	return ret;
}

Int Safe::balance(Address const& _token) const
{
	auto it = balances.find(_token);
	return it == balances.end() ? Int{0} : it->second;
}


Safe const& DB::safe(Address const& _address) const
{
	auto it = safes.find(Safe{_address, {}, {}});
	require(it != safes.end());
	return *it;
}

Token const& DB::token(Address const& _address) const
{
	auto it = tokens.find(Token{_address, {}, {}});
	require(it != tokens.end());
	return *it;
}

Token const* DB::tokenMaybe(Address const& _address) const
{
	auto it = tokens.find(Token{_address, {}, {}});
	return it == tokens.end() ? nullptr : &*it;
}
