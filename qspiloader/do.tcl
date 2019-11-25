reset halt
load_image $firmware
verify_image $firmware
arm semihosting enable
# arm semihosting_fileio  enable
#arm semihosting_resexit enable
arm semihosting_cmdline "$binary_file"
set base 0x24000000

set Reset_Handler [format "0x%08X" [expr ([mrw [expr $base + 0x04]] & 0xFFFFFFFE)]]

puts "Reset_Handler $Reset_Handler "

reset halt
reg pc $Reset_Handler
resume
wait_halt 30000
reg r0
exit
