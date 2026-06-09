cmake_minimum_required(VERSION 3.25...4.2)

project(
  FinalProject
  VERSION 0.0.0
  LANGUAGES CXX C)

set(PROJECT_CXX_STANDARD
    23
    CACHE STRING "C++ standard, available option: 20, 23")
set_property(CACHE PROJECT_CXX_STANDARD PROPERTY STRINGS 20 23)

set(PROJECT_TOPIC
    NOT_SELECTED
    CACHE STRING "Project topic, please select one of QRCODE, VIGENERE, PI")
set_property(CACHE PROJECT_TOPIC PROPERTY STRINGS NOT_SELECTED QRCODE VIGENERE
                                          PI)

if(PROJECT_TOPIC STREQUAL NOT_SELECTED)
  message(FATAL_ERROR "Please select a topic.")
elseif(PROJECT_TOPIC STREQUAL QRCODE)
  add_subdirectory(qrcode)
elseif(PROJECT_TOPIC STREQUAL VIGENERE)
  add_subdirectory(vigenere)
elseif(PROJECT_TOPIC STREQUAL PI)
  add_subdirectory(pi)
endif()
