#!/bin/bash

../../../bin/cpphdl sha256.bc SHA256 --workdir lib \
    --backend ipxact --wedge axi
