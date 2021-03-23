#pragma once

#include "types.h"
#include "json.hpp"

struct Token
{
	Address address;
	Address safeAddress;

	bool operator<(Token const& _other) const { return address < _other.address; }
};

struct Safe
{
	Address tokenAddress;
	/// token address to balance
	std::map<Address, Int> balances;
	/// Limit percentage in "send to" direction.
	std::map<Address, uint32_t> limitPercentage;
	bool organization;

	Int balance(Address const& _token) const;
	uint32_t sendToPercentage(Address const& _sendToUser) const;
};

struct DB
{
	std::map<Address, Safe> safes;
	std::map<Address, Token> tokens;
	std::set<Edge> m_edges;

	bool m_delayEdgeUpdates = false;

	Safe const& safe(Address const& _address) const;
	Safe* safeMaybe(Address const& _address)
	{
		auto it = safes.find(_address);
		return it == safes.end() ? nullptr : &it->second;
	}
	Safe const* safeMaybe(Address const& _address) const
	{
		auto it = safes.find(_address);
		return it == safes.end() ? nullptr : &it->second;
	}

	Token const& token(Address const& _address) const;
	Token const* tokenMaybe(Address const& _address) const;
	Token* tokenMaybe(Address const& _address);

	void importFromTheGraph(nlohmann::json const& _file);

	/// @returns how much of @a _user's token they can send to @a _canSendTo.
	Int limit(Address const& _user, Address const& _canSendTo) const;

	void computeEdges();
	void computeEdgesFrom(Address const& _user);
	void computeEdgesTo(Address const& _user);
	std::set<Edge> const& edges() const { return m_edges; }

	void updateLimit(DB const& _db, Connection& _connection);

	void signup(Address const& _user, Address const& _token);
	void organizationSignup(Address const& _organization);
	void trust(Address const& _canSendTo, Address const& _user, uint32_t _limitPercentage);
	void transfer(Address const& _token, Address const& _from, Address const& _to, Int const& _value);

	void updateEdgesFrom(Address const& _from);
	void updateEdgesTo(Address const& _to);

	void delayEdgeUpdates() { m_delayEdgeUpdates = true; }
	void performEdgeUpdates() { m_delayEdgeUpdates = false; computeEdges(); }
};
