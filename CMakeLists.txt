cmake_minimum_required(VERSION 3.10)
project(FBS)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(external/q3b)

add_library(q3b_includes INTERFACE)

target_include_directories(q3b_includes INTERFACE
    ${Z3_CXX_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/q3b/contrib/z3/src/api/c++
    ${CMAKE_CURRENT_SOURCE_DIR}/external/q3b/contrib/cudd/cudd
    ${CMAKE_CURRENT_SOURCE_DIR}/external/q3b/contrib/cudd/cplusplus
    ${CMAKE_CURRENT_SOURCE_DIR}/build/external/q3b/antlr4cpp_generated_src/SMTLIBv2
    ${CMAKE_CURRENT_SOURCE_DIR}/build/external/q3b/antlr4_runtime/src/antlr4_runtime/runtime/Cpp/runtime/src
    ${ANTLR4_INCLUDE_DIRS}
    ${ANTLR_SmtLibParser_OUTPUT_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/q3b/lib
)

add_executable(fbs src/main.cpp src/FBS_SMTVisitor.cpp src/FormulaSimplifier.cpp src/FBSLogger.cpp src/SimplifierThread.cpp src/SimplifierBasic.cpp src/TimeoutManager.cpp src/Settings.cpp)

target_link_libraries(fbs PRIVATE q3blib q3b_includes)

