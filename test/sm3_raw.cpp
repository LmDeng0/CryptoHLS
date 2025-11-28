#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <iostream>
using std::uint32_t; using std::uint8_t; using std::uint64_t;

static inline uint32_t rol(uint32_t x,int n){ return (x<<n)|(x>>(32-n)); }
static inline uint32_t p0(uint32_t x){ return x ^ rol(x,9) ^ rol(x,17); }
static inline uint32_t p1(uint32_t x){ return x ^ rol(x,15) ^ rol(x,23); }
static inline uint32_t ff(uint32_t x,uint32_t y,uint32_t z,int j){
    if(j<=15) return x^y^z;
    return (x&y)|(x&z)|(y&z);
}
static inline uint32_t gg(uint32_t x,uint32_t y,uint32_t z,int j){
    if(j<=15) return x^y^z;
    return (x&y)|((~x)&z);
}

static void sm3(const uint8_t* msg, size_t len, uint8_t out32[32]){
    uint32_t IV[8]={0x7380166F,0x4914B2B9,0x172442D7,0xDA8A0600,0xA96F30BC,0x163138AA,0xE38DEE4D,0xB0FB0E4E};

    std::vector<uint8_t> data(msg,msg+len);
    data.push_back(0x80);
    while ((data.size()%64)!=56) data.push_back(0x00);
    uint64_t bitlen = (uint64_t)len*8;
    for(int i=7;i>=0;--i) data.push_back((uint8_t)((bitlen>>(8*i))&0xff));

    for(size_t off=0; off<data.size(); off+=64){
        uint32_t W[68], W1[64];
        for(int i=0;i<16;i++){
            W[i]=(data[off+4*i]<<24)|(data[off+4*i+1]<<16)|(data[off+4*i+2]<<8)|(data[off+4*i+3]);
        }
        for(int i=16;i<68;i++){
            W[i]=p1(W[i-16]^W[i-9]^rol(W[i-3],15)) ^ rol(W[i-13],7) ^ W[i-6];
        }
        for(int i=0;i<64;i++){
            W1[i]=W[i]^W[i+4];
        }
        uint32_t A=IV[0],B=IV[1],C=IV[2],D=IV[3],E=IV[4],F=IV[5],G=IV[6],H=IV[7];
        for(int j=0;j<64;j++){
            uint32_t Tj = (j<=15)?0x79CC4519:0x7A879D8A;
            uint32_t SS1 = rol((rol(A,12) + E + rol(Tj, j%32)) & 0xFFFFFFFF, 7);
            uint32_t SS2 = SS1 ^ rol(A,12);
            uint32_t TT1 = (ff(A,B,C,j) + D + SS2 + W1[j]) & 0xFFFFFFFF;
            uint32_t TT2 = (gg(E,F,G,j) + H + SS1 + W[j]) & 0xFFFFFFFF;
            D=C; C=rol(B,9); B=A; A=TT1;
            H=G; G=rol(F,19); F=E; E=p0(TT2);
        }
        IV[0]^=A; IV[1]^=B; IV[2]^=C; IV[3]^=D;
        IV[4]^=E; IV[5]^=F; IV[6]^=G; IV[7]^=H;
    }
    for(int i=0;i<8;i++){
        out32[4*i+0]=(uint8_t)((IV[i]>>24)&0xff);
        out32[4*i+1]=(uint8_t)((IV[i]>>16)&0xff);
        out32[4*i+2]=(uint8_t)((IV[i]>>8)&0xff);
        out32[4*i+3]=(uint8_t)(IV[i]&0xff);
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
    uint8_t out[32]; sm3((const uint8_t*)m,3,out);
    std::cout<<"SM3(\"abc\") = "<<hex(out,32)<<"\n";
    // 期望：66c7f0f462eeedd9d1f2d46bdc10e4e2
    //       4167c4875cf2f7a2b8e0f4b5 (拼起来 32 字节)
    // 完整：66c7f0f462eeedd9d1f2d46bdc10e4e24167c4875cf2f7a2b8e0f4b5
    return 0;
}
