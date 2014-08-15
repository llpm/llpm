import os
import os.path
import sys

if not os.path.isfile("./bin/llvm/bin/llvm-config"):
    print "Must compile llvm with ./build_llvm.sh first!"
    sys.exit(1)

LibPaths = []

llvm_libraries = map(lambda x: x[2:],
    os.popen(" ".join("""
./bin/llvm/bin/llvm-config --libs analysis bitreader
bitwriter codegen core engine jit linker mcjit scalaropts
support irreader
""".splitlines())).read().split())

env = Environment(CXX="clang++",
                  LD="clang++",
                  CC="clang",
                  CPPPATH=['./lib', './bin/llvm/include/'],
                  CXXFLAGS="""-O1 -mfpmath=sse -msse4 -march=native
                            -Wall -g -std=c++11 -stdlib=libc++
                            -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS
                            -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS """.split(),
                  LDFLAGS="""-stdlib=libc++""".split(),
                  LIBS="""rt c dl z tinfo""".split() + llvm_libraries,
                  LIBPATH=LibPaths + ["bin/llvm/lib/"],
                  LINKFLAGS=['-pthread', '-stdlib=libc++'] + map(lambda x: "-Wl,-rpath=%s" % x, LibPaths))

llpm = env.Library('bin/llpm', Glob("./lib/*.cpp") +
                               Glob("./lib/*/*.cpp") +
                               Glob("./lib/*/*/*.cpp"))
env.Prepend(LIBS=[llpm])

for d in Glob("./tools/*"):
    d = str(d).split("/")[-1]
    driver = env.Program("bin/" + d, Glob("./tools/%s/*.cpp" % d))
    Default(driver)


