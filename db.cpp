#include "types.h"

#include "exceptions.h"

#include "db.h"

#include "json.hpp"

#include <fstream>

using namespace std;
using json = nlohmann::json;

Int Safe::balance(Address const& _token) const
{
	auto it = balances.find(_token);
	return it == balances.end() ? Int{0} : it->second;
}

uint32_t Safe::sendToPercentage(Address const& _sendTo) const
{
	auto it = limitPercentage.find(_sendTo);
	return it == limitPercentage.end() ? 0 : it->second;
}

Safe const& DB::safe(Address const& _address) const
{
	auto it = safes.find(_address);
	require(it != safes.end());
	return it->second;
}

Token const& DB::token(Address const& _address) const
{
	auto it = tokens.find(_address);
	require(it != tokens.end());
	return it->second;
}

Token const* DB::tokenMaybe(Address const& _address) const
{
	auto it = tokens.find(_address);
	return it == tokens.end() ? nullptr : &it->second;
}

Token* DB::tokenMaybe(Address const& _address)
{
	auto it = tokens.find(_address);
	return it == tokens.end() ? nullptr : &it->second;
}

void DB::importFromTheGraph(json const& _safesJson)
{
	safes.clear();
	tokens.clear();

	for (json const& safe: _safesJson)
	{
		Safe s;
		if (safe.contains("organization") && safe["organization"])
			s.organization = true;
		Address address = Address(string(safe["id"]));
		for (auto const& balance: safe["balances"])
		{
			Int balanceAmount = Int(string(balance["amount"]));
			Token t{Address(balance["token"]["id"]), Address(balance["token"]["owner"]["id"])};
			if (t.safeAddress == address)
				s.tokenAddress = t.address;
			tokens.insert({t.address, move(t)});
			s.balances[t.address] = balanceAmount;
		}
		safes[address] = move(s);

		for (auto const& connections: {safe["outgoing"], safe["incoming"]})
			for (auto const& connection: connections)
			{
				Address sendTo(connection["canSendToAddress"]);
				Address user(connection["userAddress"]);
				uint32_t limitPercentage = uint32_t(std::stoi(string(connection["limitPercentage"])));
				require(limitPercentage <= 100);
				if (sendTo != Address{} && user != Address{} && sendTo != user && limitPercentage > 0)
					if (safes.count(user))
						safes.at(user).limitPercentage[sendTo] = limitPercentage;
			}
	}
	computeEdges();
}


Int DB::limit(Address const& _user, Address const& _canSendTo) const
{
	Safe const* senderSafe = safeMaybe(_user);
	Safe const* receiverSafe = safeMaybe(_canSendTo);
	if (!senderSafe || !receiverSafe)
		return {};

	uint32_t sendToPercentage = senderSafe->sendToPercentage(_canSendTo);
	if (sendToPercentage == 0)
		return {};

	if (receiverSafe->organization)
		return senderSafe->balance(senderSafe->tokenAddress);

	Token const* receiverToken = tokenMaybe(receiverSafe->tokenAddress);
	if (!receiverToken)
		return {};

	Int receiverBalance = receiverSafe->balance(senderSafe->tokenAddress);

	Int amount = (receiverSafe->balance(receiverSafe->tokenAddress) * sendToPercentage) / 100;
	amount = amount < receiverBalance ? Int(0) : amount - receiverBalance;
	return min(amount, senderSafe->balance(senderSafe->tokenAddress));
}

void DB::computeEdges()
{
	cerr << "Computing Edges from " << safes.size() << " safes..." << endl;
	m_edges.clear();
	m_flowGraph.clear();
	for (auto const& safe: safes)
		computeEdgesFrom(safe.first);
	cerr << "Created " << m_edges.size() << " edges..." << endl;
}

void DB::computeEdgesFrom(Address const& _user)
{
	Safe const* safe = safeMaybe(_user);
	if (!safe)
		return;

	// Edge from user to their own token, restricted by balance.
	m_flowGraph[_user][make_pair(_user, safe->tokenAddress)] = safe->balance(safe->tokenAddress);

	// Edges along trust connections.
	for (auto const& trust: safe->limitPercentage)
	{
		Address const& sendTo = trust.first;
		if (_user == sendTo)
			continue;
		Int l = limit(_user, sendTo);
		if (l == Int(0))
			continue;
		m_edges.emplace(Edge{_user, sendTo, safe->tokenAddress, l});
		// Edge from the user/token pair to the receiver, restricted by send limit.
		m_flowGraph[make_pair(_user, safe->tokenAddress)][sendTo] = l;
	}

	// Edges that send tokens back to their owner.
	for (auto const& [tokenAddress, balance]: safe->balances)
		if (balance != Int(0))
			if (Token const* token = tokenMaybe(tokenAddress))
				if (_user != token->safeAddress)
				{
					m_edges.emplace(Edge{_user, token->safeAddress, tokenAddress, balance});
					m_flowGraph[_user][make_pair(_user, tokenAddress)] = balance;
					m_flowGraph[make_pair(_user, tokenAddress)][token->safeAddress] = balance;
				}
}

