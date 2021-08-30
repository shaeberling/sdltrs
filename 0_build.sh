#!/bin/sh

set -e

cd trs_xray
tsc
python ./gen_web_debugger_resources.py ../src
cd ..
make
