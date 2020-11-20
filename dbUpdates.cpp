#include "dbUpdates.h"

#include "exceptions.h"


using namespace std;

void checkLimit(DB const& _db, Connection& _connection)
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
			if (_connection.limitPercentage == 50)
				max = max.half();
			else if (_connection.limitPercentage == 0)
				max = Int(0); // TODO remove connection altogether?
			else
				max = (max * _connection.limitPercentage) / 100;
			limit = max < receiverBalance ? Int(0) : max - receiverBalance;
		}
	}

	_connection.limit = limit;
}
