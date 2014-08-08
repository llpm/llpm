LibPaths = []

llvm_libraries = map(lambda x: x[2:],
    """-lLLVMTransformUtils -lLLVMipa -lLLVMAnalysis -lLLVMTarget
    -lLLVMMC -lLLVMOption -lLLVMIRReader -lLLVMAsmParser
    -lLLVMDebugInfo -lLLVMObject -lLLVMBitWriter
    -lLLVMBitReader -lLLVMCore -lLLVMSupport
    """.split())

env = Environment(CXX="clang++",
                  LD="clang++",
                  CC="clang",
                  CPPPATH=['./bin/llvm/include/'],
                  CXXFLAGS="""-O3 -mfpmath=sse -msse4 -march=native
                            -Wall -g -std=c++11 -stdlib=libc++
                            -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS
                            -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS """.split(),
                  LDFLAGS="""-stdlib=libc++""".split(),
                  LIBS="""c dl""".split() + llvm_libraries,
                  LIBPATH=LibPaths + ["bin/llvm/lib/"],
                  LINKFLAGS=['-pthread'] + map(lambda x: "-Wl,-rpath=%s" % x, LibPaths))

llpm = env.Library('bin/llpm', Glob("./lib/*.cpp") +
                           Glob("./lib/*/*.cpp"))
env.Append(LIBS=[llpm])

for d in Glob("./tools/*"):
    d = str(d).split("/")[-1]
    driver = env.Program("bin/" + d, Glob("./tools/%s/*.cpp" % d))
    Default(driver)


