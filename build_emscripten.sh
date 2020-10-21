set -e

if test -z "$1"; then
    BUILD_DIR="emscripten_build"
else
    BUILD_DIR="$1"
fi

docker run -v $(pwd):/root/project -w /root/project \
    solbuildpackpusher/solidity-buildpack-deps@sha256:23dad3b34deae8107c8551804ef299f6a89c23ed506e8118fac151e2bdc9018c\
    ./build_emscripten_worker.sh $BUILD_DIR
