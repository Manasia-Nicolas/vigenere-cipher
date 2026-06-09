option(BUILD_REFERENCE "Build the reference code (gmp-chudnovsky)" OFF)

if(BUILD_REFERENCE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GMP REQUIRED IMPORTED_TARGET gmp)

  file(DOWNLOAD https://gmplib.org/download/misc/gmp-chudnovsky.c
       "${CMAKE_CURRENT_BINARY_DIR}/gmp-chudnovsky.c" SHOW_PROGRESS)

  add_executable(gmp-chudnovsky "${CMAKE_CURRENT_BINARY_DIR}/gmp-chudnovsky.c")
  target_include_directories(gmp-chudnovsky PRIVATE ${GMP_INCLUDE_DIRS})
  target_link_libraries(gmp-chudnovsky PRIVATE PkgConfig::GMP m)
  #[[
	gmp-chudnovsky <digits> <1 if you want to print pi>
	gmp-chudnovsky 100 1
	gmp-chudnovsky 1000 1
	]]
endif()

add_executable(pi main.cpp)
target_compile_features(pi PRIVATE "cxx_std_${PROJECT_CXX_STANDARD}")
