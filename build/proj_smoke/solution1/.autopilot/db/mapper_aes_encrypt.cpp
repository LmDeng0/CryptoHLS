#include <systemc>
#include <vector>
#include <iostream>
#include "hls_stream.h"
#include "ap_int.h"
#include "ap_fixed.h"
using namespace std;
using namespace sc_dt;
class AESL_RUNTIME_BC {
  public:
    AESL_RUNTIME_BC(const char* name) {
      file_token.open( name);
      if (!file_token.good()) {
        cout << "Failed to open tv file " << name << endl;
        exit (1);
      }
      file_token >> mName;//[[[runtime]]]
    }
    ~AESL_RUNTIME_BC() {
      file_token.close();
    }
    int read_size () {
      int size = 0;
      file_token >> mName;//[[transaction]]
      file_token >> mName;//transaction number
      file_token >> mName;//pop_size
      size = atoi(mName.c_str());
      file_token >> mName;//[[/transaction]]
      return size;
    }
  public:
    fstream file_token;
    string mName;
};
extern "C" void aes_encrypt(char*, char*, char*);
extern "C" void apatb_aes_encrypt_hw(volatile void * __xlx_apatb_param_key, volatile void * __xlx_apatb_param_plaintext, volatile void * __xlx_apatb_param_ciphertext) {
  // Collect __xlx_key__tmp_vec
  vector<sc_bv<8> >__xlx_key__tmp_vec;
  for (int j = 0, e = 16; j != e; ++j) {
    __xlx_key__tmp_vec.push_back(((char*)__xlx_apatb_param_key)[j]);
  }
  int __xlx_size_param_key = 16;
  int __xlx_offset_param_key = 0;
  int __xlx_offset_byte_param_key = 0*1;
  char* __xlx_key__input_buffer= new char[__xlx_key__tmp_vec.size()];
  for (int i = 0; i < __xlx_key__tmp_vec.size(); ++i) {
    __xlx_key__input_buffer[i] = __xlx_key__tmp_vec[i].range(7, 0).to_uint64();
  }
  // Collect __xlx_plaintext__tmp_vec
  vector<sc_bv<8> >__xlx_plaintext__tmp_vec;
  for (int j = 0, e = 16; j != e; ++j) {
    __xlx_plaintext__tmp_vec.push_back(((char*)__xlx_apatb_param_plaintext)[j]);
  }
  int __xlx_size_param_plaintext = 16;
  int __xlx_offset_param_plaintext = 0;
  int __xlx_offset_byte_param_plaintext = 0*1;
  char* __xlx_plaintext__input_buffer= new char[__xlx_plaintext__tmp_vec.size()];
  for (int i = 0; i < __xlx_plaintext__tmp_vec.size(); ++i) {
    __xlx_plaintext__input_buffer[i] = __xlx_plaintext__tmp_vec[i].range(7, 0).to_uint64();
  }
  // Collect __xlx_ciphertext__tmp_vec
  vector<sc_bv<8> >__xlx_ciphertext__tmp_vec;
  for (int j = 0, e = 16; j != e; ++j) {
    __xlx_ciphertext__tmp_vec.push_back(((char*)__xlx_apatb_param_ciphertext)[j]);
  }
  int __xlx_size_param_ciphertext = 16;
  int __xlx_offset_param_ciphertext = 0;
  int __xlx_offset_byte_param_ciphertext = 0*1;
  char* __xlx_ciphertext__input_buffer= new char[__xlx_ciphertext__tmp_vec.size()];
  for (int i = 0; i < __xlx_ciphertext__tmp_vec.size(); ++i) {
    __xlx_ciphertext__input_buffer[i] = __xlx_ciphertext__tmp_vec[i].range(7, 0).to_uint64();
  }
  // DUT call
  aes_encrypt(__xlx_key__input_buffer, __xlx_plaintext__input_buffer, __xlx_ciphertext__input_buffer);
// print __xlx_apatb_param_key
  sc_bv<8>*__xlx_key_output_buffer = new sc_bv<8>[__xlx_size_param_key];
  for (int i = 0; i < __xlx_size_param_key; ++i) {
    __xlx_key_output_buffer[i] = __xlx_key__input_buffer[i+__xlx_offset_param_key];
  }
  for (int i = 0; i < __xlx_size_param_key; ++i) {
    ((char*)__xlx_apatb_param_key)[i] = __xlx_key_output_buffer[i].to_uint();
  }
// print __xlx_apatb_param_plaintext
  sc_bv<8>*__xlx_plaintext_output_buffer = new sc_bv<8>[__xlx_size_param_plaintext];
  for (int i = 0; i < __xlx_size_param_plaintext; ++i) {
    __xlx_plaintext_output_buffer[i] = __xlx_plaintext__input_buffer[i+__xlx_offset_param_plaintext];
  }
  for (int i = 0; i < __xlx_size_param_plaintext; ++i) {
    ((char*)__xlx_apatb_param_plaintext)[i] = __xlx_plaintext_output_buffer[i].to_uint();
  }
// print __xlx_apatb_param_ciphertext
  sc_bv<8>*__xlx_ciphertext_output_buffer = new sc_bv<8>[__xlx_size_param_ciphertext];
  for (int i = 0; i < __xlx_size_param_ciphertext; ++i) {
    __xlx_ciphertext_output_buffer[i] = __xlx_ciphertext__input_buffer[i+__xlx_offset_param_ciphertext];
  }
  for (int i = 0; i < __xlx_size_param_ciphertext; ++i) {
    ((char*)__xlx_apatb_param_ciphertext)[i] = __xlx_ciphertext_output_buffer[i].to_uint();
  }
}
