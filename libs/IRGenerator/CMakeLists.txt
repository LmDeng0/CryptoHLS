add_library(CryptoHLS_IRGenerator IRGenerator.cpp)

target_include_directories(CryptoHLS_IRGenerator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# IRGenerator 依赖于 LLVMSupport 库
target_link_libraries(CryptoHLS_IRGenerator
    PRIVATE
    LLVMSupport
)