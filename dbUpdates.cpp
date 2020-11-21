#include "dbUpdates.h"

#include "exceptions.h"


using namespace std;

void updateLimit(DB const& _db, Connection& _connection)
{
	// tokenOwner and src are always equal

	Int limit(0);
	if (_db.safeMaybe(_connection.userAddress) && _db.safeMaybe(_connection.canSendToAddress))
	{
		Safe const& senderSafe = _db.safe(_connection.userAddress);
		Safe const& receiverSafe = _db.safe(_connection.canSendToAddress);
		Int receiverBalance = receiverSafe.balance(senderSafe.tokenAddress);

		if (_connection.userAddress == _connection.canSendToAddress)
			limit = receiverBalance;
		else
		{
			require(_connection.limitPercentage <= 100);
			Int max = _db.token(receiverSafe.tokenAddress).totalSupply;
			max = (max * _connection.limitPercentage) / 100;
			limit = max < receiverBalance ? Int(0) : max - receiverBalance;
		}
	}

	_connection.limit = limit;
	cout << "Limit change: " << _connection.userAddress << " send to " << _connection.canSendToAddress << ": " << _connection.limit << " (" << _connection.limitPercentage << "%)" << endl;
}

void updateLimit(DB& _db, Address const& _user, Address const& _canSendTo)
{
	auto it = _db.connections.find(Connection{_canSendTo, _user, {}, {}});
	if (it != _db.connections.end())
		updateLimit(_db, const_cast<Connection&>(*it));
}


void signup(DB& _db, Address const& _user, Address const& _token)
{
	cout << "Signup: " << _user << " with token " << _token << endl;
	// TODO balances empty at start?
	_db.safes.insert(Safe{_user, _token, {}});
	_db.tokens.insert(Token{_token, _user, Int{}});
}

void trust(DB& _db, Address const& _canSendTo, Address const& _user, uint32_t _limitPercentage)
{
	cout << "Trust change: " << _user << " send to " << _canSendTo << ": " << _limitPercentage << "%" << endl;
	_db.connections.erase(Connection{_canSendTo, _user, {}, {}});
	if (_limitPercentage == 0)
		return;

	Connection c{_canSendTo, _user, Int{}, _limitPercentage};
	updateLimit(_db, c);
	_db.connections.insert(move(c));
}

void transfer(
	DB& _db,
	Address const& _token,
	Address const& _from,
	Address const& _to,
	Int const& _value
)
{
	cout << "Transfer: " << _value << ": " << _from << " -> " << _to << " [" << _token << "]" << endl;
	// This is a generic ERC20 event and might be unrelated to the
	// Circles system.
	Token const* token = _db.tokenMaybe(_token);
	if (!token || _value == Int{})
	{
		if (!token)
			cout << "Token unknown." << endl;
		return;
	}

	const_cast<Safe&>(_db.safe(_to)).balances[_token] += _value;
	if (_from == Address{})
	{
		require(_to == token->safeAddress);
		const_cast<Token*>(token)->totalSupply += _value;
		for (Connection const& connection: _db.connections)
			if (connection.canSendToAddress == token->safeAddress)
				updateLimit(_db, const_cast<Connection&>(connection));
	}
	else
	{
		Safe& senderSafe = const_cast<Safe&>(_db.safe(_from));
		require(senderSafe.balances[_token] >= _value);
		senderSafe.balances[_token] -= _value;
	}
	updateLimit(_db, token->safeAddress, _from);
	updateLimit(_db, token->safeAddress, _to);

	// Which edge changes does this cause?
	//  - edges to a token's safe (balance change) - "extended connections"
	//  - the edge of the connection (user -> canSendTo via token) (balance change)
	//  - all limit changes
	//    (especially this causes many edges to change)
}
