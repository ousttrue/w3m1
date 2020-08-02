set(TARGET_NAME charutil)
add_library(${TARGET_NAME}
    charutil.cpp
    myctype.cpp
    )
target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    )

add_executable("${TARGET_NAME}_test"
    main_test.cpp
    charutil_test.cpp
    myctype_test.cpp
    )
target_link_libraries("${TARGET_NAME}_test"
PRIVATE
    ${TARGET_NAME}
    catch2
    )
