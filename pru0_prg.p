#include <pru.hp>
	
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

// Inputs are inverted	
#define QBB1 qbbc
#define QBB0 qbbs
	
#define DP1_TRANS QBB1 dp1_trans, DP
#define DP0_TRANS QBB0 dp0_trans, DP

#define DM1_TRANS QBB1 dm1_trans, DM
#define DM0_TRANS QBB0 dm0_trans, DM

	
	.struct BitRegisters
	.u32 bit_count
	.u32 shift_dp
	.u32 shift_dm
	.ends

	.assign BitRegisters, r2,r4, reg
	
#define DP R31.t14
#define DM R31.t15

	// Wait for one cycle
	.macro wait1
	add r0,r0,0
	.endm
	
	// Wait for other signal to switch
	.macro wait_skew
	wait1
	wait1
	wait1
	.endm
	
	// Send captured bits to PRU1
	// 3 cyles
	.macro send_bits
	qble send, reg.bit_count, 32
	wait1
	qba no_send
send:	
	xout XFR_SP0, reg, SIZE(BitRegisters)
	mov reg.bit_count, 0
no_send:	
	.endm
	
	.origin 0
	.entrypoint START
START:	
	// Enable OCP master port
	LBCO      r0, CONST_PRUCFG, 4, 4
	CLR     r0, r0, 4         // Clear SYSCFG[STANDBY_INIT] to enable OCP master port
	SBCO      r0, CONST_PRUCFG, 4, 4

	mov r0, PRU0_CTRL
	lbbo r1, r0, CONTROL, 4
	set r1, r1, 3 // Enable cycle counter
	sbbo r1, r0, CONTROL, 4
	
	mov reg.bit_count, 0

// ********* SE0 ***********	
SE0_trans_w: // 2 cycles after transition
	wait1
SE0_trans:
	
SE0_bit_loop: // 6 cycles after transition
	qbgt SE0_shift, reg.bit_count, 32
	wait1
	qba SE0_no_shift
SE0_shift:	
	clr reg.shift_dp,reg.bit_count
	clr reg.shift_dm,reg.bit_count
SE0_no_shift:	// 9 cycles after transition
	add reg.bit_count,reg.bit_count,1
	loop SE0_check_end, 5
	DP1_TRANS //11 cycles after transition
	DM1_TRANS
SE0_check_end:
	DP1_TRANS
	qba SE0_bit_loop

dp1_trans:
	wait_skew
	send_bits // 3 cycles		
	QBB1 SE1_trans_w, DM
	qba DIFF1_trans
	
dm1_trans:
	wait_skew
	send_bits // 3 cycles
	QBB1 SE1_trans_w, DP
	qba DIFF0_trans

// ********* SE1 ***********	
SE1_trans_w: // 2 cycles after transition
	wait1
SE1_trans:
	
SE1_bit_loop: // 6 cycles after transition
	qbgt SE1_shift, reg.bit_count, 32
	wait1
	qba SE1_no_shift
SE1_shift:	
	set reg.shift_dp,reg.bit_count
	set reg.shift_dm,reg.bit_count
SE1_no_shift:	// 9 cycles after transition
	add reg.bit_count,reg.bit_count,1
	loop SE1_check_end, 5
	DP0_TRANS //11 cycles after transition
	DM0_TRANS
SE1_check_end:
	DP0_TRANS
	qba SE1_bit_loop

dp0_trans:
	wait_skew
	send_bits
	QBB0 SE0_trans_w, DM
	qba DIFF0_trans
	
dm0_trans:
	wait_skew
	send_bits
	QBB0 SE0_trans_w, DP
	qba DIFF1_trans

// ********* DIFF0 ***********	
DIFF0_trans:
	
DIFF0_bit_loop: // 6 cycles after transition
	qbgt DIFF0_shift, reg.bit_count, 32
	wait1
	qba DIFF0_no_shift
DIFF0_shift:	
	clr reg.shift_dp,reg.bit_count
	set reg.shift_dm,reg.bit_count
DIFF0_no_shift:	// 9 cycles after transition
	add reg.bit_count,reg.bit_count,1
	loop DIFF0_check_end, 5
	DP1_TRANS //11 cycles after transition
	DM0_TRANS
DIFF0_check_end:
	DP1_TRANS
	qba DIFF0_bit_loop

// ********* DIFF1 ***********	
DIFF1_trans:
	
DIFF1_bit_loop: // 6 cycles after transition
	qbgt DIFF1_shift, reg.bit_count, 32
	wait1
	qba DIFF1_no_shift
DIFF1_shift:	
	set reg.shift_dp,reg.bit_count
	clr reg.shift_dm,reg.bit_count
DIFF1_no_shift:	// 9 cycles after transition
	add reg.bit_count,reg.bit_count,1
	loop DIFF1_check_end, 5
	DP0_TRANS //11 cycles after transition
	DM1_TRANS
DIFF1_check_end:
	DP0_TRANS
	qba DIFF1_bit_loop


stop:
	halt
	
     
