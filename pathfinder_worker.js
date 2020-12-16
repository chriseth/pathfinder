console.log("Worker started.");
importScripts(['./emscripten_build/pathfinder.js']);
console.log("Emscripten binary loaded.");

fetch('db.dat').then(async response => {
    let db = await response.arrayBuffer();
    let length = db.byteLength;
    var ptr = Module._malloc(length);
    var heapBytes = new Uint8Array(Module.HEAPU8.buffer, ptr, length);
    heapBytes.set(new Uint8Array(db));
    let latestBlockNumber = Module._loadDB(heapBytes.byteOffset, length);
    Module._free(ptr);
    postMessage({callID: "loaded", result: latestBlockNumber});
});

let functions = {
    signup: Module.cwrap("signup", null, ['string', 'string']),
    trust: Module.cwrap("trust", null, ['string', 'string', 'number']),
    transfer: Module.cwrap("transfer", null, ['string', 'string', 'string', 'string']),
    edgeCount: Module.cwrap("edgeCount", 'number', []),
    delayEdgeUpdates: Module.cwrap("delayEdgeUpdates", null, []),
    performEdgeUpdates: Module.cwrap("performEdgeUpdates", null, []),
    adjacencies: Module.cwrap("adjacencies", 'string', ['string']),
    flow: Module.cwrap("flow", 'string', ['string'])
};

addEventListener('message', (message) => {
    let result = functions[message.data.function](...message.data.arguments);
    postMessage({callID: message.data.callID, result: result});
})
