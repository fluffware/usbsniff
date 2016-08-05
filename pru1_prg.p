#include <pru.hp>

	
#define RSC_CFG 0x12c00

#define PRU0_CTRL 0x22000
#define PRU1_CTRL 0x24000
#define CONTROL 0x00	
#define STATUS 0x04	
#define WAKEUP_EN 0x08	
#define CYCLE 0x0c	
#define STALL 0x10	
#define CTBIR0 0x20	
#define CTBIR1 0x24	
#define CTPPR0 0x28	
#define CTPPR1 0x2c	

	
#define BUFFER_PA  0x1c
#define BUFFER_LEN  0x20


	.struct RingBuffer
	.u32 start
	.u32 end
	.u32 read
	.u32 write
	.ends

	.struct Registers
	.u32 buffer
	.u16 sequence
	.ends

	.assign Registers, r20, r21.w0, reg

	.assign RingBuffer, r10, r13, buf

	.struct BitRegisters
	.u32 bit_count
	.u32 shift_dp
	.u32 shift_dm
	.ends

	.assign BitRegisters, r2,r4, bit_reg
	
	// Add a block to the ring buffer
	.macro send_block
	.mparam block, len
	lbbo buf, reg.buffer, OFFSET(RingBuffer), SIZE(RingBuffer)
	add r0, buf.write, 2 
	add r0, r0, len
	qbge no_wrap, r0, buf.end // Check if the block will fit before end

	qbgt buffer_full, buf.write, buf.read
	qbeq buffer_full, buf.read, buf.start
	ldi r0, 0
	sbbo r0.b0, buf.write, 0, 1  // Write 0 to mark that the buffer wraps
	mov buf.write, buf.start  // Reset write pointer to beginning of buffer
no_wrap:
	
	qble read_below, buf.write, buf.read // Read pointer is before write pointer
	add r0, buf.write, 1 // Space for length byte
	add r0, r0, len
	qble buffer_full, r0, buf.read // Write pointer would pass read pointer
read_below:
	
	mov r0.b0, len
	sbbo r0.b0, buf.write, 0, 1
	add buf.write, buf.write, 1
	sbbo block, buf.write, 0, len
	add buf.write, buf.write, len

	sbbo buf.write, reg.buffer, OFFSET(RingBuffer.write), SIZE(RingBuffer.write)
buffer_full:
	.endm
	
	.origin 0
	.entrypoint START
START:	
	// Enable OCP master port
	lbco      r0, CONST_PRUCFG, 4, 4
	clr     r0, r0, 4         // Clear SYSCFG[STANDBY_INIT] to enable OCP master port
	sbco      r0, CONST_PRUCFG, 4, 4
	
	mov r0, PRU1_CTRL
	lbbo r1, r0, CONTROL, 4
	set r1, r1, 3 // Enable cycle counter
	sbbo r1, r0, CONTROL, 4
	
	// Set up ring buffer
	mov r1, RSC_CFG
	lbbo r2, r1, BUFFER_PA, 4
	mov reg.buffer, r2
	add buf.start, r2, SIZE(RingBuffer)
	lbbo r3, r1, BUFFER_LEN, 4
	add buf.end, r2, r3
	mov buf.write, buf.start
	mov buf.read, buf.start
	sbbo buf, reg.buffer, OFFSET(RingBuffer), SIZE(RingBuffer)
	
	mov bit_reg.bit_count, 0
	xout XFR_SP0, bit_reg.bit_count, SIZE(BitRegisters.bit_count)
	mov reg.sequence, 0
start_send:	
	xin XFR_SP0, bit_reg, SIZE(BitRegisters)
	qbeq start_send, bit_reg.bit_count, 0
	mov bit_reg.bit_count.w2, reg.sequence
	send_block bit_reg,SIZE(bit_reg)
	add reg.sequence, reg.sequence, 1
	mov bit_reg.bit_count, 0
	xout XFR_SP0, bit_reg.bit_count, SIZE(BitRegisters.bit_count)
	qba start_send
end_send:
	
stop:
	halt
	
     
