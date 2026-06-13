file(COPY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/wordlist-en_US/en_US.txt"
     "${CMAKE_CURRENT_BINARY_DIR}/en_US.txt")

# Warning flags applied to all targets in this directory.
add_compile_options(-Wall -Wextra -Wpedantic)

# ---- shared core library ---------------------------------------------------
set(VIGENERE_PUBLIC_HEADERS
    src/alphabet.hpp
    src/file_io.hpp
    src/language_resources.hpp
    src/segmenter.hpp
    src/vigenere.hpp)

add_library(vigenere_core STATIC src/language_resources.cpp ${VIGENERE_PUBLIC_HEADERS})
target_include_directories(vigenere_core PUBLIC src)
target_compile_features(vigenere_core PUBLIC "cxx_std_${PROJECT_CXX_STANDARD}")

# ---- main program ----------------------------------------------------------
add_executable(vigenere src/main.cpp)
target_link_libraries(vigenere PRIVATE vigenere_core)
target_compile_features(vigenere PRIVATE "cxx_std_${PROJECT_CXX_STANDARD}")

# ---- unit tests (doctest + CTest) ------------------------------------------
enable_testing()

add_executable(vigenere_tests tests/test_vigenere.cpp)
target_link_libraries(vigenere_tests PRIVATE vigenere_core)
target_compile_features(vigenere_tests PRIVATE "cxx_std_${PROJECT_CXX_STANDARD}")

# The tests read the sample files and wordlists by their repo-relative paths,
# so run them from the repository root.
add_test(NAME vigenere_tests COMMAND vigenere_tests)
set_tests_properties(vigenere_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# Exercise the executable and its organized data paths, not only the headers.
add_test(NAME vigenere_cli_short
    COMMAND vigenere en vigenere/data/short-encrypted.txt)
set_tests_properties(vigenere_cli_short PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "1.LOCK")

add_test(NAME vigenere_cli_default COMMAND vigenere)
set_tests_properties(vigenere_cli_default PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "1.FAIRY")

add_test(NAME vigenere_cli_german
    COMMAND vigenere de vigenere/data/german-encrypted.txt)
set_tests_properties(vigenere_cli_german PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "1.MÄRCHEN")

add_test(NAME vigenere_cli_spaceless
    COMMAND vigenere en vigenere/data/spaceless-encrypted.txt)
set_tests_properties(vigenere_cli_spaceless PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "3.BY THE SIDE")

# ---- benchmark -------------------------------------------------------------
add_executable(vigenere_bench bench/bench_vigenere.cpp)
target_link_libraries(vigenere_bench PRIVATE vigenere_core)
target_compile_features(vigenere_bench PRIVATE "cxx_std_${PROJECT_CXX_STANDARD}")
target_compile_options(vigenere_bench PRIVATE -O2)

# Optional sanitizer build: -DVIGENERE_SANITIZE=ON
option(VIGENERE_SANITIZE "Build targets with Address/UB sanitizers" OFF)
if(VIGENERE_SANITIZE)
  foreach(target vigenere_core vigenere vigenere_tests vigenere_bench)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -g)
  endforeach()
  foreach(target vigenere vigenere_tests vigenere_bench)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endforeach()
endif()
