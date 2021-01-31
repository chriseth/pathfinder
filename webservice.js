let http = require("http");
let url = require("url");
let path = require("path");
const pathfinderd = require("./pathfinderd");

let port = 80;

let respond = function(response, data) {
    response.writeHead(200, {
        "Content-Type": "application/json",
        "Access-Control-Allow-Origin": "*",
        "Access-Control-Allow-Headers": "content-type",
        "Access-Control-Allow-Origin": "*",
        "Access-Control-Allow-Methods": "GET, POST, HEAD, OPTIONS",
    });
    response.end(JSON.stringify(data));
};

let readBody = (request) => {
    return new Promise((resolve, reject) => {
        var body = '';    
        request.on('data', (data) => {
            body += data;
            if (body.length > 5000) {
                request.connection.destroy();
                reject("Too much data.");
            }
        });
        request.on('end', function () {
            resolve(body);
        });
    });
};

let handler = async function(request, response) {
    if (request.method.toLowerCase() == 'options') {
        respond(response, '');
        return;
    }

    var uri = url.parse(request.url).pathname;
    try {
        if (uri == '/status') {
            respond(response, {block: pathfinderd.latestBlock(), edges: pathfinderd.edgeCount()})
        } else if (uri == '/flow') {
            var body = JSON.parse(await readBody(request));
            let from = body['from'];
            let to = body['to'];
            let value = body['value'] || "115792089237316195423570985008687907853269984665640564039457584007913129639935";
            let data = JSON.parse(pathfinderd.flow(JSON.stringify({"from": from, "to": to, "value": value})));
            respond(response, data);
        } else if (uri.startsWith('/adjacencies/')) {
            var address = uri.substring('/adjacencies/'.length);
            respond(response, JSON.parse(pathfinderd.adjacencies(address)));
        } else {
            response.writeHead(404, {"Content-Type": "text/plain"});
            response.write("404 Not Found\n");
            response.end();
        }
    } catch (e) {
        request.connection.destroy();
        console.log("Error processing request at " + uri);
        console.log(e, e.stack);
    }
};

let initialize = function(port) {
    http.createServer(handler).listen(port);
};

module.exports = {
    initialize: initialize
}