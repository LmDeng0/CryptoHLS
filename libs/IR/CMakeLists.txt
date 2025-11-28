add_library(CryptoHLS_IR STATIC
    IRAnalyzer.cpp
)

target_include_directories(CryptoHLS_IR PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(CryptoHLS_IR PRIVATE
    LLVMCore
    LLVMSupport
    LLVMDemangle 
)


