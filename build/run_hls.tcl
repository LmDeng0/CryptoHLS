
open_project -reset hls_project
add_files ../test/aes_raw.optimized.cpp -cflags "-std=c++17"
set_top aes_encrypt
open_solution -reset solution1
set_part xcvu9p-flga2104-2L-e
create_clock -period 10 -name default
csynth_design
exit
