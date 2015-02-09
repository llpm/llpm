#!/bin/bash

../../../bin/cpphdl sha256.bc SHA256 --workdir lib1000 \
    --backend ipxact --wedge axi --clk 1000.0
