#!/usr/bin/python3

import requests
import json

query="""{
    id
    outgoing { limit canSendToAddress userAddress }
    incoming { limit canSendToAddress userAddress }
    balances { amount token { id owner { id } } }
}""".replace('\n', ' ')

API='https://graph.circles.garden/subgraphs/name/CirclesUBI/circles-subgraph'

skip = 0
count = 200
safes = []
while True:
    print(skip)
    result = requests.post(API, data='{"query":"{ safes( orderBy: id, first: %d, skip: %d ) %s }"}' % (count, skip, query)).json()
    if 'data' not in result or 'safes' not in result['data'] or len(result['data']['safes']) == 0:
        break
    safes += result['data']['safes']
    skip += count

json.dump(safes, open('safes.json', 'w'))
