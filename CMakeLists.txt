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
    HAVE_CONFIG_H
    AUXBIN_DIR=\"/usr/local/w3m/libexec\"
    CGIBIN_DIR=\"/usr/local/w3m/cgi-bin\"
    HELP_DIR=\"/usr/local/share/w3m\"
    ETC_DIR=\"/usr/local/etc\"
    CONF_DIR=\"/usr/local/etc/w3m\"
    RC_DIR=\"~/.w3m\"
    LOCALEDIR=\"/usr/local/share/locale\"
    )
