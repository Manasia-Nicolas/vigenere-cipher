file(COPY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/wordlist-en_US/en_US.txt"
     "${CMAKE_CURRENT_BINARY_DIR}/en_US.txt")

add_executable(vigenere main.cpp)
target_compile_features(vigenere PRIVATE "cxx_std_${PROJECT_CXX_STANDARD}")
