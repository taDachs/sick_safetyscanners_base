set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

set(Boost_DIR /home/max/Work/FZI/projects/sick_cross_compilation/boost_mingw/boost_1_78_0/stage/lib/cmake/Boost-1.78.0/)
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

set(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/x86_64-w64-mingw32-g++)
