#!/usr/bin/python3

import requests
import json

blockNumber =requests.get('https://blockscout.com/poa/xdai/api?module=block&action=eth_block_number').json()['result']

query="""{
    id
    outgoing { limit limitPercentage canSendToAddress userAddress }
    incoming { limit limitPercentage canSendToAddress userAddress }
    balances { amount token { id owner { id } } }
}""".replace('\n', ' ')

API='https://graph.circles.garden/subgraphs/name/CirclesUBI/circles-subgraph'

lastID = 0
count = 1000
safes = []
while True:
    print("ID: %s" % lastID)
    result = requests.post(API, data='{"query":"{ safes( orderBy: id, first: %d, where: { id_gt: \\"%s\\" } ) %s }"}' % (count, lastID, query)).json()
    if 'data' not in result or 'safes' not in result['data'] or len(result['data']['safes']) == 0:
        break
    print("Got %d safes..." % len(result['data']['safes']))
    safes += result['data']['safes']
    lastID = result['data']['safes'][-1]['id']

json.dump({'blockNumber': blockNumber, 'safes': safes}, open('safes.json', 'w'))
