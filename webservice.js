let http = require("http");
let https = require("https");
let url = require("url");
let fs = require("fs");
let path = require("path");
const pathfinderd = require("./pathfinderd");

let port = 80;

let respond = function(response, data) {
    response.writeHead(200, {
        "Content-Type": "application/json",
        "Access-Control-Allow-Origin": "*"
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
        response.writeHead(204, {
            "Access-Control-Allow-Origin": "*",
            "Access-Control-Allow-Headers": "*",
            "Access-Control-Allow-Methods": "GET,HEAD,POST",
            "Content-Length": "0"
        });
        response.end();
        return;
    }

    var uri = url.parse(request.url).pathname;
    try {
        if (uri == '/') {
            response.writeHead(200, {
                "Content-Type": "text/html",
                "Access-Control-Allow-Origin": "*"
            });
            response.end(fs.readFileSync('index.html',{encoding: 'utf-8'}).replace('setupWorker();', 'setupServer();'));
        } else if (uri == '/status') {
            respond(response, {block: await pathfinderd.latestBlock(), edges: await pathfinderd.edgeCount()})
        } else if (uri.startsWith('/health/')) {
            let age = ((+new Date()) - pathfinderd.latestUpdate()) / 1000;
            if (age > uri.substring('/health/'.length) - 0)
                response.writeHead(500);
            else
                response.writeHead(200);
            response.end('Last update ' + age + ' seconds ago.');
        } else if (uri == '/flow') {
            var body = JSON.parse(await readBody(request));
            let from = body['from'];
            let to = body['to'];
            let value = body['value'] || "115792089237316195423570985008687907853269984665640564039457584007913129639935";
            respond(response, await pathfinderd.flow(from, to, value));
        } else if (uri.startsWith('/adjacencies/')) {
            var address = uri.substring('/adjacencies/'.length);
            respond(response, await pathfinderd.adjacencies(address));
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
    if (port < 1024) {
        https.createServer({
            cert: fs.readFileSync('cert.pem'),
            key: fs.readFileSync('key.pem')
        }, handler).listen(443);
    }
};

module.exports = {
    initialize: initialize
}
