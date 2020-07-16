project(w3m)
cmake_minimum_required(VERSION 3.0.0)

add_executable(w3m
    main.c
)
target_include_directories(w3m
PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/libwc
)
target_compile_definitions(w3m
PRIVATE
    "RC_DIR=\"~/.w3m\""
)
