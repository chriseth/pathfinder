let fs = require('fs').promises;
let ethers = require('ethers')
let pathfinder_ = require('./emscripten_build/pathfinder.js')
let pathfinder = {
    loadDB: pathfinder_.cwrap("loadDB", 'number', ['array', 'number']),
    signup: pathfinder_.cwrap("signup", null, ['string', 'string']),
    trust: pathfinder_.cwrap("trust", null, ['string', 'string', 'number']),
    transfer: pathfinder_.cwrap("transfer", null, ['string', 'string', 'string', 'string']),
    edgeCount: pathfinder_.cwrap("edgeCount", 'number', []),
    delayEdgeUpdates: pathfinder_.cwrap("delayEdgeUpdates", null, []),
    performEdgeUpdates: pathfinder_.cwrap("performEdgeUpdates", null, []),
    adjacencies: pathfinder_.cwrap("adjacencies", 'string', ['string']),
    flow: pathfinder_.cwrap("flow", 'string', ['string'])
};


const CirclesAPI = 'https://api.circles.garden/api/';
let GraphAPI = 'https://graph.circles.garden/subgraphs/name/CirclesUBI/circles-subgraph';
const provider = new ethers.providers.JsonRpcProvider('https://xdai-archive.blockscout.com');

const hubAbi = [
    "function name() view returns (string)",
    "function userToToken(address) view returns (address)",
    "function symbol() view returns (string)",
    "function balanceOf(address) view returns (uint)",
    "function transfer(address to, uint amount)",
    "function checkSendLimit(address tokenOwner, address src, address dest) view returns (uint256)",
    "function limits(address,address) view returns (uint256)",
    "event Signup(address indexed user, address token)",
    "event OrganizationSignup(address indexed organization)",
    "event Trust(address indexed canSendTo, address indexed user, uint256 limit)",
    "event HubTransfer(address indexed from, address indexed to, uint256 amount)"
];
const hubAddress = "0x29b9a7fBb8995b2423a71cC17cf9810798F6C543";
const hubContract = new ethers.Contract(hubAddress, hubAbi, provider);

let latestBlockNumber = 0;

let uintToAddress = function(value) {
    if (value.length != 66 || value.substr(0, 26) != "0x000000000000000000000000")
        throw("invalid address: " + value);
    value = "0x" + value.substr(26)
    return ethers.utils.getAddress(value);
};


let loadDB = async function() {
    let db = await fs.readFile('db.dat');
    let length = db.byteLength;
    var ptr = pathfinder_._malloc(length);
    var heapBytes = new Uint8Array(pathfinder_.HEAPU8.buffer, ptr, length);
    heapBytes.set(new Uint8Array(db));
    let latestBlockNumber = pathfinder_._loadDB(heapBytes.byteOffset, length);
    pathfinder_._free(ptr);
    console.log("loaded, latest block: " + latestBlockNumber);
    return latestBlockNumber;
};

let updateSinceBlock = async function(lastKnownBlock) {
    await pathfinder.delayEdgeUpdates();
    // Process all hub events first and then the transfer,
    // so we can properly filter out unrelated transfers.
    let signupID = ethers.utils.id("Signup(address,address)");
    let trustID = ethers.utils.id("Trust(address,address,uint256)");
    console.log("Retrieving logs...")
    let res = await provider.getLogs({
        fromBlock: lastKnownBlock,
        address: hubContract.address,
        topics: [[signupID, trustID]]
    });
    console.log("Number of events from hub to process: " + res.length)
    let successNr = 0;
    for (log of res) {
        try {
            if (log.topics[0] == signupID) {
                await pathfinder.signup(uintToAddress(log.topics[1]), uintToAddress(log.data));
            } else if (log.topics[0] == trustID) {
                await pathfinder.trust(uintToAddress(log.topics[1]), uintToAddress(log.topics[2]), log.data - 0)
            }
            //hubEventContainer.innerHTML = log.blockNumber;
            successNr += 1;
        } catch {
            //console.log(`Error processing log: ${log.blockNumber} - ${log.transactionIndex}`)
            //console.log(log)
        }
    }
    console.log(`Hub events processed, ${successNr} out of ${res.length} successfully.`);
    res = await provider.getLogs({
        fromBlock: lastKnownBlock,
        topics: [ethers.utils.id("Transfer(address,address,uint256)")]
    });
    console.log("Number of transfers to process: " + res.length);
    successNr = 0;
    for (log of res) {
        try {
            let value = log.data;
            let token = log.address;
            let from = uintToAddress(log.topics[1]);
            let to = uintToAddress(log.topics[2]);
            await pathfinder.transfer(token, from, to, value)
            //transferEventContainer.innerHTML = log.blockNumber;
            successNr += 1;
        } catch {
            //console.log(`Error processing log: ${log.blockNumber} - ${log.transactionIndex}`)
            //console.log(log)
        }
    }
    console.log(`Transfers processed, ${successNr} out of ${res.length} successfully.`);
    await pathfinder.performEdgeUpdates();
    console.log("Edge count: " + await pathfinder.edgeCount());
};


let setupEventListener = async function() {
    console.log("Block: " + await provider.getBlockNumber());
    hubContract.on("Trust", async (sendTo, user, limitPercentage) => {
        console.log(`trust ${user} -> ${sendTo} (${limitPercentage}) `);
        // TODO check that limitPercentage is actually a number.
        await pathfinder.trust(sendTo, user, limitPercentage - 0)
        // TODO block number?
    });
    hubContract.on("Signup", async (user, token) => {
        console.log(`signup ${user} ${token}`);
        await pathfinder.signup(user, token);
        // TODO block number?
    });
    provider.on({ topics: [ ethers.utils.id("Transfer(address,address,uint256)") ] }, async (log) => {
        let value = log.data;
        let token = log.address;
        let from = uintToAddress(log.topics[1]);
        let to = uintToAddress(log.topics[2]);
        console.log(`Transfer ${from} -> ${to}: ${value} ${token}`);
        await pathfinder.transfer(token, from, to, value)
        latestBlockNumber = log.blockNumber;
    });
};

let startup = async function() {
    latestBlockNumber = await loadDB();
    await updateSinceBlock(latestBlockNumber);
    await setupEventListener();
};

module.exports = {
    startup: startup,
    latestBlock: () => latestBlockNumber,
    edgeCount: pathfinder.edgeCount,
    flow: pathfinder.flow,
    adjacencies: pathfinder.adjacencies
}
