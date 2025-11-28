add_library(CryptoHLS_DSE STATIC
    DSE.cpp
)

target_include_directories(CryptoHLS_DSE PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
