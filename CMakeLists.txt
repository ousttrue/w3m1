project(w3m)
cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 20)
set(W3M_PREFIX $ENV{HOME}/local)
   
add_library(catch2 INTERFACE)
target_include_directories(catch2
INTERFACE
    _external/catch2
)

add_library(tcb_span INTERFACE)
target_include_directories(tcb_span
INTERFACE
    _external/tcb_span/include
)

add_library(plog INTERFACE)
target_include_directories(plog
INTERFACE
    _external/plog/include
)

add_library(asio INTERFACE)
target_include_directories(asio
INTERFACE
    _external/asio/asio/include
)
target_link_libraries(asio
INTERFACE
    pthread
)

subdirs(
    charutil
    libwc
)

#
# w3mbookmark
#
add_executable(w3mbookmark
    src/w3mbookmark.cpp
    src/indep.cpp
    src/textlist.cpp
    src/html/parsetag.cpp
    src/hash.cpp
)
target_include_directories(w3mbookmark
PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)  
target_compile_definitions(w3mbookmark
PUBLIC
    DUMMY
    HAVE_CONFIG_H
    AUXBIN_DIR=\"${W3M_PREFIX}/libexec/w3m\"
    CGIBIN_DIR=\"${W3M_PREFIX}/libexec/w3m/cgi-bin\"
    HELP_DIR=\"${W3M_PREFIX}/share/w3m\"
    ETC_DIR=\"${W3M_PREFIX}/etc\"
    CONF_DIR=\"${W3M_PREFIX}/etc/w3m\"
    RC_DIR=\"~/.w3m\"
    LOCALEDIR=\"${W3M_PREFIX}/share/locale\"
    USE_UNICODE
)
target_link_libraries(w3mbookmark
PRIVATE
    wc
    tcb_span
)

#
# w3mhelperpanel
#
add_executable(w3mhelperpanel
    src/w3mhelperpanel.cpp
    src/indep.cpp
    src/textlist.cpp
    src/html/parsetag.cpp
)
target_include_directories(w3mhelperpanel
PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)  
target_compile_definitions(w3mhelperpanel
PUBLIC
    DUMMY
    HAVE_CONFIG_H
    AUXBIN_DIR=\"${W3M_PREFIX}/libexec/w3m\"
    CGIBIN_DIR=\"${W3M_PREFIX}/libexec/w3m/cgi-bin\"
    HELP_DIR=\"${W3M_PREFIX}/share/w3m\"
    ETC_DIR=\"${W3M_PREFIX}/etc\"
    CONF_DIR=\"${W3M_PREFIX}/etc/w3m\"
    RC_DIR=\"~/.w3m\"
    LOCALEDIR=\"${W3M_PREFIX}/share/locale\"
    USE_UNICODE
)
target_link_libraries(w3mhelperpanel
PRIVATE
    wc
    tcb_span
)

#
# w3m
#
add_executable(w3m
    src/main.cpp
    src/w3m.cpp
    src/cli.cpp
    #
    src/command_dispatcher.cpp
    src/commands.cpp
    src/public.cpp
    src/urimethod.cpp
    src/frontend/display.cpp
    src/frontend/link.cpp
    src/frontend/lineinput.cpp
    src/frontend/propstring.cpp
    src/frontend/line.cpp
    src/frontend/buffer.cpp
    src/frontend/tab.cpp
    src/frontend/tabbar.cpp
    src/frontend/menu.cpp
    src/frontend/terminal.cpp
    src/frontend/screen.cpp
    src/frontend/mouse.cpp
    src/frontend/keybind.cpp
    src/frontend/event.cpp
    src/frontend/anchor.cpp
    src/file.cpp
    src/stream/network.cpp
    src/stream/input_stream.cpp
    src/stream/local_cgi.cpp
    src/stream/url.cpp
    src/stream/open_socket.cpp
    src/stream/compression.cpp
    src/stream/auth.cpp
    src/stream/http.cpp
    src/stream/cookie.cpp
    src/stream/ssl_socket.cpp
    src/mime/mimeencoding.cpp
    src/mime/mimetypes.cpp
    src/mime/mailcap.cpp
    src/html/tagstack.cpp
    src/html/readbuffer.cpp
    src/html/html_context.cpp
    src/html/table.cpp
    src/html/form.cpp
    src/html/parsetag.cpp
    src/html/html.cpp
    src/html/html_tag.cpp
    src/html/maparea.cpp
    src/html/image.cpp
    src/html/html_to_buffer.cpp
    src/html/formdata.cpp
    src/loader.cpp
    src/mytime.cpp
    src/indep.cpp
    src/rc.cpp
    src/textlist.cpp
    src/hash.cpp
    src/symbol.cpp
    src/history.cpp
    src/download_list.cpp
)
target_include_directories(w3m
PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)
target_compile_definitions(w3m
PUBLIC
    HAVE_CONFIG_H
    AUXBIN_DIR=\"${W3M_PREFIX}/libexec/w3m\"
    CGIBIN_DIR=\"${W3M_PREFIX}/libexec/w3m/cgi-bin\"
    HELP_DIR=\"${W3M_PREFIX}/share/w3m\"
    ETC_DIR=\"${W3M_PREFIX}/etc\"
    CONF_DIR=\"${W3M_PREFIX}/etc/w3m\"
    RC_DIR=\"~/.w3m\"
    LOCALEDIR=\"${W3M_PREFIX}/share/locale\"
    USE_UNICODE
)
target_compile_options(w3m
PRIVATE
    # -Werror=implicit-function-declaration
    # -Werror=int-conversion
    -Werror=conversion-null
)
target_link_libraries(w3m
PRIVATE
    z
    tcb_span
    m
    ncurses
    ssl
    crypto
    plog
    asio
    #
    wc
)
