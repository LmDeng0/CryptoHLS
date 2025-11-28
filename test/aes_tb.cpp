#include <iostream>
#include <cstdint>
#include <cstring>

// ============================================================================
// Testbench for AES HLS core
// ============================================================================

// 必须声明您要测试的顶层函数
void aes_encrypt(uint8_t key[16], uint8_t plaintext[16], uint8_t ciphertext[16]);

int main() {
    // 使用一组标准的AES测试向量 (NIST FIPS-197, Appendix C.1)
    uint8_t key[16] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
    };

    uint8_t plaintext[16] = {
        0x32, 0x43, 0xf6, 0xa8, 0x88, 0x5a, 0x30, 0x8d,
        0x31, 0x31, 0x98, 0xa2, 0xe0, 0x37, 0x07, 0x34
    };

    // 预期的正确加密结果
    uint8_t expected_ciphertext[16] = {
        0x39, 0x25, 0x84, 0x1d, 0x02, 0xdc, 0x09, 0xfb,
        0xdc, 0x11, 0x85, 0x97, 0x19, 0x6a, 0x0b, 0x32
    };

    uint8_t actual_ciphertext[16];

    // 调用由CryptoHLS优化后的硬件函数
    aes_encrypt(key, plaintext, actual_ciphertext);

    // 检查结果是否与预期一致
    int errors = 0;
    for (int i = 0; i < 16; ++i) {
        if (actual_ciphertext[i] != expected_ciphertext[i]) {
            std::cout << "ERROR: Mismatch at byte " << i
                      << ": expected " << std::hex << (int)expected_ciphertext[i]
                      << ", got " << (int)actual_ciphertext[i] << std::endl;
            errors++;
        }
    }

    if (errors == 0) {
        std::cout << "SUCCESS: Test passed!" << std::endl;
        return 0; // 返回 0 代表测试成功
    } else {
        std::cout << "FAILURE: Test failed with " << errors << " errors." << std::endl;
        return 1; // 返回非零代表测试失败
    }
}