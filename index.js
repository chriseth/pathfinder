let pathfinderd = require('./pathfinderd');
let webservice = require('./webservice');
let process = require('process');

(async function() {
    webservice.initialize(parseInt(process.argv[2], 10) || 80);
    await pathfinderd.startup();
})();
