# =================================================================
# CMakeLists.txt for CryptoHLS Project (Final Refactored Version)
# =================================================================

cmake_minimum_required(VERSION 3.16)
project(CryptoHLS)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 查找外部依赖 ---
find_package(LLVM 16 REQUIRED CONFIG)
find_package(Clang 16 REQUIRED CONFIG)


find_package(nlohmann_json 3.2.0 REQUIRED)



include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/libs/Analysis
    ${PROJECT_SOURCE_DIR}/libs/AST
    ${PROJECT_SOURCE_DIR}/libs/GraphExtractor
    ${PROJECT_SOURCE_DIR}/libs/IR
    ${PROJECT_SOURCE_DIR}/libs/IRGenerator
)


# --- 添加我们的子项目库 ---
# CMake会依次进入这些目录，并执行其中的CMakeLists.txt
add_subdirectory(libs/AST)
add_subdirectory(libs/IR)
add_subdirectory(libs/DSE)
add_subdirectory(libs/IRGenerator)
add_subdirectory(libs/Analysis)
add_subdirectory(libs/GraphExtractor)

# --- 定义我们的主程序 ---
# [关键修改] 可执行程序现在叫 cryptohls-tool，源码位于顶层src目录
add_executable(cryptohls-tool src/main.cpp)
# add_executable(aes-test-target test/aes_raw.cpp)
# add_library(aes-test-lib STATIC test/aes_raw.cpp)


# [关键修改] 让主程序链接到我们所有的核心逻辑库
target_link_libraries(cryptohls-tool
    PRIVATE
    # 我们自己的库
    CryptoHLS_AST
    CryptoHLS_IR
    CryptoHLS_DSE
    CryptoHLS_IRGenerator
    CryptoHLS_Analysis 
    CryptoHLS_GraphExtractor
    
    # Clang/LLVM 库
    clangTooling
    clangFrontend
    clangBasic
    clangLex
    clangCodeGen
    LLVMIRReader
    LLVMCore
    LLVMSupport
    ${CLANG_LIBRARIES}
    ${LLVM_LIBRARIES}
)
