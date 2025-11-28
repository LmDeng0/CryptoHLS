// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2021.1 (64-bit)
// Copyright 1986-2021 Xilinx, Inc. All Rights Reserved.
// ==============================================================
#ifndef __aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_47_2_Rcon_H__
#define __aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_47_2_Rcon_H__


#include <systemc>
using namespace sc_core;
using namespace sc_dt;




#include <iostream>
#include <fstream>

struct aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_47_2_Rcon_ram : public sc_core::sc_module {

  static const unsigned DataWidth = 8;
  static const unsigned AddressRange = 11;
  static const unsigned AddressWidth = 4;

//latency = 1
//input_reg = 1
//output_reg = 0
sc_core::sc_in <sc_lv<AddressWidth> > address0;
sc_core::sc_in <sc_logic> ce0;
sc_core::sc_out <sc_lv<DataWidth> > q0;
sc_core::sc_in<sc_logic> reset;
sc_core::sc_in<bool> clk;


sc_lv<DataWidth> ram[AddressRange];


   SC_CTOR(aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_47_2_Rcon_ram) {
        ram[0] = "0b10001101";
        ram[1] = "0b00000001";
        ram[2] = "0b00000010";
        ram[3] = "0b00000100";
        ram[4] = "0b00001000";
        ram[5] = "0b00010000";
        ram[6] = "0b00100000";
        ram[7] = "0b01000000";
        ram[8] = "0b10000000";
        ram[9] = "0b00011011";
        ram[10] = "0b00110110";


SC_METHOD(prc_write_0);
  sensitive<<clk.pos();
   }


void prc_write_0()
{
    if (ce0.read() == sc_dt::Log_1) 
    {
            if(address0.read().is_01() && address0.read().to_uint()<AddressRange)
              q0 = ram[address0.read().to_uint()];
            else
              q0 = sc_lv<DataWidth>();
    }
}


}; //endmodule


SC_MODULE(aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_47_2_Rcon) {


static const unsigned DataWidth = 8;
static const unsigned AddressRange = 11;
static const unsigned AddressWidth = 4;

sc_core::sc_in <sc_lv<AddressWidth> > address0;
sc_core::sc_in<sc_logic> ce0;
sc_core::sc_out <sc_lv<DataWidth> > q0;
sc_core::sc_in<sc_logic> reset;
sc_core::sc_in<bool> clk;


aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_47_2_Rcon_ram* meminst;


SC_CTOR(aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_47_2_Rcon) {
meminst = new aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_47_2_Rcon_ram("aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_47_2_Rcon_ram");
meminst->address0(address0);
meminst->ce0(ce0);
meminst->q0(q0);

meminst->reset(reset);
meminst->clk(clk);
}
~aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_47_2_Rcon() {
    delete meminst;
}


};//endmodule
#endif
