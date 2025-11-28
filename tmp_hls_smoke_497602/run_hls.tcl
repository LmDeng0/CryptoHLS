
open_project -reset /home/CryptoHLS/tmp_hls_smoke_497602
add_files /home/CryptoHLS/test/aes_raw.cpp -cflags "-std=c++17"
set_top aes_encrypt
open_solution -reset solution1
set_part xcvu9p-flga2104-2L-e
create_clock -period 10.0 -name default
csynth_design
exit
