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
	bool organization{false};

	Int balance(Address const& _token) const;
	uint32_t sendToPercentage(Address const& _sendToUser) const;
};

struct DB
{
	std::map<Address, Safe> safes;
	std::map<Address, Token> tokens;
	/// Trust edges.
	std::set<Edge> m_edges;

	/// Adjacency list of the flow graph.
	/// The trust graph is a multi-graph, but this one introduces
	/// one node on each edge of the trust graph.
	std::map<FlowGraphNode, std::map<FlowGraphNode, Int>> m_flowGraph;

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

	/// @returns how much of @a _user's token they can send to @a _canSendTo.
	Int limit(Address const& _user, Address const& _canSendTo) const;

	void computeEdges();
	void computeEdgesFrom(Address const& _user);
	std::set<Edge> const& edges() const { return m_edges; }
	std::map<FlowGraphNode, std::map<FlowGraphNode, Int>> const& flowGraph() const { return m_flowGraph; }
};
