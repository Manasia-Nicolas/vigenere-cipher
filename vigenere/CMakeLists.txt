file(COPY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/wordlist-en_US/en_US.txt"
     "${CMAKE_CURRENT_BINARY_DIR}/en_US.txt")

# Warning flags applied to all targets in this directory.
add_compile_options(-Wall -Wextra -Wpedantic)

# ---- main program ----------------------------------------------------------
add_executable(vigenere main.cpp)
target_compile_features(vigenere PRIVATE "cxx_std_${PROJECT_CXX_STANDARD}")

# ---- unit tests (doctest + CTest) ------------------------------------------
enable_testing()

add_executable(vigenere_tests tests/test_vigenere.cpp)
target_compile_features(vigenere_tests PRIVATE "cxx_std_${PROJECT_CXX_STANDARD}")

# The tests read the sample files and wordlists by their repo-relative paths,
# so run them from the repository root.
add_test(NAME vigenere_tests COMMAND vigenere_tests)
set_tests_properties(vigenere_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# Optional sanitizer build of the tests: -DVIGENERE_SANITIZE=ON
option(VIGENERE_SANITIZE "Build tests with Address/UB sanitizers" OFF)
if(VIGENERE_SANITIZE)
  target_compile_options(vigenere_tests PRIVATE -fsanitize=address,undefined -g)
  target_link_options(vigenere_tests PRIVATE -fsanitize=address,undefined)
endif()

# ---- benchmark -------------------------------------------------------------
add_executable(vigenere_bench bench/bench_vigenere.cpp)
target_compile_features(vigenere_bench PRIVATE "cxx_std_${PROJECT_CXX_STANDARD}")
target_compile_options(vigenere_bench PRIVATE -O2)
