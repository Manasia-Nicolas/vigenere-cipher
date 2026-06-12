# CMake generated Testfile for 
# Source directory: /Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/vigenere
# Build directory: /Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/build/vigenere
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[vigenere_tests]=] "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/build/vigenere/vigenere_tests")
set_tests_properties([=[vigenere_tests]=] PROPERTIES  WORKING_DIRECTORY "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid" _BACKTRACE_TRIPLES "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/vigenere/CMakeLists.txt;32;add_test;/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/vigenere/CMakeLists.txt;0;")
add_test([=[vigenere_cli_short]=] "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/build/vigenere/vigenere" "en" "vigenere/data/short-encrypted.txt")
set_tests_properties([=[vigenere_cli_short]=] PROPERTIES  PASS_REGULAR_EXPRESSION "1.LOCK" WORKING_DIRECTORY "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid" _BACKTRACE_TRIPLES "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/vigenere/CMakeLists.txt;37;add_test;/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/vigenere/CMakeLists.txt;0;")
add_test([=[vigenere_cli_default]=] "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/build/vigenere/vigenere")
set_tests_properties([=[vigenere_cli_default]=] PROPERTIES  PASS_REGULAR_EXPRESSION "1.FAIRY" WORKING_DIRECTORY "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid" _BACKTRACE_TRIPLES "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/vigenere/CMakeLists.txt;43;add_test;/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/vigenere/CMakeLists.txt;0;")
add_test([=[vigenere_cli_german]=] "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/build/vigenere/vigenere" "de" "vigenere/data/german-encrypted.txt")
set_tests_properties([=[vigenere_cli_german]=] PROPERTIES  PASS_REGULAR_EXPRESSION "1.MÄRCHEN" WORKING_DIRECTORY "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid" _BACKTRACE_TRIPLES "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/vigenere/CMakeLists.txt;48;add_test;/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/vigenere/CMakeLists.txt;0;")
add_test([=[vigenere_cli_spaceless]=] "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/build/vigenere/vigenere" "en" "vigenere/data/spaceless-encrypted.txt")
set_tests_properties([=[vigenere_cli_spaceless]=] PROPERTIES  PASS_REGULAR_EXPRESSION "3.BY THE SIDE" WORKING_DIRECTORY "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid" _BACKTRACE_TRIPLES "/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/vigenere/CMakeLists.txt;54;add_test;/Users/nicolas/Desktop/Proiecte/C++/itc26project-go54yid/vigenere/CMakeLists.txt;0;")
