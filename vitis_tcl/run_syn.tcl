# ============================================================================
# Vitis HLS Synthesis Test Script for CryptoHLS
#
# 作用:
#   验证由CryptoHLS生成的 .optimized.cpp 文件是否能被Vitis HLS正确综合。
#
# 使用方法 (在终端中，确保Vitis HLS环境已设置):
#   cd /path/to/your/build/directory
#   vitis_hls -f ../test_synthesis.tcl
# ============================================================================

# --- 1. 配置区 ---

# [待验证] 设置由 CryptoHLS 生成的优化后 C++ 文件的路径
# 我们假设此脚本是从 build 目录运行的
set OPTIMIZED_CPP_FILE "../test/aes_raw.optimized.cpp"

# [待验证] 设置 Testbench 文件的路径
set TESTBENCH_FILE "../test/aes_tb.cpp"

# [待验证] 设置顶层函数的名字
set TOP_FUNCTION "aes_encrypt"

# [请确认] 设置您的目标FPGA器件型号
set FPGA_PART "xcvu9p-flga2104-2L-e"

# --- 2. 脚本执行区 ---

puts "INFO: Starting Vitis HLS synthesis test..."
puts "INFO: Target source file: $OPTIMIZED_CPP_FILE"

# 创建一个临时的HLS工程
open_project -reset "verification_project"
set_top $TOP_FUNCTION

# 添加设计和测试文件
add_files $OPTIMIZED_CPP_FILE
add_files -tb $TESTBENCH_FILE

# 创建一个解决方案
open_solution -reset "solution1"
set_part $FPGA_PART
create_clock -period 10 -name default

# 步骤 1: 运行 C 仿真
# 验证代码在逻辑功能上是否正确
puts "\n[STEP 1] Running C-Simulation..."
if {[catch {csim_design} result]} {
    puts "ERROR: C-Simulation failed. Check the C++ code for functional errors."
    exit 1
}
puts "INFO: C-Simulation PASSED."

# 步骤 2: 运行 C 综合
# 将 C++ 代码转换为 RTL
puts "\n[STEP 2] Running C-Synthesis..."
if {[catch {csynth_design} result]} {
    puts "ERROR: C-Synthesis failed. Check the .optimized.cpp file for syntax errors or unsupported constructs."
    exit 1
}
puts "INFO: C-Synthesis PASSED."

# 步骤 3: 运行 C/RTL 协同仿真
# 验证生成的RTL与原始C++代码的行为是否一致
puts "\n[STEP 3] Running Co-simulation..."
if {[catch {cosim_design} result]} {
    puts "ERROR: Co-simulation failed. The generated RTL does not match the C++ behavior."
    exit 1
}
puts "INFO: Co-simulation PASSED."

# 步骤 4: 导出RTL IP核
puts "\n[STEP 4] Exporting RTL IP..."
if {[catch {export_design -format ip_catalog} result]} {
    puts "ERROR: RTL export failed."
    exit 1
}
puts "INFO: RTL IP exported successfully."

puts "\n================================================="
puts "SUCCESS: The generated C++ file has been successfully synthesized to RTL."
puts "================================================="

exit