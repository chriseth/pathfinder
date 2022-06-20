#include "types.h"
#include "exceptions.h"
#include "db.h"

using namespace std;

Int Safe::balance(Address const& _token) const
{
	auto it = balances.find(_token);
	return it == balances.end()
		? Int{0}
		: it->second;
}

uint32_t Safe::sendToPercentage(Address const& _sendTo) const
{
	auto it = limitPercentage.find(_sendTo);
	return it == limitPercentage.end()
		? 0
		: it->second;
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
	return it == tokens.end()
		? nullptr
		: &it->second;
}

Token* DB::tokenMaybe(Address const& _address)
{
	auto it = tokens.find(_address);
	return it == tokens.end()
		? nullptr
		: &it->second;
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
	for (auto const& safe: safes) {
		computeEdgesFrom(safe.first);
	}
	cerr << "Created " << m_edges.size() << " edges..." << endl;
}

void DB::computeEdgesFrom(Address const& _user)
{
	Safe const* safe = safeMaybe(_user);
	if (!safe)
		return;

	// Edge from user to their own token, restricted by balance.
	if (safe->tokenAddress != Address{})
		m_flowGraph[_user][make_pair(_user, safe->tokenAddress)] = safe->balance(safe->tokenAddress);

	// Edges along trust connections.
	for (auto const& trust: safe->limitPercentage)
	{
		Address const& sendTo = trust.first;
		if (_user == sendTo) {
			continue;
		}
		Int l = limit(_user, sendTo);
		if (l == Int(0)) {
			continue;
		}
		m_edges.emplace(Edge{_user, sendTo, safe->tokenAddress, l});
		// Edge from the user/token pair to the receiver, restricted by send limit.
		m_flowGraph[make_pair(_user, safe->tokenAddress)][sendTo] = l;
	}

	// Edges that send tokens back to their owner.
	for (auto const& [tokenAddress, balance]: safe->balances) {
		if (balance == Int(0)) {
			continue;
		}

		Token const *token = tokenMaybe(tokenAddress);
		if (!token) {
			continue;
		}

		if (_user == token->safeAddress) {
			continue;
		}

		m_edges.emplace(Edge{_user, token->safeAddress, tokenAddress, balance});
		m_flowGraph[_user][make_pair(_user, tokenAddress)] = balance;
		m_flowGraph[make_pair(_user, tokenAddress)][token->safeAddress] = balance;
	}
}