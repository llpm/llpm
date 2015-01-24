import os
import os.path
import sys

if not os.path.isfile("./bin/llvm/bin/llvm-config"):
    print "Must compile llvm with ./build_llvm.sh first!"
    sys.exit(1)

LibPaths = [".", "llvm/lib"]

CxxLdFlags = """
-g -pthread -fno-omit-frame-pointer
""".split()

AddOption('--cxx', action='store', type='string', dest='cxx', nargs=1, metavar='COMPILER',
		   help='Use argument as the C++ compiler.')

env = Environment(
    CPPPATH=['./lib', './bin/llvm/include/'],
    CXXFLAGS="""-O0 -mfpmath=sse -msse4 -march=native
            -Wall -Wextra -std=c++1y
            -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS
            -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS """.split()
            + CxxLdFlags,
    LIBS="""boost_program_options LLVM-3.5""".split(),
    LIBPATH=map(lambda x: "bin/" + x, LibPaths),
    LINKFLAGS=[]
              + CxxLdFlags
              + map(lambda x: "-Wl,-rpath=\$$ORIGIN/%s" % x,
              LibPaths),
    tools = ["default", "doxygen"],
    toolpath = '.'
)

if GetOption('cxx') != None:
   env.Replace(CXX = GetOption('cxx'))

libenv = env.Clone()
llpm = libenv.SharedLibrary('bin/llpm', Glob("./lib/*.cpp") +
                               Glob("./lib/*/*.cpp") +
                               Glob("./lib/*/*/*.cpp"))
env.Prepend(LIBS=['llpm'])

for d in Glob("./tools/*"):
    d = str(d).split("/")[-1]
    driver = env.Program("bin/" + d, Glob("./tools/%s/*.cpp" % d))
    Default(driver)

supportFiles = Glob("./support/*") + \
               Glob("./support/*/*") + \
               Glob("./support/*/*/*")

for f in supportFiles:
    if f.isfile():
        i = env.Install("bin/" + str(f.get_dir()), f)
        Default(i)

docs = env.Doxygen("Doxyfile")


