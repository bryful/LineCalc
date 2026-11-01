cmake_minimum_required(VERSION 3.22)
project(LineCalcAndroid LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
add_library(linecalc SHARED
    NativeCalculator.cpp
    "${REPO_ROOT}/LineCalc/Calculator.cpp"
    "${REPO_ROOT}/vendor/quickjs/quickjs-amalgam.c")
target_include_directories(linecalc PRIVATE "${REPO_ROOT}/LineCalc")
# codecvtはC++20で有効な標準変換として使用する。
target_compile_definitions(linecalc PRIVATE _GNU_SOURCE _LIBCPP_DISABLE_DEPRECATION_WARNINGS)
target_compile_options(linecalc PRIVATE -Wall -Wextra)
set_source_files_properties("${REPO_ROOT}/vendor/quickjs/quickjs-amalgam.c" PROPERTIES COMPILE_OPTIONS "-w")
target_link_libraries(linecalc PRIVATE m)
