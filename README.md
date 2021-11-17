## Pathfinder

Pathfinder is a collection of tools related to
computing transitive transfers in the
[CirclesUBI](https://joincircles.net) trust graph.

Since the trust graph is currently only some megabytes big,
the idea was to use a custom C++ implementation of the
flow computation in memory to avoid the overhead of graph databases.

### The Core Executable

You can build the core executable either into a native binary
using

```bash
mkdir build
cd build
cmake ..
make
```

or into a webassembly binary using

```bash
./build_emscripten.sh
```

The core binary - let us call it ``pathfinder`` - has the following modes:

#### Use as Library

It can be used as a library, especially when compiled via emscripten.
Then it provides the folloing C API:

```C
size_t loadDB(char const* _data, size_t _length);
size_t edgeCount();
void delayEdgeUpdates();
void performEdgeUpdates();
void signup(char const* _user, char const* _token);
void organizationSignup(char const* _organization);
void trust(char const* _canSendTo, char const* _user, int _limitPercentage);
void transfer(char const* _token, char const* _from, char const* _to, char const* _value);
char const* adjacencies(char const* _user);
char const* flow(char const* _input);
```

TODO: Document properly

#### Use as Program

You can run pathfinder from the shell.
Here it provides the folloing options:

```
Options: 
  --json                                     JSON mode via stdin/stdout.
  [--flow] <from> <to> <value> <db.dat>      Compute max flow up to <value> and output transfer steps in json.
  --importDB <safes.json> <db.dat>           Import safes with trust edges and generate transfer limit graph.
  --dbToEdges <db.dat> <edges.dat>           Import safes with trust edges and generate transfer limit graph.
```

The file `safes.json` is an export from TheGraph and can be obtained by running `download_safes.py`.

### The Website

The utilities can be integrated into a website that has two flavours:
Either all computations are performed in the browser, or they are performed on the server.

#### Browser-Based

The standalone browser-based website can be accessed through the `index.html` file.
In order for it to run, you need the file `db.dat` and the binary obtained
from running `build_emscripten.sh`. The `db.dat` can be obtained by running

```bash
./download_safes.py
./build/pathfinder --importDB safes.json db.dat
```

#### Server-Based

The server-based version is launched by running
`node ./index.js <port>`. For it to work, you will need the
natively-built `pathfinder`, since it will spawn
it internally in "JSON mode".

If you provide a privileged port, the process will
try to listen also on port 443 by reading a certain
SSL certificate (please see the source code of `webservice.js`).

The server-based version will provide the same user-interface
as the browser-based, but in addition, provides the following API,
which it tries to keep CORS-compatible as much as possible:

- GET `/`
  
  Return the user-interface as HTML.

- GET `/status`

  Return ``{block: <latest block>, edges: <edge count>}` where "latest block" is the latest
  block in the xdai chain that contained an event relevant to the CirclesUBI system
  and "edge count" is the number of edges in the trust graph.

- GET `/health/<seconds>`

  Returns the number of seconds since the last update to the internal database.
  If it has been longer ago than the provided parameter "seconds", results
  in a 500 response, otherwise returns 200.

- POST `/flow`

  Expects json data of the form `{from: <from>, to: <to>, value: <value>}`, with `value`
  being optional. Computes a sequence of transfers of Circles tokens and the maximum
  value (flow) that can be transferred. Returns json formatted as follows:

```json
{
    "flow":"1329220370370358587275",
    "transfers": [
        {
            "from":"0x8DC7e86fF693e9032A0F41711b5581a04b26Be2E",
            "to":"0x55E0fF8d8eF8194aBF0F6378076193B4554376C6",
            "token":"0x246a0296a9Be05BFC3389d542eF3678FdAE6874e",
            "tokenOwner":"0x8DC7e86fF693e9032A0F41711b5581a04b26Be2E",
            "value":"397185648148151053100"
        }, {
            "from":"0x8DC7e86fF693e9032A0F41711b5581a04b26Be2E",
            "to":"0x57928Fb15ffB7303b65EDC326dc4dc38150008e1",
            "token":"0x246a0296a9Be05BFC3389d542eF3678FdAE6874e",
            "tokenOwner":"0x8DC7e86fF693e9032A0F41711b5581a04b26Be2E",
            "value":"1999999999999822849"
        },
        //   ...
    ]
}
```

- GET `/adjacencies/<user>`

  Returns the list of adjacent nodes to `<user>` in the trust graph together with trust percentages. Example:

```json
[
    {"percentage":50,"trusts":"0x15A322661bea3106AD018b4B629398d2cCD52D51","user":"0x68D300764059bAbD26B75DA1141f05a9ffb6Fbc0"},
    {"percentage":50,"trusts":"0x19D6290ED7Ecab6DCddBfCe16dA54994Bbb7D922","user":"0x68D300764059bAbD26B75DA1141f05a9ffb6Fbc0"},
    {"percentage":50,"trusts":"0x283952f8f94cC41a5E7c27A8080E0681dA66Ab0B","user":"0x68D300764059bAbD26B75DA1141f05a9ffb6Fbc0"},
    // ...
]
```