void DB::computeEdgesTo(Address const& _sendTo)
{
	Safe const* receiverSafe = safeMaybe(_sendTo);
	if (!receiverSafe)
		return;
	Address const& tokenAddress = receiverSafe->tokenAddress;

	for (auto const& [sender, safe]: safes)
	{
		if (sender == _sendTo)
			continue;

		// Edges along trust connections.
		if (safe.limitPercentage.count(sender))
		{
			Int l = limit(sender, _sendTo);
			if (l == Int(0))
				continue;
			m_edges.emplace(Edge{sender, _sendTo, safe.tokenAddress, l});
			m_flowGraph[sender][make_pair(sender, safe.tokenAddress)] = safe.balance(safe.tokenAddress);
			m_flowGraph[make_pair(sender, safe.tokenAddress)][_sendTo] = l;
		}
		// Edges that send tokens back to their owner.
		Int balance = safe.balance(tokenAddress);
		if (balance != Int{})
		{
			m_edges.emplace(Edge{sender, _sendTo, tokenAddress, balance});
			m_flowGraph[sender][make_pair(sender, tokenAddress)] = balance;
			m_flowGraph[make_pair(sender, tokenAddress)][_sendTo] = balance;
		}
	}
}

void DB::signup(Address const& _user, Address const& _token)
{
	cerr << "Signup: " << _user << " with token " << _token << endl;
	// TODO balances empty at start?
	if (!safeMaybe(_user))
		safes[_user] = Safe{_token, {}, {}, false};
	if (!tokenMaybe(_token))
		tokens[_token] = Token{_token, _user};
}

void DB::organizationSignup(Address const& _organization)
{
	cerr << "Organization signup: " << _organization << endl;
	if (!safeMaybe(_organization))
		safes[_organization] = Safe{{}, {}, {}, true};
}

void DB::trust(Address const& _canSendTo, Address const& _user, uint32_t _limitPercentage)
{
	cerr << "Trust change: " << _user << " send to " << _canSendTo << ": " << _limitPercentage << "%" << endl;
	require(_limitPercentage <= 100);

	if (Safe* safe = safeMaybe(_user))
	{
		if (_limitPercentage == 0)
			safe->limitPercentage.erase(_canSendTo);
		else
			safe->limitPercentage[_canSendTo] = _limitPercentage;

		updateEdgesFrom(_user);
		// TODO actually only this edge:
		//updateEdges(_user, _canSendTo, safe->tokenAddress);
	}
	else
		cerr << "Unknown safe." << endl;

	cerr << "Trust change update complete." << endl;
}

void DB::transfer(
	Address const& _token,
	Address const& _from,
	Address const& _to,
	Int const& _value
)
{
	cerr << "Transfer: " << _value << ": " << _from << " -> " << _to << " [" << _token << "]" << endl;
	// This is a generic ERC20 event and might be unrelated to the
	// Circles system.
	Token* token = tokenMaybe(_token);
	if (!token || _value == Int{})
	{
		if (!token)
			cerr << "Token unknown." << endl;
		return;
	}

	Safe* senderSafe = nullptr;
	if (_from == Address{})
		require(_to == token->safeAddress);
	else
	{
		senderSafe = safeMaybe(_from);
		if (!senderSafe)
		{
			cerr << "Unknown sender safe." << endl;
			return;
		}
	}

	Safe* receiverSafe = safeMaybe(_to);
	if (receiverSafe)
		receiverSafe->balances[_token] += _value;
	else
	{
		cerr << "Unknown receiver safe." << endl;
		return;
	}

	if (_from == Address{})
	{
		// Token minted.
		updateEdgesTo(_to);
		updateEdgesFrom(_to);
	}
	else
	{
		// Regular transfer
		require(senderSafe->balances[_token] >= _value);
		senderSafe->balances[_token] -= _value;
		// TODO actually only the token
		// TODO really all of them?
		updateEdgesFrom(_from);
		updateEdgesFrom(_to);
		updateEdgesTo(_from);
		updateEdgesTo(_to);
	}
	cerr << "Update following transfer complete." << endl;
}

void DB::updateEdgesFrom(Address const& _from)
{
	if (m_delayEdgeUpdates)
		return;

	cerr << "Updating edges from " << _from << endl;

	// TODO this loop can be optimized because of the sort order.
	for (auto it = m_edges.begin(); it != m_edges.end();)
		if (it->from == _from)
			it = m_edges.erase(it);
		else
			++it;

	m_flowGraph.erase(_from);
	cerr << "erasing pseudo-edges..." <<endl;
	m_flowGraph.erase(
		m_flowGraph.lower_bound(make_pair(_from, Address{})),
		m_flowGraph.upper_bound(make_pair(_from, Address{"0xffffffffffffffffffffffffffffffffffffffff"s}))
	);
	cerr << "done" << endl;

	computeEdgesFrom(_from);

	cerr << "Done." << endl;
}

void DB::updateEdgesTo(Address const& _to)
{
	if (m_delayEdgeUpdates)
		return;

	cerr << "Updating edges to " << _to << endl;
	for (auto it = m_edges.begin(); it != m_edges.end();)
		if (it->to == _to)
			it = m_edges.erase(it);
		else
			++it;
	cerr << "erasing pseudo-edges..." <<endl;
	// TODO this does not leave the graph in a clean state, but
	// it is probably enough.
	for (auto& [node, targets]: m_flowGraph)
		targets.erase(_to);
	cerr << "done" << endl;

	computeEdgesTo(_to);

	cerr << "Done." << endl;
}
