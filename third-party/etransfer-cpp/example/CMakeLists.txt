find_package(Threads REQUIRED)

add_executable(example example_main.cpp)

target_link_libraries(example
        PUBLIC tool Threads::Threads
)

target_link_options(example
        PUBLIC -static-libstdc++
)