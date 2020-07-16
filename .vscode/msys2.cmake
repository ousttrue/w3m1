# the name of the target operating system
set(CMAKE_SYSTEM_NAME Windows)

# which compilers to use for C and C++
# include(CMakeForceCompiler)
# cmake_force_c_compiler(x86_64-pc-msys-gcc GNU)
# cmake_force_cxx_compiler(x86_64-pc-msys-g++ GNU)
set(
  CMAKE_C_COMPILER
  x86_64-pc-msys-gcc
  CACHE
  STRING
  "C compiler"
  FORCE
)

set(
  CMAKE_CXX_COMPILER
  x86_64-pc-msys-g++
  CACHE
  STRING
  "C++ compiler"
  FORCE
)

set(
  CMAKE_AR
  x86_64-pc-msys-gcc-ar
  CACHE
  STRING
  "archiver"
  FORCE
)

# here is the target environment located
set(CMAKE_FIND_ROOT_PATH
  C:/msys64/usr/bin
)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_FIND_LIBRARY_PREFIXES "lib" "")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll" ".dll.a" ".lib" ".a")
