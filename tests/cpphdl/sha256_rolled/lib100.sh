#!/bin/bash

../../../bin/cpphdl sha256.bc SHA256 --workdir lib100 \
    --backend ipxact --wedge axi --clk 100.0
