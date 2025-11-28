add_library(CryptoHLS_Analysis AnalysisPasses.cpp)

target_include_directories(CryptoHLS_Analysis PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# 我们的分析Pass需要链接到LLVM的核心库和IR库
target_link_libraries(CryptoHLS_Analysis PRIVATE
    LLVMCore
    LLVMIRReader
    LLVMSupport
)