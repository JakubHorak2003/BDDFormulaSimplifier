cmake_minimum_required(VERSION 3.10)
project(Bakalarka)

set(CMAKE_CXX_STANDARD 17)

# Add Q3B as a subdirectory (this processes Q3B's CMakeLists.txt)
add_subdirectory(external/q3b)

# Create an INTERFACE library to aggregate Q3B’s include directories.
add_library(q3b_includes INTERFACE)

# Q3B’s CMakeLists.txt adds these include directories:
#
# 1. Z3 include directories from find_package(Z3 CONFIG):
#      include_directories(${Z3_CXX_INCLUDE_DIRS})
#
# 2. CUDD include directories:
#      set(CUDD_DIR "contrib/cudd")
#      include_directories(${CUDD_DIR}/cudd ${CUDD_DIR}/cplusplus)
#    Since Q3B is a submodule, these resolve to:
#      ${CMAKE_CURRENT_SOURCE_DIR}/external/q3b/contrib/cudd/cudd
#      ${CMAKE_CURRENT_SOURCE_DIR}/external/q3b/contrib/cudd/cplusplus
#
# 3. ANTLR include directories:
#      include_directories(SYSTEM ${ANTLR4_INCLUDE_DIRS})
#      include_directories(SYSTEM ${ANTLR_SmtLibParser_OUTPUT_DIR})
#
# 4. Optionally, you may want to add Q3B’s own source directory:
#      ${CMAKE_CURRENT_SOURCE_DIR}/external/q3b/lib
#
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

# Now add your own executable target.
add_executable(myapp src/main.cpp src/FormulaSimplifier.cpp src/FAS_SMTVisitor.cpp)

# Link your executable with both the Q3B library and the interface target carrying include directories.
target_link_libraries(myapp PRIVATE q3blib q3b_includes)

