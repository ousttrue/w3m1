set(TARGET_NAME test)
add_executable(${TARGET_NAME}
    some_test.cpp
    )
target_link_libraries(${TARGET_NAME}
PRIVATE
    catch2
    )
