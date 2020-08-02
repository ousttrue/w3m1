set(TARGET_NAME charutil)
add_library(${TARGET_NAME}
    charutil.cpp
    )

add_executable(${TARGET_NAME}_test
    charutil_test.cpp
    )
target_link_libraries(${TARGET_NAME}_test
PRIVATE
    ${TARGET_NAME}
    catch2
    )
