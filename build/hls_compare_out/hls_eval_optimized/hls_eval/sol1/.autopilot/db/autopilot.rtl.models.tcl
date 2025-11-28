set SynModuleInfo {
  {SRCNAME aes_encrypt_Pipeline_VITIS_LOOP_41_1 MODELNAME aes_encrypt_Pipeline_VITIS_LOOP_41_1 RTLNAME aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_41_1
    SUBMODULES {
      {MODELNAME aes_encrypt_flow_control_loop_pipe_sequential_init RTLNAME aes_encrypt_flow_control_loop_pipe_sequential_init BINDTYPE interface TYPE internal_upc_flow_control INSTNAME aes_encrypt_flow_control_loop_pipe_sequential_init_U}
    }
  }
  {SRCNAME aes_encrypt_Pipeline_VITIS_LOOP_47_2 MODELNAME aes_encrypt_Pipeline_VITIS_LOOP_47_2 RTLNAME aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_47_2
    SUBMODULES {
      {MODELNAME aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_47_2_Rcon RTLNAME aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_47_2_Rcon BINDTYPE storage TYPE rom IMPL auto LATENCY 2 ALLOW_PRAGMA 1}
    }
  }
  {SRCNAME aes_encrypt_Pipeline_VITIS_LOOP_160_1_VITIS_LOOP_161_2 MODELNAME aes_encrypt_Pipeline_VITIS_LOOP_160_1_VITIS_LOOP_161_2 RTLNAME aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_160_1_VITIS_LOOP_161_2}
  {SRCNAME cipher_Pipeline_VITIS_LOOP_77_1_VITIS_LOOP_78_2 MODELNAME cipher_Pipeline_VITIS_LOOP_77_1_VITIS_LOOP_78_2 RTLNAME aes_encrypt_cipher_Pipeline_VITIS_LOOP_77_1_VITIS_LOOP_78_2}
  {SRCNAME cipher_Pipeline_VITIS_LOOP_136_1 MODELNAME cipher_Pipeline_VITIS_LOOP_136_1 RTLNAME aes_encrypt_cipher_Pipeline_VITIS_LOOP_136_1
    SUBMODULES {
      {MODELNAME aes_encrypt_cipher_Pipeline_VITIS_LOOP_136_1_sbox RTLNAME aes_encrypt_cipher_Pipeline_VITIS_LOOP_136_1_sbox BINDTYPE storage TYPE rom IMPL auto LATENCY 2 ALLOW_PRAGMA 1}
    }
  }
  {SRCNAME cipher_Pipeline_VITIS_LOOP_85_1_VITIS_LOOP_86_2 MODELNAME cipher_Pipeline_VITIS_LOOP_85_1_VITIS_LOOP_86_2 RTLNAME aes_encrypt_cipher_Pipeline_VITIS_LOOP_85_1_VITIS_LOOP_86_2}
  {SRCNAME cipher_Pipeline_VITIS_LOOP_77_1_VITIS_LOOP_78_22 MODELNAME cipher_Pipeline_VITIS_LOOP_77_1_VITIS_LOOP_78_22 RTLNAME aes_encrypt_cipher_Pipeline_VITIS_LOOP_77_1_VITIS_LOOP_78_22}
  {SRCNAME cipher MODELNAME cipher RTLNAME aes_encrypt_cipher}
  {SRCNAME aes_encrypt_Pipeline_VITIS_LOOP_171_3_VITIS_LOOP_172_4 MODELNAME aes_encrypt_Pipeline_VITIS_LOOP_171_3_VITIS_LOOP_172_4 RTLNAME aes_encrypt_aes_encrypt_Pipeline_VITIS_LOOP_171_3_VITIS_LOOP_172_4}
  {SRCNAME aes_encrypt MODELNAME aes_encrypt RTLNAME aes_encrypt IS_TOP 1
    SUBMODULES {
      {MODELNAME aes_encrypt_RoundKey RTLNAME aes_encrypt_RoundKey BINDTYPE storage TYPE ram IMPL auto LATENCY 2 ALLOW_PRAGMA 1}
      {MODELNAME aes_encrypt_state RTLNAME aes_encrypt_state BINDTYPE storage TYPE ram IMPL auto LATENCY 2 ALLOW_PRAGMA 1}
    }
  }
}
