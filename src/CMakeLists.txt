#
# dogtricks executable
#

# Dependencies #################################################################

find_package(PkgConfig)
pkg_check_modules(tclap REQUIRED tclap)

find_package (Threads REQUIRED)

# Binary #######################################################################

add_executable(dogtricks
  main.cpp
  radio.cpp
  transport.cpp
)

target_link_libraries(dogtricks Threads::Threads)
