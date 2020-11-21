#pragma once

#include "types.h"


void updateLimit(DB const& _db, Connection& _connection);

void signup(DB& _db, Address const& _user, Address const& _token);
void trust(DB& _db, Address const& _canSendTo, Address const& _user, uint64_t _limitPercentage);
void transfer(DB& _db, Address const& _from, Address const& _to, Int const& _value);
