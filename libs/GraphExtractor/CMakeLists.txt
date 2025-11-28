# libs/GraphExtractor/CMakeLists.txt

add_library(CryptoHLS_GraphExtractor
    GraphExtractor.cpp
)

target_include_directories(CryptoHLS_GraphExtractor
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# 我们的图提取器依赖于LLVM的核心库和Demangle库
target_link_libraries(CryptoHLS_GraphExtractor
    PRIVATE
    LLVMCore
    LLVMIRReader
    LLVMSupport
    LLVMDemangle
)