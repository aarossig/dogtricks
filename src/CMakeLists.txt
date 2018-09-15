#
# dogtricks executable
#

# Dependencies #################################################################

find_package(PkgConfig)
pkg_check_modules(tclap REQUIRED tclap)

# Binary #######################################################################

add_executable(dogtricks
  main.cpp
)
