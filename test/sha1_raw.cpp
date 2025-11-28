#include <cstdint>
#include <cstring>

// ===========================
// SHA-1 原始实现（HLS 友好版）
// ===========================
//
// 设计要点：
// 1) 无 STL/动态内存；
// 2) 顶层函数 sha1(msg, msg_len, out20)；
// 3) 处理任意长度消息：逐块处理 + 最多 2 个尾块(128B)做 padding；
// 4) 不包含任何 HLS pragma，由 CryptoHLS 后续自动插入。

static inline uint32_t rol32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

// 将 big-endian 字节序转换为 32 位整
static inline uint32_t be32_load(const uint8_t* p) {
    return ( (uint32_t)p[0] << 24 ) |
           ( (uint32_t)p[1] << 16 ) |
           ( (uint32_t)p[2] <<  8 ) |
           ( (uint32_t)p[3]       );
}

// 将 32 位写成 big-endian
static inline void be32_store(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xff);
    p[1] = (uint8_t)((v >> 16) & 0xff);
    p[2] = (uint8_t)((v >>  8) & 0xff);
    p[3] = (uint8_t)( v        & 0xff);
}

// 处理 64 字节一块
static void sha1_compress(const uint8_t block[64],
                          uint32_t& h0, uint32_t& h1, uint32_t& h2,
                          uint32_t& h3, uint32_t& h4)
{
    uint32_t w[80];
    // 前 16 个字
    for (int i = 0; i < 16; ++i) {
        w[i] = be32_load(block + 4*i);
    }
    // 扩展到 80 个字
    for (int i = 16; i < 80; ++i) {
        w[i] = rol32( w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1 );
    }

    uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if      (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
        else if (i < 40) { f = b ^ c ^ d;            k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
        else             { f = b ^ c ^ d;            k = 0xCA62C1D6u; }

        uint32_t temp = rol32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rol32(b, 30);
        b = a;
        a = temp;
    }

    h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
}

// 顶层函数：对任意长度消息做 SHA-1，输出 20 字节摘要
// 注意：不加任何 pragma（保持“原始输入”），由 CryptoHLS 进行 DSE。
void sha1(uint8_t* msg, unsigned msg_len, uint8_t out20[20]) {
    // 初始值
    uint32_t h0 = 0x67452301u;
    uint32_t h1 = 0xEFCDAB89u;
    uint32_t h2 = 0x98BADCFEu;
    uint32_t h3 = 0x10325476u;
    uint32_t h4 = 0xC3D2E1F0u;

    // 1) 处理所有完整的 64 字节块
    unsigned off = 0;
    while (off + 64u <= msg_len) {
        sha1_compress(msg + off, h0, h1, h2, h3, h4);
        off += 64u;
    }

    // 2) 构造尾部（最多两个 64B 块 = 128 字节）
    uint8_t tail[128];
    // 清零
    for (int i = 0; i < 128; ++i) tail[i] = 0;

    unsigned rem = msg_len - off;       // 剩余未处理的字节数
    // 拷贝剩余字节
    for (unsigned i = 0; i < rem; ++i) {
        tail[i] = msg[off + i];
    }
    // 追加 0x80
    tail[rem] = 0x80;

    // 总比特长度（big-endian 写入最后 8 字节）
    // 注意：msg_len 乘 8 需要 64 位
    uint64_t bitlen = (uint64_t)msg_len * 8u;

    // 计算尾部总长度：如果 (rem + 1 + 8) <= 64 则 1 块，否则 2 块
    unsigned total_tail = (rem + 1 + 8 <= 64) ? 64u : 128u;

    // 将 bitlength 写到最后 8 字节（big-endian）
    unsigned len_pos = total_tail - 8u;
    tail[len_pos + 0] = (uint8_t)((bitlen >> 56) & 0xff);
    tail[len_pos + 1] = (uint8_t)((bitlen >> 48) & 0xff);
    tail[len_pos + 2] = (uint8_t)((bitlen >> 40) & 0xff);
    tail[len_pos + 3] = (uint8_t)((bitlen >> 32) & 0xff);
    tail[len_pos + 4] = (uint8_t)((bitlen >> 24) & 0xff);
    tail[len_pos + 5] = (uint8_t)((bitlen >> 16) & 0xff);
    tail[len_pos + 6] = (uint8_t)((bitlen >>  8) & 0xff);
    tail[len_pos + 7] = (uint8_t)( bitlen        & 0xff);

    // 压缩 1 或 2 个尾块
    sha1_compress(tail + 0, h0, h1, h2, h3, h4);
    if (total_tail == 128u) {
        sha1_compress(tail + 64, h0, h1, h2, h3, h4);
    }

    // 3) 输出 20 字节 (big-endian)
    be32_store(out20 +  0, h0);
    be32_store(out20 +  4, h1);
    be32_store(out20 +  8, h2);
    be32_store(out20 + 12, h3);
    be32_store(out20 + 16, h4);
}

#ifdef CRYPTOHLS_TEST
// 可选：功能测试（普通 g++ 编译时 -DCRYPTOHLS_TEST 打开）
// 注意：CryptoHLS 运行时不会编译 main。
#include <cstdio>

static const uint8_t msg_abc[] = {'a','b','c'};
static const uint8_t expected_abc[20] = {
    0xa9,0x99,0x3e,0x36,0x47,0x06,0x81,0x6a,0xba,0x3e,
    0x25,0x71,0x78,0x50,0xc2,0x6c,0x9c,0xd0,0xd8,0x9d
};

int main() {
    uint8_t out[20];
    sha1((uint8_t*)msg_abc, 3u, out);

    bool ok = true;
    for (int i = 0; i < 20; ++i) if (out[i] != expected_abc[i]) ok = false;

    std::printf("SHA1(\"abc\") = ");
    for (int i = 0; i < 20; ++i) std::printf("%02x", out[i]);
    std::printf("\nmatch = %s\n", ok ? "true" : "false");
    return ok ? 0 : 1;
}
#endif
