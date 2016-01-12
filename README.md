LLPM
====

Low Level Physical Machine framework


Building LLPM
====
First install all the packages upon which LLPM  & LLVM rely. For ubuntu,
I think the following are required, though this list may be missing a few:
- build-essential
- libboost-all-dev
- libtinyxml2-dev
- flex
- bison
- groff
- graphviz
- scons
- cmake
- autoconf
- doxygen (recommended)
- gdb (recommended)

Next, LLPM relies on two submodules which are build out-of-band. First, fetch
and build them:
```bash
$ git submodule init
$ git submodule update
$ ./build_llvm.sh
$ ./build_verilator.sh
```

Finally, you should be able to build llpm:
```bash
$ scons
```

LLPM Status
====
LLPM is very early in its development. At any given time, it may or may not
work properly. I'm moving fast and breaking stuff! So, user beware.


Documentation
====
There is not yet any sort of high-level design document. There
is, however, _some_ Doxygen documentation in the code. You can build the
documentation with 'scons docs'. You'll find the HTML docs in
docs/html/index.html once it is done building. I have primarily aimed to
document the functionality of the core blocks, so it's best to start at the
llpm::Block class.

