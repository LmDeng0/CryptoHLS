open_project hls_eval
set_top aes_encrypt
add_files /home/CryptoHLS/test/aes_raw.cpp
open_solution "sol1"
set_part {xcvu9p-flga2104-2L-e}
create_clock -period 10.0 -name default
csynth_design
exit
