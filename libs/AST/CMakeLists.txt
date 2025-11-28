add_library(CryptoHLS_AST STATIC
    DesignSpaceVisitor.cpp
    PragmaInserter.cpp 
)

target_include_directories(CryptoHLS_AST PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(CryptoHLS_AST PRIVATE
    clangAST
    clangBasic
    clangRewrite  # <--- [修正] 添加这个库以支持 Rewriter
)
