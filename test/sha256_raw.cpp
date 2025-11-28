#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <iostream>
using std::uint32_t; using std::uint8_t; using std::uint64_t;

static inline uint32_t rotr(uint32_t x, int n){ return (x>>n)|(x<<(32-n)); }

static void sha256(const uint8_t* msg, size_t len, uint8_t out32[32]) {
    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t H[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};

    std::vector<uint8_t> data(msg,msg+len);
    data.push_back(0x80);
    while ((data.size()%64)!=56) data.push_back(0x00);
    uint64_t bitlen=(uint64_t)len*8;
    for(int i=7;i>=0;--i) data.push_back((uint8_t)((bitlen>>(8*i))&0xff));

    for(size_t off=0; off<data.size(); off+=64){
        uint32_t w[64];
        for(int i=0;i<16;i++){
            w[i]=(data[off+4*i]<<24)|(data[off+4*i+1]<<16)|(data[off+4*i+2]<<8)|(data[off+4*i+3]);
        }
        for(int i=16;i<64;i++){
            uint32_t s0 = rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);
            uint32_t s1 = rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);
            w[i]=w[i-16]+s0+w[i-7]+s1;
        }
        uint32_t a=H[0],b=H[1],c=H[2],d=H[3],e=H[4],f=H[5],g=H[6],h=H[7];
        for(int i=0;i<64;i++){
            uint32_t S1=rotr(e,6)^rotr(e,11)^rotr(e,25);
            uint32_t ch=(e&f)^((~e)&g);
            uint32_t temp1=h+S1+ch+K[i]+w[i];
            uint32_t S0=rotr(a,2)^rotr(a,13)^rotr(a,22);
            uint32_t maj=(a&b)^(a&c)^(b&c);
            uint32_t temp2=S0+maj;
            h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
        }
        H[0]+=a; H[1]+=b; H[2]+=c; H[3]+=d; H[4]+=e; H[5]+=f; H[6]+=g; H[7]+=h;
    }

    for(int i=0;i<8;i++){
        out32[4*i+0]=(uint8_t)((H[i]>>24)&0xff);
        out32[4*i+1]=(uint8_t)((H[i]>>16)&0xff);
        out32[4*i+2]=(uint8_t)((H[i]>>8)&0xff);
        out32[4*i+3]=(uint8_t)(H[i]&0xff);
    }
}

static std::string hex(const uint8_t* p, size_t n){
    static const char* hexd="0123456789abcdef";
    std::string s; s.reserve(2*n);
    for(size_t i=0;i<n;i++){ s.push_back(hexd[p[i]>>4]); s.push_back(hexd[p[i]&15]); }
    return s;
}

int main(){
    const char* m="abc";
    uint8_t out[32]; sha256((const uint8_t*)m,3,out);
    std::cout<<"SHA256(\"abc\") = "<<hex(out,32)<<"\n";
    // 期望：ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    return 0;
}
