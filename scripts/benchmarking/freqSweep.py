#!/usr/bin/python

import sys
import os

spath = os.path.dirname(os.path.realpath(__file__))
bindir = spath + "/../../bin/"

def run(cmd):
    print cmd
    os.system(cmd)

args = " ".join(sys.argv[1:])

clkRange = range(0, 250, 10)
for clk in clkRange:
    run("%s/cpphdl %s --clk %s --workdir freq%s" %
            (bindir, args, clk, clk))

