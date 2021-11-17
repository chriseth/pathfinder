#!/bin/bash

set -e

(
cd "$(dirname "$0")"

for testfile in *
do
    if [[ "$testfile" =~ .*_exp\.json || ! "$testfile" =~ .*\.json ]]
    then
        continue
    fi
    set -e
    ../build/pathfinder --json < "$testfile" > /tmp/output.json
    expfile="$testfile"_exp.json
    set +e
    if [ "$1" = "--update" ]
    then
        cp /tmp/output.json "$expfile"
    elif ! diff -q /tmp/output.json "$expfile"
    then
        diff /tmp/output.json "$expfile"
        exit 1
    fi
done

)