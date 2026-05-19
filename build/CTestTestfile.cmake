# CMake generated Testfile for 
# Source directory: /home/lin/chat_server
# Build directory: /home/lin/chat_server/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(chat_server_tests "/home/lin/chat_server/build/chat_server_tests")
set_tests_properties(chat_server_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/lin/chat_server/CMakeLists.txt;47;add_test;/home/lin/chat_server/CMakeLists.txt;0;")
add_test(chat_protocol_fuzz "/home/lin/chat_server/build/chat_protocol_fuzz" "--corpus" "/home/lin/chat_server/tests/corpus/protocol" "--iterations" "5000")
set_tests_properties(chat_protocol_fuzz PROPERTIES  _BACKTRACE_TRIPLES "/home/lin/chat_server/CMakeLists.txt;50;add_test;/home/lin/chat_server/CMakeLists.txt;0;")
