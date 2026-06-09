include(FetchContent)

FetchContent_Declare(
  ZXing
  GIT_REPOSITORY https://github.com/zxing-cpp/zxing-cpp.git
  GIT_TAG v3.0.2)

FetchContent_MakeAvailable(ZXing)

zxing_add_package_stb()

add_executable(qrcode main.cpp)
target_compile_features(qrcode PRIVATE "cxx_std_${PROJECT_CXX_STANDARD}")
target_link_libraries(qrcode PRIVATE ZXing::ZXing stb::stb)
