include(gtest)

find_package(Threads REQUIRED)

file(GLOB srcs CONFIGURE_DEPENDS ./*.cpp)

add_executable(unit_test ${srcs})

target_link_libraries(unit_test
        PUBLIC gtest tool Threads::Threads
)

target_link_options(unit_test
        PUBLIC -static-libstdc++
)