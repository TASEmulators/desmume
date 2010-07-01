@ armwrestler.asm
@ mic, 2004-2006 | micol972@gmail.com

@ Runs ARM instructions and checks for valid results. Useful for people developing ARM emulators
@ Assemble with arm-elf-as

.global main
.global forever

.equ VARBASE,	0x2200000
.equ TESTNUM,	(VARBASE+8)
.equ CURSEL,	(VARBASE+16)

.equ MENU, 	10

.equ BAD_Rd,	0x10
.equ BAD_Rn,	0x20


main:

mov	r0, #0x04000000			@ IME = 0;
add	r0, r0, #0x208
strh	r0, [r0]

ldr 	r0,=0x11340
mov 	r1,#0x4000000
str 	r0,[r1]

@ Turn on both screens
ldr 	r0,=0x830F
ldr 	r1,=0x4000304
str 	r0,[r1]

@ Map vram banks A and B to LCDC
mov 	r0,#0x80
ldr 	r1,=0x4000240
strb 	r0,[r1],#1
strb 	r0,[r1],#1

mov 	r0,#0		
ldr 	r1,=(VARBASE+0x10)
str 	r0,[r1],#-4		@ Menu item selection
str 	r0,[r1],#-4		@ Used in CheckKeys
mov 	r0,#MENU
str 	r0,[r1],#-4		@ The test number
mov 	r0,#2
str 	r0,[r1]

mov 	r10,#0

@ Set VRAM display mode (bank A)
mov 	r0,#0x20000
mov 	r1,#0x4000000
str 	r0,[r1]	

forever:
	bl	VSync

	mov 	r8,#16
	mov 	r9,#0

	@ Do we need to clear the screen?
	cmp 	r10,#1
	moveq 	r10,#0
	bleq 	ClearScreen
	
	@ Load the test number
	ldr 	r0,=TESTNUM
	ldr 	r1,[r0]
	
	@ Run it
	bl 	RunTest

	ldr 	r0,=TESTNUM
	ldr 	r1,[r0]
	cmp 	r1,#MENU
	beq 	handle_menu
		ldr 	r0,=szStart
		mov 	r1,#20+8
		mov 	r2,#180
		mov 	r3,#5
		bl 	DrawText	
		ldr 	r0,=szNext
		mov 	r1,#76+8
		mov 	r2,#180
		mov 	r3,#4
		bl 	DrawText	
		ldr 	r0,=szSelect2
		mov 	r1,#132+8
		mov 	r2,#180
		mov 	r3,#5
		bl 	DrawText	
		ldr 	r0,=szMenu
		mov 	r1,#192+8
		mov 	r2,#180
		mov 	r3,#4
		bl 	DrawText	

		bl 	CheckKeys

		tsts 	r2,#0x8
		beq 	not_start
			ldr 	r4,=TESTNUM
			ldrb 	r3,[r4]
			add 	r3,r3,#1
			cmp 	r3,#5
			movge 	r3,#MENU
			mov 	r10,#1
			strb 	r3,[r4]
		not_start:

		tsts 	r2,#0x4
		beq 	not_select
			ldr 	r4,=TESTNUM
			mov 	r10,#1
			mov 	r3,#MENU
			strb 	r3,[r4]
		not_select:
		
		b 	forever
		
	handle_menu:
		ldr 	r0,=szSelect
		mov 	r1,#4+4
		mov 	r2,#180
		mov 	r3,#4
		bl 	DrawText	
		ldr 	r0,=szStart2
		mov 	r1,#196+8
		mov 	r2,#180
		mov 	r3,#5
		bl 	DrawText	

		bl 	CheckKeys
		
		tsts 	r2,#0x8
		beq 	menu_not_start
			ldr 	r0,=menulinks
			ldr 	r4,=(VARBASE+0x10)
			ldrb	r3,[r4]
			sub 	r4,r4,#8
			add 	r0,r0,r3,lsl#2
			ldr 	r5,[r0]
			mov 	r10,#1
			strb 	r5,[r4]
			b 	forever
		menu_not_start:
		
		@ Check up/down
		mov 	r5,#0

		ldr 	r4,=(VARBASE+0x10)
		ldrb 	r3,[r4]
		mov 	r6,r3
		tsts 	r2,#0x40
		beq 	not_up
			subs 	r3,r3,#1
			movmi 	r3,#0
			movpl 	r5,#1
		not_up:
		tsts 	r2,#0x80
		beq 	not_down
			add 	r3,r3,#1
			cmp 	r3,#6
			movgt 	r3,#6
			movle 	r5,#1
		not_down:
		strb 	r3,[r4]

		cmp 	r5,#1
		bne no_erase
			ldr 	r0,=szSpace
			mov 	r1,#44+8
			mov 	r2,r6,lsl#3
			add 	r2,r2,#64+8
			mov 	r3,#5
			bl 	DrawText
		no_erase:
		
		b 	forever



VSync:
  stmfd sp!,{r0-r1}
  mov 	r0,#0x4000000
  add 	r0,r0,#4		@ DISPCNT+4 = DISPSTAT
  vs_loop1:
    	ldrh 	r1,[r0]
    	ands 	r1,r1,#1
  	bne 	vs_loop1
  vs_loop2:
    	ldrh 	r1,[r0]
    	ands 	r1,r1,#1
  	beq 	vs_loop2
  ldmfd sp!,{r0-r1}
  mov 	pc,lr



DrawText:
@ r0: lpszText
@ r1: x
@ r2: y
@ r3: color
	stmfd 	sp!,{r4-r10}

	ldr 	r10,=palette
	mov 	r3,r3,lsl#1
	ldrh 	r3,[r10,r3]
	
	mov 	r9,r2,lsl#9
	add 	r9,r9,r1,lsl#1
	add 	r9,r9,#0x6800000
	
dt_cloop:	
	ldr 	r10,=font
	ldrb 	r4,[r0],#1
	cmp 	r4,#0
	beq 	dt_null
	cmp 	r4,#32
	moveq 	r4,#0
	subne 	r4,r4,#37
	add 	r10,r10,r4,lsl#6
	
	mov 	r7,r9
	add 	r9,r9,#16
	mov 	r5,#8
dt_vloop:
	mov 	r6,#8
dt_hloop:
	ldrb 	r1,[r10],#1
	@mov 	r2,r1,lsr#8
	ands 	r1,r1,#0xFF 
	movne 	r1,r3
	@ands 	r2,r2,#0xFF
	@movne 	r2,r3
	@orr 	r1,r1,r2,lsl#8
	strh 	r1,[r7],#2
	subs 	r6,r6,#1
	bne 	dt_hloop
	
	add 	r7,r7,#496
	subs 	r5,r5,#1
	bne 	dt_vloop
	
	b 	dt_cloop
dt_null:
	ldmfd 	sp!,{r4-r10}
	mov 	pc,lr

.pool
.align


DrawHex:
@ r0: value
@ r1: x
@ r2: y
@ r3: color
	stmfd 	sp!,{r4-r10}

	ldr 	r10,=palette
	mov 	r3,r3,lsl#1
	ldrh 	r3,[r10,r3]
	
	mov 	r9,r2,lsl#9
	add 	r9,r9,r1,lsl#1
	add 	r9,r9,#0x6800000
	mov 	r8,#8
	
dh_cloop:	
	ldr 	r10,=font
	mov 	r0,r0,ror#28
	and 	r4,r0,#0xF
	cmp	r4,#9
	add 	r4,r4,#11
	addhi	r4,r4,#7	
	add 	r10,r10,r4,lsl#6
	
	mov 	r7,r9
	add 	r9,r9,#16
	mov 	r5,#8
dh_vloop:
	mov 	r6,#8
dh_hloop:
	ldrb 	r1,[r10],#1
	ands 	r1,r1,#0xFF 
	movne 	r1,r3
	strh 	r1,[r7],#2
	subs 	r6,r6,#1
	bne 	dh_hloop
	
	add 	r7,r7,#496
	subs 	r5,r5,#1
	bne 	dh_vloop
	
	subs 	r8,r8,#1
	bne 	dh_cloop
	ldmfd 	sp!,{r4-r10}
	mov 	pc,lr
.pool
.align


DrawResult:
@ r0: lpszText
@ r1: bitmask
	stmfd 	sp!,{r0-r5,lr}

	mov 	r4,r1
	mov 	r5,r2
	
	mov 	r1,#16
	mov 	r2,r8
	mov 	r3,#3
	bl 	DrawText
	
	mov 	r2,r5

	mov 	r5,#72

	tsts 	r4,#0x80000000
	beq 	dt_not_ldr
		ldr 	r0,=szLDRtype
		add 	r0,r0,r2,lsl#2
		add 	r0,r0,r2
		mov 	r1,#64
		mov 	r2,r8
		mov 	r3,#3
		bl 	DrawText
		mov 	r5,#104
	dt_not_ldr:

	tsts 	r4,#0x40000000
	beq 	dt_not_ldm
		ldr 	r0,=szLDMtype
		add 	r0,r0,r2,lsl#2
		add 	r0,r0,r2
		mov 	r1,#40
		mov 	r2,r8
		mov 	r3,#3
		bl 	DrawText
		mov 	r5,#80
	dt_not_ldm:


	tsts 	r4,#0xFF
	beq 	dr_ok

	ldr 	r0,=szBad
	mov 	r1,r5
	mov 	r2,r8
	mov 	r3,#2
	bl 	DrawText
	add 	r5,r5,#32

	tsts 	r4,#0x40000000
	bne 	skip_flags
	
	tsts 	r4,#1
	beq 	dr_c_ok
	ldr 	r0,=szC
	mov 	r1,r5
	mov 	r2,r8
	mov 	r3,#2
	bl 	DrawText
	add 	r5,r5,#8
dr_c_ok:

	tsts 	r4,#2
	beq 	dr_n_ok
	ldr 	r0,=szN
	mov 	r1,r5
	mov 	r2,r8
	mov 	r3,#2
	bl 	DrawText
	add 	r5,r5,#8
dr_n_ok:

	tsts 	r4,#4
	beq 	dr_v_ok
	ldr 	r0,=szV
	mov 	r1,r5
	mov 	r2,r8
	mov 	r3,#2
	bl 	DrawText
	add 	r5,r5,#8
dr_v_ok:

	tsts 	r4,#8
	beq 	dr_z_ok
	ldr 	r0,=szZ
	mov 	r1,r5
	mov 	r2,r8
	mov 	r3,#2
	bl 	DrawText
	add 	r5,r5,#16
dr_z_ok:

	tsts 	r4,#0x40
	beq 	dr_q_ok
	ldr 	r0,=szQ
	mov 	r1,r5
	mov 	r2,r8
	mov 	r3,#2
	bl 	DrawText
	add 	r5,r5,#16
dr_q_ok:

skip_flags:
	tsts 	r4,#BAD_Rd
	beq 	dr_rd_ok
	ldr 	r0,=szRd
	mov 	r1,r5
	mov 	r2,r8
	mov 	r3,#2
	bl 	DrawText
	add 	r5,r5,#16
dr_rd_ok:

	tsts 	r4,#0x80
	beq 	dr_mem_ok
	ldr 	r0,=szMem
	mov 	r1,r5
	mov 	r2,r8
	mov 	r3,#2
	bl 	DrawText
	add 	r5,r5,#24
	ldr 	r0,=memVal
	ldr 	r0,[r0]
	mov 	r1,r5
	add 	r1,r1,#32
	mov 	r2,r8
	mov 	r3,#2
	bl 	DrawHex
	b 	dr_done
dr_mem_ok:

	tsts 	r4,#BAD_Rn
	beq 	dr_rn_ok
	ldr 	r0,=szRn
	mov 	r1,r5
	mov 	r2,r8
	mov 	r3,#2
	bl 	DrawText
	ldr 	r0,=rnVal
	ldr 	r0,[r0]
	mov 	r1,r5
	add 	r1,r1,#32
	mov 	r2,r8
	mov 	r3,#2
	bl 	DrawHex
dr_rn_ok:
	
	b 	dr_done
dr_ok:
	ldr 	r0,=szOK
	mov 	r1,r5
	mov 	r2,r8
	mov 	r3,#1
	bl 	DrawText

dr_done:
	ldmfd 	sp!,{r0-r5,lr}
	mov 	pc,lr
	
.pool
.align



ClearScreen:	
	mov 	r0,#0x6800000
	mov 	r4,#0
	ldr 	r5,=24576
	cs_repeat:
		str 	r4,[r0],#4
		subs 	r5,r5,#1
		bne 	cs_repeat
	mov 	pc,lr
	

CheckKeys:
	mov 	r0,#0x4000000
	add 	r0,r0,#0x130
	ldrh 	r2,[r0]
	ldr 	r0,=(VARBASE+12)
	ldrh 	r3,[r0]
	and 	r2,r2,#0xff

	eor 	r2,r2,#0xff
	strh 	r2,[r0]
	cmp 	r2,#0 
	eorne 	r2,r2,r3
	mov 	pc,lr
.pool
.align



RunTest:
@ r1: test number
	mov 	r1,r1,lsl#2	@ r1 *= sizeof(word)
	ldr 	r0,=jumptable
	add 	r0,r0,r1
	ldr 	r1,[r0]		@ r1 = jumptable[test_number]
	mov 	pc,r1		@ Branch to the code
	


Menu:
	stmfd 	sp!,{lr}

	ldr 	r0,=szAsterixes
	mov 	r1,#0
	mov 	r2,#0
	mov 	r3,#4
	bl 	DrawText
	ldr 	r0,=szAster2
	mov 	r1,#0
	mov 	r2,#8
	mov 	r3,#4
	bl 	DrawText
	ldr 	r0,=szArmwrestler
	mov 	r1,#0
	mov 	r2,#16
	mov 	r3,#4
	bl 	DrawText
	ldr 	r0,=szAster2
	mov 	r1,#0
	mov 	r2,#24
	mov 	r3,#4
	bl 	DrawText
	ldr 	r0,=szAuthor
	mov 	r1,#0
	mov 	r2,#32
	mov 	r3,#4
	bl 	DrawText
	ldr 	r0,=szAster2
	mov 	r1,#0
	mov 	r2,#40
	mov 	r3,#4
	bl 	DrawText
	ldr 	r0,=szAsterixes
	mov 	r1,#0
	mov 	r2,#48
	mov 	r3,#4
	bl 	DrawText


	ldr 	r5,=menuitems
	mov 	r4,#64+8
	draw_menuitems:
		mov 	r1,#56+8
		mov 	r2,r4
		mov 	r3,#3
		ldr 	r0,[r5],#4
		bl 	DrawText
		add 	r4,r4,#8
		cmp 	r4,#128
		bne 	draw_menuitems

	ldr 	r0,=szMarker
	mov 	r1,#44+8
	ldr 	r4,=CURSEL
	ldrb	r2,[r4]

	mov 	r2,r2,lsl#3
	add 	r2,r2,#64+8
	mov 	r3,#5
	bl 	DrawText
		
	ldmfd 	sp!,{lr}
	mov 	pc,lr
	
	

Test0:
	stmfd sp!,{lr}
	
	ldr r0,=szALU1
	mov r1,#56
	mov r2,#1
	mov r3,#4
	bl DrawText
	

	@ ADC
	mov 	r1,#0
	mov 	r2,#0x80000000
	mov 	r3,#0xF
	adds 	r9,r9,r9	@ clear carry
	adcs 	r2,r2,r3
	orrcs 	r1,r1,#1
	orrpl 	r1,r1,#2
	orrvs 	r1,r1,#4
	orreq 	r1,r1,#8
	adcs 	r2,r2,r2	
	orrcc 	r1,r1,#1
	orrmi 	r1,r1,#2
	adc 	r3,r3,r3
	cmp 	r3,#0x1F
	orrne 	r1,r1,#BAD_Rd
	
	adds 	r9,r9,r9	@ clear carry
	mov 	r0,#0
	mov 	r2,#1
	adc 	r0,r0,r2,lsr#1
	cmp 	r0,#1
	@orrne 	r1,r1,#BAD_Rd
	
	ldr 	r0,=szADC
	bl 	DrawResult
	add 	r8,r8,#8

	@ ADD
	mov 	r1,#0
	ldr 	r2,=0xFFFFFFFE
	mov 	r3,#1
	adds 	r2,r2,r3
	orrcs 	r1,r1,#1
	orrpl 	r1,r1,#2
	orrvs 	r1,r1,#4
	orreq 	r1,r1,#8
	adds 	r2,r2,r3	
	orrcc 	r1,r1,#1
	orrmi 	r1,r1,#2
	orrvs 	r1,r1,#4

	orrne 	r1,r1,#8
	ldr 	r0,=szADD
	bl 	DrawResult
	add 	r8,r8,#8

	

	@ AND
	mov 	r1,#0
	mov 	r2,#2
	mov 	r3,#5
	ands 	r2,r2,r3,lsr#1
	orrcc 	r1,r1,#1
	orreq 	r1,r1,#8
	cmp 	r2,#2
	orrne 	r1,r1,#BAD_Rd
	mov 	r2,#0xC00
	mov 	r3,r2

	mov 	r4,#0x80000000
	ands 	r2,r2,r4,asr#32
	orrcc 	r1,r1,#1
	orrmi 	r1,r1,#2
	orreq 	r1,r1,#8
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=szAND
	bl 	DrawResult
	add 	r8,r8,#8


	@ BIC
	mov 	r1,#0
	adds 	r9,r9,r9 @ clear carry
	ldr 	r2,=0xFFFFFFFF
	ldr 	r3,=0xC000000D
	bics 	r2,r2,r3,asr#1
	orrcc 	r1,r1,#1
	orrmi 	r1,r1,#2	
	orreq 	r1,r1,#8
	ldr 	r3,=0x1FFFFFF9
	cmp 	r2,r3
	orrne 	r1,r1,#16
	ldr 	r0,=szBIC
	bl 	DrawResult
	add 	r8,r8,#8
	
	@ CMN
	mov 	r1,#0
	adds 	r9,r9,r9 @ clear carry
	ldr 	r2,=0x7FFFFFFF
	ldr 	r3,=0x70000000
	cmns 	r2,r3
	orrcs 	r1,r1,#1
	orrpl 	r1,r1,#2
	orrvc 	r1,r1,#4
	orreq 	r1,r1,#8
	ldr 	r3,=0x7FFFFFFF
	cmp 	r2,r3
	orrne 	r1,r1,#16
	ldr 	r0,=szCMN
	bl 	DrawResult
	add 	r8,r8,#8

	
	@ EOR
	mov 	r1,#0
	mov 	r2,#1
	mov 	r3,#3
	eors 	r2,r2,r3,lsl#31
	eors 	r2,r2,r3,lsl#0
	orrcc 	r1,r1,#1
	orrpl 	r1,r1,#2
	orreq 	r1,r1,#8
	ldr 	r4,=0x80000002
	cmp 	r4,r2
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=szEOR
	bl 	DrawResult
	add 	r8,r8,#8

	
	@ MOV
	mov 	r1,#0
	ldr 	r2,=labelone
	mov 	r3,r15
	cmp 	r2,r3
labelone:
	orrne 	r1,r1,#BAD_Rd
	ldr 	r2,=labeltwo
	mov 	r3,#0
	movs 	r4,r15,lsl r3	@ 0
	orreq 	r1,r1,#8	@ 4
	cmp 	r4,r2		@ 8
labeltwo: 			@ 12	
	orrne 	r1,r1,#BAD_Rd
	ldr 	r2,=0x80000001
	movs 	r3,r2,lsr#32
	orrcc 	r1,r1,#1
	orrmi 	r1,r1,#2
	
	orrne 	r1,r1,#8
	cmp 	r3,#0
	orrne 	r1,r1,#BAD_Rd

	@ Test ASR by reg==0
	mov 	r3,#3
	movs 	r4,r3,lsr#1	@ set carry 	
	mov 	r2,#0
	movs 	r3,r4,asr r2
	orrcc 	r1,r1,#1
	cmp 	r3,#1
	orrne 	r1,r1,#16

	@ Test ASR by reg==33
	ldr 	r2,=0x80000000
	mov 	r3,#33
	movs 	r2,r2,asr r3
	orrcc 	r1,r1,#1
	ldr 	r4,=0xFFFFFFFF
	cmp 	r2,r4
	orrne 	r1,r1,#16
	
	ldr 	r0,=szMOV
	bl 	DrawResult
	add 	r8,r8,#8


	@ MVN
	mov 	r1,#0
	ldr 	r2,=labelthree	
	ldr 	r3,=0xFFFFFFFF
	eor 	r2,r2,r3
	mvn 	r3,r15
	cmp 	r3,r2
labelthree:	
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=szMVN
	bl 	DrawResult
	add 	r8,r8,#8
	
	@ ORR
	mov 	r1,#0
	mov 	r2,#2
	mov 	r3,#3
	movs 	r4,r3,lsr#1	@ set carry 
	orrs 	r3,r3,r2,rrx
	orrcs 	r1,r1,#1
	orrpl 	r1,r1,#2
	orreq 	r1,r1,#8
	ldr 	r4,=0x80000003
	cmp 	r4,r3
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=szORR
	bl 	DrawResult
	add 	r8,r8,#8
	
	
	@ RSC
	mov 	r1,#0
	mov 	r2,#2
	mov 	r3,#3
	adds 	r9,r9,r9	@ clear carry
	rscs 	r3,r2,r3
	orrcc 	r1,r1,#1
	orrmi 	r1,r1,#2
	orrne 	r1,r1,#8
	cmp 	r2,#2
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=szRSC
	bl 	DrawResult
	add 	r8,r8,#8

	
	@ SBC
	mov 	r1,#0
	ldr 	r2,=0xFFFFFFFF
	adds 	r3,r2,r2	@ set carry
	sbcs 	r2,r2,r2
	orrcc 	r1,r1,#1
	orrmi 	r1,r1,#2
	orrne 	r1,r1,#8
	adds 	r9,r9,r9	@ clear carry
	sbcs 	r2,r2,#0
	orreq 	r1,r1,#8
	orrcs 	r1,r1,#1
	orrpl 	r1,r1,#2
	ldr 	r0,=szSBC
	bl 	DrawResult
	add 	r8,r8,#8
	

	@ MLA
	mov 	r1,#0
	ldr 	r2,=0xFFFFFFF6
	mov 	r3,#0x14
	ldr 	r4,=0xD0
	mlas 	r2,r3,r2,r4
	orrmi 	r1,r1,#2
	orreq 	r1,r1,#8
	cmp 	r2,#8
	orrne 	r1,r1,#16
	ldr 	r0,=szMLA
	bl 	DrawResult
	add 	r8,r8,#8


	@ MUL
	mov 	r1,#0
	ldr 	r2,=0xFFFFFFF6
	mov 	r3,#0x14
	ldr 	r4,=0xFFFFFF38
	muls 	r2,r3,r2
	orrpl 	r1,r1,#2
	orreq 	r1,r1,#8
	cmp 	r2,r4
	orrne 	r1,r1,#16
	ldr 	r0,=szMUL
	bl 	DrawResult
	add 	r8,r8,#8

	
	@ UMULL
	mov 	r1,#0
	ldr 	r2,=0x80000000
	mov 	r3,#8
	umulls 	r4,r5,r2,r3
	orrmi 	r1,r1,#2
	orreq 	r1,r1,#8
	mov 	r2,#4
	cmp 	r2,r5
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=szUMULL
	bl 	DrawResult
	add 	r8,r8,#8

	@ SMULL
	mov 	r1,#0
	ldr 	r2,=0x80000000
	mov 	r3,#8
	smulls 	r4,r5,r2,r3
	orrpl 	r1,r1,#2
	orreq 	r1,r1,#8
	ldr 	r2,=0xFFFFFFFC
	cmp 	r2,r5
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=szSMULL
	bl 	DrawResult
	add 	r8,r8,#8
	
	ldmfd 	sp!,{lr}
	mov 	pc,lr
.pool
.align

	
Test1:
	stmfd 	sp!,{lr}
	
	ldr 	r0,=szALU2
	mov 	r1,#60
	mov 	r2,#1
	mov 	r3,#4
	bl 	DrawText

	
	mov 	r1,#0
	ldr 	r2,=0x80000000
	mov 	r3,#8
	mov 	r5,#1
	mov 	r4,#2
	umlals 	r4,r5,r2,r3
	orrmi 	r1,r1,#2
	orreq 	r1,r1,#8
	cmp 	r4,#2
	orrne 	r1,r1,#BAD_Rd
	cmp 	r5,#5	
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=szUMLAL
	bl 	DrawResult
	add 	r8,r8,#8

	@ SMLAL
	mov 	r1,#0
	ldr 	r2,=0x80000001
	mov 	r3,#8
	mov 	r5,#5
	ldr 	r4,=0xfffffff8
	smlals 	r4,r5,r2,r3
	orrmi 	r1,r1,#2
	orreq 	r1,r1,#8
	cmp 	r5,#2
	orrne 	r1,r1,#BAD_Rd
	cmp 	r4,#0
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=szSMLAL
	bl 	DrawResult
	add 	r8,r8,#8

	@ SWP
	mov 	r1,#0
	adds 	r1,r1,#1		@ Clear C,N,V,Z
	mov 	r1,#0
	ldr 	r5,=(VARBASE+0x100)
	str 	r1,[r5]
	mov 	r0,#0xC0000000
	swp 	r0,r0,[r5]
	orrcs 	r1,r1,#1
	orrmi 	r1,r1,#2
	orrvs 	r1,r1,#4
	orreq 	r1,r1,#8
	cmp 	r0,#0
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r5]
	cmp 	r0,#0xC0000000
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=szSWP
	bl 	DrawResult
	add 	r8,r8,#8
	
	@ SWPB
	mov 	r1,#0
	adds 	r1,r1,#0		@ Clear C,N,V
	ldr 	r5,=(VARBASE+0x100)
	mov 	r4,#0xff
	add 	r4,r4,#0x80
	str 	r4,[r5]
	mov 	r0,#0xC0000000
	orr 	r0,r0,#0x80
	swpb 	r0,r0,[r5]
	orrcs 	r1,r1,#1
	orrmi 	r1,r1,#2
	orrvs 	r1,r1,#4
	orrne 	r1,r1,#8
	cmp 	r0,#0x7f
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r5]
	cmp 	r0,#0x180
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=szSWPB
	bl 	DrawResult
	add 	r8,r8,#8

	
	@ MRS
	mov 	r1,#0
	mov 	r0,#0xC0000000
	adds 	r0,r0,r0		@ Z=0, C=1, V=0, N=1
	mov 	r2,#0x50000000
	mrs 	r2,cpsr
	tsts 	r2,#0x20000000
	orreq 	r1,r1,#1
	tsts 	r2,#0x80000000
	orreq 	r1,r1,#2
	tsts 	r2,#0x10000000
	orrne 	r1,r1,#4
	tsts 	r2,#0x40000000
	orrne 	r1,r1,#8
	ldr 	r0,=szMRS
	bl 	DrawResult
	add 	r8,r8,#8
	
	
	@ MSR
	mov 	r1,#0
	movs 	r2,#0
	msr 	cpsr_flg,#0x90000000
	orrcs 	r1,r1,#1
	orrpl 	r1,r1,#2
	orrvc 	r1,r1,#4
	orreq 	r1,r1,#8
	mov 	r11,#1
	mrs 	r2,cpsr
	bic 	r2,r2,#0x1f
	orr 	r2,r2,#0x11	
	msr 	cpsr,r2		@ Set FIQ mode
	mov 	r11,#2
	orr 	r2,r2,#0x1f
	msr 	cpsr,r2		@ Set System mode
	cmp 	r11,#1
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=szMSR
	bl 	DrawResult
	add 	r8,r8,#8
	
	
	ldmfd 	sp!,{lr}
	mov 	pc,lr	
.pool
.align



Test2:
	stmfd 	sp!,{lr}

	ldr 	r0,=szLS1
	mov 	r1,#52
	mov 	r2,#1
	mov 	r3,#4
	bl 	DrawText


	@ LDR
	
	@ +#]
	mov 	r1,#0
	ldr 	r0,=romvar
	sub 	r2,r0,#3
	mov 	r3,r2
	ldr 	r0,[r0,#0]
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r2,#3]
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	@ Test non word-aligned load
	ldr 	r0,=romvar2
	ldr 	r0,[r0,#1]
	ldr 	r2,=0x00ff008f
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=romvar2
	ldr 	r0,[r0,#2]
	ldr 	r2,=0x8f00ff00
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=romvar2
	ldr 	r0,[r0,#3]
	ldr 	r2,=0x008f00ff
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd

	mov 	r2,#0
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDR
	bl 	DrawResult
	add 	r8,r8,#8


	@ -#]
	mov 	r1,#0
	ldr 	r0,=romvar
	mov 	r2,r0
	mov 	r3,r2
	add 	r0,r0,#206
	ldr 	r0,[r0,#-206]
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r2,#-0]
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	@ Test non word-aligned load
	ldr 	r0,=romvar2+4
	ldr 	r0,[r0,#-2]
	ldr 	r2,=0x8f00ff00
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd

	mov 	r2,#1
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDR
	bl 	DrawResult
	add 	r8,r8,#8


	@ +#]!
	mov 	r1,#0
	ldr 	r0,=romvar
	sub 	r2,r0,#3
	mov 	r3,r0
	ldr 	r0,[r0,#0]!
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r2,#3]!
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	@ Test non word-aligned load
	ldr 	r0,=romvar2
	ldr 	r0,[r0,#2]!
	ldr 	r2,=0x8f00ff00
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd
	
	mov 	r2,#2
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDR
	bl 	DrawResult
	add 	r8,r8,#8


	@ -#]!
	mov 	r1,#0
	ldr 	r0,=romvar
	add 	r2,r0,#1
	mov 	r3,r0
	add 	r0,r0,#206
	ldr 	r0,[r0,#-206]!
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r2,#-1]!
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	@ Test non word-aligned load
	ldr 	r0,=romvar2+4
	ldr 	r0,[r0,#-2]!
	ldr 	r2,=0x8f00ff00
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd
	
	mov 	r2,#3
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDR
	bl 	DrawResult
	add 	r8,r8,#8


	@ +R]
	mov 	r1,#0
	ldr 	r0,=romvar
	sub 	r2,r0,#8
	sub 	r0,r0,#1
	mov 	r3,r2
	mov 	r4,#2
	ldr 	r0,[r0,r4, lsr #1]
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r2,r4, lsl #2]
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn

	ldr 	r2,=romvar
	mov 	r2,r2,lsr#1
	mov 	r3,#0xC0000000
	ldr 	r0,[r2,r2]
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd

	ldr 	r2,=romvar
	mov 	r3,#0x8
	ldr 	r0,[r2,r3, lsr #32]
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	
	ldr 	r2,=romvar
	add 	r2,r2,#1
	mov 	r3,#0xC0000000
	ldr 	r0,[r2,r3, asr #32]
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd

	ldr 	r2,=romvar
	add 	r2,r2,#2
	ldr 	r3,=0xfffffffc
	adds 	r4,r3,r3		@ set carry
	ldr 	r0,[r2,r3, rrx]
	orrcc 	r1,r1,#1
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd

	@ Test non word-aligned load
	ldr 	r0,=romvar2
	mov 	r2,#2
	ldr 	r0,[r0,r2]
	ldr 	r2,=0x8f00ff00
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd
	
	mov 	r2,#4
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDR
	bl 	DrawResult
	add 	r8,r8,#8


	@ -R]
	mov 	r1,#0
	ldr 	r0,=romvar
	add 	r2,r0,#8
	add 	r0,r0,#1
	mov 	r3,r2
	mov 	r4,#2
	ldr 	r0,[r0,-r4, lsr #1]
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r2,-r4, lsl #2]
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn

	ldr 	r2,=romvar
	mov 	r3,#0x8
	ldr 	r0,[r2,-r3, lsr #32]
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd

	ldr 	r2,=romvar
	sub 	r2,r2,#1
	mov 	r3,#0x80000000
	ldr 	r0,[r2,-r3, asr #32]
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	
	ldr 	r2,=romvar
	sub 	r2,r2,#4
	ldr 	r3,=0xfffffff8
	adds 	r4,r3,r3		@ set carry
	ldr 	r0,[r2,-r3, rrx]
	orrcc 	r1,r1,#1
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd

	@ Test non word-aligned load
	ldr 	r0,=romvar2+4
	mov 	r2,#1
	ldr 	r0,[r0,-r2, lsl #1]
	ldr 	r2,=0x8f00ff00
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd
	
	mov 	r2,#5
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDR
	bl 	DrawResult
	add 	r8,r8,#8
	

	@ +R]!
	mov 	r1,#0
	ldr 	r0,=romvar
	mov 	r3,r0
	sub 	r2,r0,#8
	sub 	r0,r0,#1
	mov 	r4,#2
	ldr 	r0,[r0,r4, lsr #1]!
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r2,r4, lsl #2]!
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn

	ldr 	r2,=romvar
	mov 	r4,r2
	mov 	r2,r2,lsr#1
	mov 	r3,#0xC0000000
	ldr 	r0,[r2,r2]!
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r4
	orrne 	r1,r1,#BAD_Rn
	
	ldr 	r2,=romvar
	mov 	r4,r2
	add 	r2,r2,#1
	mov 	r3,#0xC0000000
	ldr 	r0,[r2,r3, asr #32]!
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r4
	orrne 	r1,r1,#BAD_Rn

	ldr 	r2,=romvar
	mov 	r5,r2
	add 	r2,r2,#2
	ldr 	r3,=0xfffffffc
	adds 	r4,r3,r3		@ set carry
	ldr 	r0,[r2,r3, rrx]!
	orrcc 	r1,r1,#1
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r5
	orrne 	r1,r1,#BAD_Rn

	@ Test non word-aligned load
	ldr 	r0,=romvar2
	mov 	r2,#2
	ldr 	r0,[r0,r2]!
	ldr 	r2,=0x8f00ff00
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd
	
	mov 	r2,#6
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDR
	bl 	DrawResult
	add 	r8,r8,#8


	@ -R]!
	mov 	r1,#0
	ldr 	r0,=romvar
	mov 	r3,r0
	add 	r2,r0,#8
	add 	r0,r0,#1
	mov 	r4,#2
	ldr 	r0,[r0,-r4, lsr #1]!
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r2,-r4, lsl #2]!
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn

	ldr 	r2,=romvar
	mov 	r4,r2
	sub 	r2,r2,#1
	mov 	r3,#0x80000000
	ldr 	r0,[r2,-r3, asr #32]!
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r4
	orrne 	r1,r1,#BAD_Rn
	
	ldr 	r2,=romvar
	mov 	r5,r2
	sub 	r2,r2,#4
	ldr 	r3,=0xfffffff8
	adds 	r4,r3,r3		@ set carry
	ldr 	r0,[r2,-r3, rrx]!
	orrcc 	r1,r1,#1
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r5
	orrne 	r1,r1,#BAD_Rn

	@ Test non word-aligned load
	ldr 	r0,=romvar2+4
	mov 	r2,#2
	ldr 	r0,[r0,-r2]!
	ldr 	r2,=0x8f00ff00
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd
	
	mov 	r2,#7
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDR
	bl 	DrawResult
	add 	r8,r8,#8
	

	@ ]+#
	mov 	r1,#0
	ldr 	r0,=romvar
	add 	r3,r0,#3
	mov 	r2,r0
	ldr 	r0,[r0],#3
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r2],#3
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	@ Test non word-aligned load
	ldr 	r0,=romvar2+2
	ldr 	r0,[r0],#5
	ldr 	r2,=0x8f00ff00
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd
	mov 	r2,#8
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDR
	bl 	DrawResult
	add 	r8,r8,#8

	@ ]-#
	mov 	r1,#0
	ldr 	r0,=romvar
	mov 	r2,r0
	sub 	r3,r0,#0xff
	ldr 	r0,[r0],#-0xff
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r2],#-0xff
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	@ Test non word-aligned load
	ldr 	r0,=romvar2+2
	ldr 	r0,[r0],#-5
	ldr 	r2,=0x8f00ff00
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd
	mov 	r2,#9
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDR
	bl 	DrawResult
	add 	r8,r8,#8


	@ ]+R
	mov 	r1,#0
	ldr 	r0,=romvar
	mov 	r2,r0
	add 	r5,r0,#8
	mov 	r3,r0
	mov 	r4,#2
	ldr 	r0,[r0],r4, lsr #1
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r2],r4, lsl #2
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r5
	orrne 	r1,r1,#BAD_Rn

	ldr 	r2,=romvar
	mov 	r0,#123
	add 	r3,r2,r0
	ldr 	r0,[r2],r0
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	
	ldr 	r2,=romvar
	sub 	r4,r2,#1
	mov 	r3,#0xC0000000
	ldr 	r0,[r2],r3, asr #32
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r4
	orrne 	r1,r1,#BAD_Rn

	ldr 	r2,=romvar
	sub 	r4,r2,#2
	ldr 	r3,=0xfffffffc
	adds 	r5,r3,r3		@ set carry
	ldr 	r0,[r2],r3, rrx
	orrcc 	r1,r1,#1
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r4
	orrne 	r1,r1,#BAD_Rn

	@ Test non word-aligned load
	ldr 	r0,=romvar2+2
	mov 	r2,#1
	ldr 	r0,[r0],r2
	ldr 	r2,=0x8f00ff00
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd
	
	mov 	r2,#10
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDR
	bl 	DrawResult
	add 	r8,r8,#8


	@ ]-R
	mov 	r1,#0
	ldr 	r0,=romvar
	mov 	r2,r0
	sub 	r5,r0,#16
	mov 	r3,r0
	mov 	r4,#2
	ldr 	r0,[r0],-r4, lsr #1
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,[r2],-r4, lsl #3
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r5
	orrne 	r1,r1,#BAD_Rn

	ldr	r2,=romvar
	mov 	r0,#123
	sub 	r3,r2,r0
	ldr 	r0,[r2],-r0
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	
	ldr 	r2,=romvar
	add 	r4,r2,#1
	mov 	r3,#0xC0000000
	ldr 	r0,[r2],-r3, asr #32
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r4
	orrne 	r1,r1,#BAD_Rn

	ldr 	r2,=romvar
	add 	r4,r2,#2
	ldr 	r3,=0xfffffffc
	adds 	r5,r3,r3		@ set carry
	ldr 	r0,[r2],-r3, rrx
	orrcc 	r1,r1,#1
	cmp 	r0,#0x80
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r4
	orrne 	r1,r1,#BAD_Rn

	@ Test non word-aligned load
	ldr 	r0,=romvar2+2
	mov 	r2,#5
	ldr 	r0,[r0],-r2
	ldr 	r2,=0x8f00ff00
	cmp 	r0,r2
	orrne 	r1,r1,#BAD_Rd
	
	mov 	r2,#11
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDR
	bl 	DrawResult
	add 	r8,r8,#8
	
	
	
	@ LDRH
	
	@ +#]
	mov 	r1,#0
	ldr 	r0,=romvar2
	sub 	r0,r0,#1
	sub 	r2,r0,#3
	mov 	r3,r2
	ldrh 	r0,[r0,#1]
	ldr 	r5,=0x00008f00
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	ldrh 	r0,[r2,#4]
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#0
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRH
	bl 	DrawResult
	add 	r8,r8,#8

	@ -#]
	mov 	r1,#0
	ldr 	r0,=romvar2
	add 	r0,r0,#1
	add 	r2,r0,#3
	mov 	r3,r2
	ldrh 	r0,[r0,#-1]
	ldr 	r5,=0x00008f00
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	ldrh 	r0,[r2,#-4]
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#1
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRH
	bl 	DrawResult
	add 	r8,r8,#8

	@ +#]!
	mov 	r1,#0
	ldr 	r0,=romvar2
	mov 	r3,r0
	sub 	r0,r0,#1
	sub 	r2,r0,#3
	ldrh 	r0,[r0,#1]!
	ldr 	r5,=0x00008f00
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	ldrh 	r0,[r2,#4]!
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#2
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRH
	bl 	DrawResult
	add 	r8,r8,#8

	@ -#]!
	mov 	r1,#0
	ldr 	r0,=romvar2
	mov 	r3,r0
	add 	r0,r0,#1
	add 	r2,r0,#3
	ldrh 	r0,[r0,#-1]!
	ldr 	r5,=0x00008f00
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	ldrh 	r0,[r2,#-4]!
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#3
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRH
	bl 	DrawResult
	add 	r8,r8,#8
	
	ldmfd 	sp!,{lr}
	mov 	pc,lr
.pool
.align



Test3:
	stmfd 	sp!,{lr}

	ldr 	r0,=szLS2
	mov 	r1,#52
	mov 	r2,#1
	mov 	r3,#4
	bl 	DrawText


	@ LDRH
	
	@ +#]
	mov 	r1,#0
	ldr 	r0,=romvar2
	sub 	r0,r0,#1
	sub 	r2,r0,#3
	mov 	r3,r2
	mov 	r4,#1
	ldrh 	r0,[r0,r4]
	ldr 	r5,=0x00008f00
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	mov 	r4,#4
	ldrh 	r0,[r2,r4]
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#4
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRH
	bl 	DrawResult
	add 	r8,r8,#8

	@ -#]
	mov 	r1,#0
	ldr 	r0,=romvar2
	add 	r0,r0,#1
	add 	r2,r0,#3
	mov 	r3,r2
	mov 	r4,#1
	ldrh 	r0,[r0,-r4]
	ldr 	r5,=0x00008f00
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	mov 	r4,#4
	ldrh 	r0,[r2,-r4]
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#5
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRH
	bl 	DrawResult
	add 	r8,r8,#8
	
	
	@ LDRB
	
	@ +#]
	mov 	r1,#0
	ldr 	r0,=romvar2
	sub 	r2,r0,#1
	mov 	r3,r2
	ldrb 	r0,[r0,#3]
	cmp 	r0,#0xff
	orrne 	r1,r1,#BAD_Rd
	ldrb 	r0,[r2,#3]
	cmp 	r0,#0
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#0
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRB
	bl 	DrawResult
	add 	r8,r8,#8

	@ -#]
	mov 	r1,#0
	ldr 	r0,=romvar2
	add 	r0,r0,#4
	add 	r2,r0,#1
	mov 	r3,r2
	ldrb 	r0,[r0,#-1]
	cmp 	r0,#0xff
	orrne 	r1,r1,#BAD_Rd
	ldrb 	r0,[r2,#-3]
	cmp 	r0,#0
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#1
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRB
	bl 	DrawResult
	add 	r8,r8,#8

	@ +#]!
	mov 	r1,#0
	ldr 	r0,=romvar2
	add 	r3,r0,#2
	sub 	r2,r0,#3
	ldrb 	r0,[r0,#3]!
	cmp 	r0,#0xff
	orrne 	r1,r1,#BAD_Rd
	ldrb 	r0,[r2,#5]!
	cmp 	r0,#0
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#2
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRB
	bl 	DrawResult
	add 	r8,r8,#8

	@ -#]!
	mov 	r1,#0
	ldr 	r0,=romvar2
	add 	r3,r0,#2
	add 	r0,r0,#4
	add 	r2,r0,#1
	ldrb 	r0,[r0,#-1]!
	cmp 	r0,#0xff
	orrne 	r1,r1,#BAD_Rd
	ldrb 	r0,[r2,#-3]!
	cmp 	r0,#0
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#3
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRB
	bl 	DrawResult
	add 	r8,r8,#8
	

	@ LDRSB
	
	@ +#]
	mov 	r1,#0
	ldr 	r0,=romvar3
	sub 	r2,r0,#3
	mov 	r3,r2
	ldrsb 	r0,[r0,#0]
	ldr 	r5,=0xffffff80
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	ldrsb 	r0,[r2,#4]
	cmp 	r0,#0x7f
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#0
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRSB
	bl 	DrawResult
	add 	r8,r8,#8


	@ -#]
	mov 	r1,#0
	ldr 	r0,=romvar3
	add 	r2,r0,#3
	add 	r0,r0,#1
	mov 	r3,r2
	ldrsb 	r0,[r0,#-1]
	ldr 	r5,=0xffffff80
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	ldrsb 	r0,[r2,#-2]
	cmp 	r0,#0x7f
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#1
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRSB
	bl 	DrawResult
	add 	r8,r8,#8

	@ +R]
	mov 	r1,#0
	ldr 	r0,=romvar3
	sub 	r2,r0,#3
	mov 	r3,r2
	mov 	r4,#4
	ldrsb 	r0,[r0,r9]
	ldr 	r5,=0xffffff80
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	ldrsb 	r0,[r2,r4]
	cmp 	r0,#0x7f
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#4
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRSB
	bl 	DrawResult
	add 	r8,r8,#8


	@ -R]
	mov 	r1,#0
	ldr 	r0,=romvar3
	add 	r2,r0,#3
	add 	r0,r0,#1
	mov 	r3,r2
	mov 	r4,#1
	ldrsb 	r0,[r0,-r4]
	ldr 	r5,=0xffffff80
	cmp 	r0,r5
	orrne 	r1,r1,#BAD_Rd
	add 	r4,r4,r4
	ldrsb 	r0,[r2,-r4]
	cmp 	r0,#0x7f
	orrne 	r1,r1,#BAD_Rd
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rn
	mov 	r2,#5
	orr 	r1,r1,#0x80000000
	ldr 	r0,=szLDRSB
	bl 	DrawResult
	add 	r8,r8,#8
	
	
	ldmfd 	sp!,{lr}
	mov 	pc,lr
.pool
.align



Test4:
	stmfd 	sp!,{lr}

	ldr 	r0,=szLDM1
	mov 	r1,#52
	mov 	r2,#1
	mov 	r3,#4
	bl 	DrawText

	@ LDMIB!
	mov 	r1,#0
	ldr 	r3,=var64
	sub 	r3,r3,#4
	ldmib 	r3!,{r4,r5}
	ldr 	r0,=var64+4
	cmp 	r3,r0
	orrne 	r1,r1,#BAD_Rn
	mov 	r4,#5

	@ Test writeback for when the base register is included in the

	@ register list.

	ldr 	r3,=var64
	sub 	r3,r3,#4
	ldmib 	r3!,{r2,r3}
	ldr 	r0,=var64+4
	mov 	r5,r2
	ldr 	r2,[r0]
	cmp 	r3,r2
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r2,=rnVal
	strne 	r3,[r2]

	ldr 	r3,=var64
	sub 	r3,r3,#4
	ldmib 	r3!,{r3,r5}
	ldr 	r2,=var64+4
	cmp 	r3,r2
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r2,=rnVal
	strne 	r3,[r2]
	
	ldr 	r2,[r0]
	cmp 	r5,r2
	orrne 	r1,r1,#BAD_Rd
	cmp 	r4,#5
	orrne 	r1,r1,#BAD_Rd
	mov 	r2,#0
	orr 	r1,r1,#0x40000000
	ldr 	r0,=szLDM
	bl 	DrawResult
	add 	r8,r8,#8


	@ LDMIA!
	mov 	r1,#0
	ldr 	r3,=var64
	ldmia 	r3!,{r4,r5}
	ldr 	r0,=var64+8
	cmp 	r3,r0
	orrne 	r1,r1,#BAD_Rn
	mov 	r4,#5

	@ Test writeback for when the base register is included in the
	@ register list.
	ldr 	r3,=var64
	ldmia 	r3!,{r2,r3}
	ldr 	r0,=var64+4
	mov 	r5,r2
	ldr 	r2,[r0]
	cmp 	r3,r2
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r2,=rnVal
	strne 	r3,[r2]

	ldr 	r3,=var64
	ldmia 	r3!,{r3,r5}
	ldr 	r2,=var64+8
	cmp 	r3,r2
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r2,=rnVal
	strne 	r3,[r2]
	
	ldr 	r2,[r0]
	cmp 	r5,r2
	orrne 	r1,r1,#BAD_Rd
	cmp 	r4,#5
	orrne 	r1,r1,#BAD_Rd
	mov 	r2,#1
	orr 	r1,r1,#0x40000000
	ldr 	r0,=szLDM
	bl 	DrawResult
	add 	r8,r8,#8


	@ LDMDB!
	mov 	r1,#0
	ldr 	r3,=var64+8
	ldmdb 	r3!,{r4,r5}
	ldr 	r0,=var64
	cmp 	r3,r0
	orrne 	r1,r1,#BAD_Rn
	mov 	r4,#5

	@ Test writeback for when the base register is included in the
	@ register list.
	ldr 	r3,=var64+8
	ldmdb 	r3!,{r2,r3}
	ldr 	r0,=var64+4
	mov 	r5,r2
	ldr 	r2,[r0]
	cmp 	r3,r2
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r2,=rnVal
	strne 	r3,[r2]

	ldr 	r3,=var64+8
	ldmdb 	r3!,{r3,r5}
	ldr 	r2,=var64
	cmp 	r3,r2
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r2,=rnVal
	strne 	r3,[r2]
	
	ldr 	r2,[r0]
	cmp 	r5,r2
	orrne 	r1,r1,#BAD_Rd
	cmp 	r4,#5
	orrne 	r1,r1,#BAD_Rd
	mov 	r2,#2
	orr 	r1,r1,#0x40000000
	ldr 	r0,=szLDM
	bl 	DrawResult
	add 	r8,r8,#8
	

	@ LDMDA!
	mov 	r1,#0
	ldr 	r3,=var64+4
	ldmda 	r3!,{r4,r5}
	ldr 	r0,=var64-4
	cmp 	r3,r0
	orrne 	r1,r1,#BAD_Rn
	mov 	r4,#5

	@ Test writeback for when the base register is included in the
	@ register list.
	ldr 	r3,=var64+4
	ldmda 	r3!,{r2,r3}
	ldr 	r0,=var64+4
	mov 	r5,r2
	ldr 	r2,[r0]
	cmp 	r3,r2
	orrne 	r1,r1,#BAD_Rn	@ r3 should contain the value loaded from memory
	ldrne 	r2,=rnVal
	strne 	r3,[r2]

	ldr 	r3,=var64+4
	ldmda 	r3!,{r3,r5}
	ldr 	r2,=var64-4
	cmp 	r3,r2
	orrne 	r1,r1,#BAD_Rn	@ r3 should contain the updated base
	ldrne 	r2,=rnVal
	strne 	r3,[r2]
	
	ldr 	r2,[r0]
	cmp 	r5,r2
	orrne 	r1,r1,#BAD_Rd
	cmp 	r4,#5
	orrne 	r1,r1,#BAD_Rd	@ Make sure that the LDM didn't touch other registers
	mov 	r2,#3
	orr 	r1,r1,#0x40000000
	ldr 	r0,=szLDM
	bl 	DrawResult
	add 	r8,r8,#8

	
	@ LDMIBS!
	mov	r0, #0x12	@ Switch to IRQ mode
	msr	cpsr, r0
	mov	r1,#0
	mov	r14,#123
	ldr	r0,=var64-4
	ldmib	r0!,{r3,r14}^
	ldr	r2,=var64+4
	cmp	r0,r2
	orrne	r1,r1,#BAD_Rn
	ldrne 	r5,=rnVal
	strne 	r0,[r5]
	sub	r2,r2,#4
	ldr	r2,[r2]
	cmp	r2,r3
	orrne	r1,r1,#BAD_Rd
	cmp	r14,#123
	orrne	r1,r1,#BAD_Rd
	mov 	r2,#4
	mov	r0, #0x1F	@ Switch to user mode
	msr	cpsr, r0
	orr 	r1,r1,#0x40000000
	ldr 	r0,=szLDM
	bl 	DrawResult
	add 	r8,r8,#8

	@ LDMIAS!
	mov	r0, #0x12	@ Switch to IRQ mode
	msr	cpsr, r0
	mov	r1,#0
	mov	r14,#123
	ldr	r0,=var64
	ldmia	r0!,{r3,r14}^
	ldr	r2,=var64+8
	cmp	r0,r2
	orrne	r1,r1,#BAD_Rn
	ldrne 	r5,=rnVal
	strne 	r0,[r5]
	sub	r2,r2,#8
	ldr	r2,[r2]
	cmp	r2,r3
	orrne	r1,r1,#BAD_Rd
	cmp	r14,#123
	orrne	r1,r1,#BAD_Rd
	mov 	r2,#5
	mov	r0, #0x1F	@ Switch to user mode
	msr	cpsr, r0
	orr 	r1,r1,#0x40000000
	ldr 	r0,=szLDM
	bl 	DrawResult
	add 	r8,r8,#8

	@ LDMDBS!
	mov	r0, #0x12	@ Switch to IRQ mode
	msr	cpsr, r0
	mov	r1,#0
	mov	r14,#123
	ldr	r0,=var64+8
	ldmdb	r0!,{r3,r14}^
	ldr	r2,=var64
	cmp	r0,r2
	orrne	r1,r1,#BAD_Rn
	ldrne 	r5,=rnVal
	strne 	r0,[r5]
	ldr	r2,[r2]
	cmp	r2,r3
	orrne	r1,r1,#BAD_Rd
	cmp	r14,#123
	orrne	r1,r1,#BAD_Rd
	mov 	r2,#6
	mov	r0, #0x1F	@ Switch to user mode
	msr	cpsr, r0
	orr 	r1,r1,#0x40000000
	ldr 	r0,=szLDM
	bl 	DrawResult
	add 	r8,r8,#8

	@ LDMDAS!
	mov	r0, #0x12	@ Switch to IRQ mode
	msr	cpsr, r0
	mov	r1,#0
	mov	r14,#123
	ldr	r0,=var64+4
	ldmda	r0!,{r3,r14}^
	ldr	r2,=var64-4
	cmp	r0,r2
	orrne	r1,r1,#BAD_Rn
	ldrne 	r5,=rnVal
	strne 	r0,[r5]
	add	r2,r2,#4
	ldr	r2,[r2]
	cmp	r2,r3
	orrne	r1,r1,#BAD_Rd
	cmp	r14,#123
	orrne	r1,r1,#BAD_Rd
	mov 	r2,#7
	mov	r0, #0x1F	@ Switch to user mode
	msr	cpsr, r0
	orr 	r1,r1,#0x40000000
	ldr 	r0,=szLDM
	bl 	DrawResult
	add 	r8,r8,#8
	
	@ STMIB!
	mov 	r1,#0
	ldr 	r3,=(VARBASE+0x1FC)
	mov 	r4,#5
	stmib 	r3!,{r3,r4,r5}
	ldr 	r0,=(VARBASE+0x208)
	cmp 	r3,r0
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r5,=rnVal
	strne 	r3,[r5]
	sub 	r0,r0,#8
	ldr 	r2,[r0]
	sub 	r0,r0,#4
	cmp 	r2,r0
	@orrne 	r1,r1,#0x80
	@ldrne	r0,=memVal
	@strne	r2,[r0]

	ldr 	r3,=(VARBASE+0x1FC)
	mov 	r4,#5
	stmib 	r3!,{r2,r3,r4}
	ldr 	r0,=(VARBASE+0x208)
	cmp 	r3,r0
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r5,=rnVal
	strne 	r3,[r5]
	ldr	r2,[r0]
	cmp	r4,r2
	orrne	r1,r1,#0x80
	ldrne	r0,=memVal
	strne	r4,[r0] @r2,[r0]
	

	@ldr 	r0,=(VARBASE+0x204)
	@ldr 	r2,[r0]
	@ldr	r3,=(VARBASE+0x208)
	@cmp 	r3,r2
	@orrne 	r1,r1,#0x80
	@ldrne	r0,=memVal
	@strne	r2,[r0]
	
	mov 	r2,#0
	orr 	r1,r1,#0x40000000
	ldr 	r0,=szSTM
	bl 	DrawResult
	add 	r8,r8,#8
	
	@ STMIA!
	mov 	r1,#0
	ldr 	r3,=(VARBASE+0x200)
	mov 	r4,#5
	stmia 	r3!,{r3,r4,r5}
	ldr 	r0,=(VARBASE+0x20C)
	cmp 	r3,r0
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r5,=rnVal
	strne 	r3,[r5]
	sub 	r0,r0,#0xC
	ldr 	r2,[r0]
	cmp 	r2,r0
	orrne 	r1,r1,#0x80
	ldrne	r4,=memVal
	strne	r0,[r4] @r2,[r4]

	ldr 	r3,=(VARBASE+0x200)
	mov 	r4,#5
	stmia 	r3!,{r2,r3,r4}
	ldr 	r0,=(VARBASE+0x20C)
	cmp 	r3,r0
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r5,=rnVal
	strne 	r3,[r5]
	ldr 	r0,=(VARBASE+0x208)
	ldr	r2,[r0]
	cmp	r4,r2
	@orrne	r1,r1,#0x80
	@ldrne	r0,=memVal
	@strne	r2,[r0]
	

	@ldr 	r0,=(VARBASE+0x204)
	@ldr 	r2,[r0]
	@ldr	r3,=(VARBASE+0x20C)
	@cmp 	r3,r2
	@orrne 	r1,r1,#0x80
	@ldrne	r0,=memVal
	@strne	r2,[r0]
	
	mov 	r2,#1
	orr 	r1,r1,#0x40000000
	ldr 	r0,=szSTM
	bl 	DrawResult
	add 	r8,r8,#8	


	@ STMDB!
	mov 	r1,#0
	ldr 	r3,=(VARBASE+0x20C)
	mov 	r4,#5
	stmdb 	r3!,{r3,r4,r5}
	ldr 	r0,=(VARBASE+0x200)
	cmp 	r3,r0
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r5,=rnVal
	strne 	r3,[r5]
	ldr 	r2,[r0]
	add	r0,r0,#0xC
	cmp 	r2,r0
	orrne 	r1,r1,#0x80
	ldrne	r0,=memVal
	strne	r2,[r0]

	ldr 	r3,=(VARBASE+0x20C)
	mov 	r4,#5
	stmdb 	r3!,{r2,r3,r4}
	ldr 	r0,=(VARBASE+0x200)
	cmp 	r3,r0
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r5,=rnVal
	strne 	r3,[r5]
	add	r0,r0,#8
	ldr	r2,[r0]
	cmp	r4,r2
	orrne	r1,r1,#0x80
	ldrne	r0,=memVal
	strne	r2,[r0]
	@ldr 	r0,=(VARBASE+0x204)
	@ldr 	r2,[r0]
	@ldr	r3,=(VARBASE+0x200)
	@cmp 	r3,r2
	@orrne 	r1,r1,#0x80
	@ldrne	r0,=memVal
	@strne	r2,[r0]
	
	mov 	r2,#2
	orr 	r1,r1,#0x40000000
	ldr 	r0,=szSTM
	bl 	DrawResult
	add 	r8,r8,#8


	@ STMDA!
	mov 	r1,#0
	ldr 	r3,=(VARBASE+0x208)
	mov 	r4,#5
	stmda 	r3!,{r3,r4,r5}
	ldr 	r0,=(VARBASE+0x1FC)
	cmp 	r3,r0
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r5,=rnVal
	strne 	r3,[r5]
	add	r0,r0,#4
	ldr 	r2,[r0]
	add	r0,r0,#8
	cmp 	r2,r0
	orrne 	r1,r1,#0x80
	ldrne	r0,=memVal
	strne	r2,[r0]

	ldr 	r3,=(VARBASE+0x208)
	mov 	r4,#5
	stmda 	r3!,{r2,r3,r4}
	ldr 	r0,=(VARBASE+0x1FC)
	cmp 	r3,r0
	orrne 	r1,r1,#BAD_Rn
	ldrne 	r5,=rnVal
	strne 	r3,[r5]
	add	r0,r0,#0xC
	ldr	r2,[r0]
	cmp	r4,r2
	orrne	r1,r1,#0x80
	ldrne	r0,=memVal
	strne	r2,[r0]
	@ldr 	r0,=(VARBASE+0x204)
	@ldr 	r2,[r0]
	@ldr	r3,=(VARBASE+0x1FC)
	@cmp 	r3,r2
	@orrne 	r1,r1,#0x80
	@ldrne	r0,=memVal
	@strne	r2,[r0]
	
	mov 	r2,#3
	orr 	r1,r1,#0x40000000
	ldr 	r0,=szSTM
	bl 	DrawResult
	add 	r8,r8,#8
	
	ldmfd sp!,{lr}
	mov pc,lr
.pool
.align



	
Test5:
	stmfd sp!,{lr}
	
	ldr r0,=szAV5
	mov r1,#64
	mov r2,#1
	mov r3,#4
	bl DrawText
	

	@ CLZ
	mov 	r1,#0
	mov 	r2,#0x80000000
	mov 	r3,#0x1F
	mov 	r4,#0
	clz 	r0,r2
	cmp 	r0,#0
	orrne 	r1,r1,#BAD_Rd
	clz 	r0,r3
	cmp 	r0,#27
	orrne 	r1,r1,#BAD_Rd
	clz 	r0,r4
	cmp 	r0,#32
	orrne 	r1,r1,#BAD_Rd
	ldr 	r0,=szCLZ
	bl 	DrawResult
	add 	r8,r8,#8

	@ LDRD
	mov 	r1,#0
	ldr 	r0,=var64
	mov 	r2,#0

	mov 	r3,#0
	ldrd 	r2,[r0],#-1
	ldr 	r4,=0x11223344
	ldr 	r5,=0x55667788
	cmp 	r2,r4
	orrne 	r1,r1,#BAD_Rd
	cmp 	r3,r5
	orrne 	r1,r1,#BAD_Rd
	ldr 	r4,=var64 - 1
	cmp 	r0,r4
	orrne 	r1,r1,#BAD_Rn
	ldr 	r0,=szLDRD
	bl 	DrawResult
	add 	r8,r8,#8
	
	
	@ MRC
	mrc 	p15,0,r2,c0,c0,1	@ Get cache type
	mov 	r1,#0
	ldr 	r3,=0x0F0D2112
	cmp 	r2,r3
	orrne 	r1,r1,#BAD_Rd		@ Harvard, 4k data 4-way, 8k instr 4-way	
	ldr 	r0,=szMRC
	bl 	DrawResult
	add 	r8,r8,#8


	@ QADD
	mov 	r1,#0
	msr 	cpsr_f,#0
	mov 	r2,#0x70000000
	qadd 	r3,r2,r2
	ldr 	r4,=0x7FFFFFFF
	cmp 	r3,r4
	orrne 	r1,r1,#BAD_Rd
	mrs 	r3,cpsr
	and 	r3,r3,#0x08000000	@0xF8000000
	cmp 	r3,#0x08000000
	orrne 	r1,r1,#0x40
	ldr 	r0,=szQADD
	bl 	DrawResult
	add 	r8,r8,#8

	@ SMLABB
	mov 	r1,#0
	msr 	cpsr_f,#0
	mov 	r2,#0x7000
	mov 	r3,#0x7000
	mov 	r4,#0x50000000
	smlabb	r5,r2,r3,r4
	@ldr 	r4,=0x7FFFFFFF
	ldr 	r4,=0x81000000
	cmp 	r5,r4
	orrne 	r1,r1,#BAD_Rd
	mrs 	r3,cpsr
	and 	r3,r3,#0x08000000
	cmp 	r3,#0x08000000
	orrne 	r1,r1,#0x40
	mov 	r3,#0x7000
	mov 	r4,#0
	smlabb 	r5,r2,r3,r4
	ldr 	r4,=0x31000000
	cmp 	r5,r4
	orrne 	r1,r1,#BAD_Rd
	mrs 	r3,cpsr
	and 	r3,r3,#0x08000000
	cmp 	r3,#0x08000000
	orrne 	r1,r1,#0x40
	ldr 	r0,=szSMLABB
	bl 	DrawResult
	add 	r8,r8,#8


	@ SMLABT
	mov 	r1,#0
	msr 	cpsr_f,#0
	mov 	r2,#0x00007000
	mov 	r3,#0x70000000
	mov 	r4,#0x50000000
	smlabt 	r5,r2,r3,r4
	@ldr 	r4,=0x7FFFFFFF
	ldr 	r4,=0x81000000
	cmp 	r5,r4
	orrne 	r1,r1,#BAD_Rd
	mrs 	r3,cpsr
	and 	r3,r3,#0x08000000
	cmp 	r3,#0x08000000
	orrne 	r1,r1,#0x40
	mov 	r3,#0x70000000
	mov 	r4,#0
	smlabt 	r5,r2,r3,r4
	ldr 	r4,=0x31000000
	cmp 	r5,r4
	orrne 	r1,r1,#BAD_Rd
	mrs 	r3,cpsr
	and 	r3,r3,#0x08000000
	cmp 	r3,#0x08000000
	orrne 	r1,r1,#0x40
	ldr 	r0,=szSMLABT
	bl 	DrawResult
	add 	r8,r8,#8

	@ SMLATB
	mov 	r1,#0
	msr 	cpsr_f,#0
	mov 	r2,#0x00007000
	mov 	r3,#0x70000000
	mov 	r4,#0x50000000
	smlatb 	r5,r3,r2,r4
	@ldr 	r4,=0x7FFFFFFF
	ldr 	r4,=0x81000000
	cmp 	r5,r4
	orrne 	r1,r1,#BAD_Rd
	mrs 	r3,cpsr
	and 	r3,r3,#0x08000000
	cmp 	r3,#0x08000000
	orrne 	r1,r1,#0x40
	mov 	r3,#0x70000000
	mov 	r4,#0
	smlatb 	r5,r3,r2,r4
	ldr 	r4,=0x31000000
	cmp 	r5,r4
	orrne 	r1,r1,#BAD_Rd
	mrs 	r3,cpsr
	and 	r3,r3,#0x08000000
	cmp 	r3,#0x08000000
	orrne 	r1,r1,#0x40
	ldr 	r0,=szSMLATB
	bl 	DrawResult
	add 	r8,r8,#8

	@ SMLATT
	mov 	r1,#0
	msr 	cpsr_f,#0
	mov 	r2,#0x70000000
	mov 	r3,#0x70000000
	mov 	r4,#0x50000000
	smlatt 	r5,r2,r3,r4
	@ldr 	r4,=0x7FFFFFFF
	ldr 	r4,=0x81000000
	cmp 	r5,r4
	orrne 	r1,r1,#BAD_Rd
	mrs 	r3,cpsr
	and 	r3,r3,#0x08000000
	cmp 	r3,#0x08000000
	orrne 	r1,r1,#0x40
	mov 	r3,#0x70000000
	mov 	r4,#0
	smlatt 	r5,r2,r3,r4
	ldr 	r4,=0x31000000
	cmp 	r5,r4
	orrne 	r1,r1,#BAD_Rd
	mrs 	r3,cpsr
	and 	r3,r3,#0x08000000
	cmp 	r3,#0x08000000
	orrne 	r1,r1,#0x40
	ldr 	r0,=szSMLATT
	bl 	DrawResult
	add 	r8,r8,#8

	ldmfd 	sp!,{lr}
	mov 	pc,lr
.pool
.align


Test6:
Test7:
Test8:
Test9:

TestTmb:
	mov r8,#0
	mov r9,#3
	ldr r11,=0xFFFFFFFF

	ldr r0,=menulinks
	ldr r4,=CURSEL
	ldrb r3,[r4]
	sub r4,r4,#8
	add r0,r0,r3,lsl#2
	ldr r5,[r0]
	sub r5,r5,#11
	str r5,[r4]
	
	ldr r0,=_tmbmain
	add r0,r0,#1
	bx r0

.pool
.align


.align 2
.global romvar
.global romvar2
.global romvar3
.global palette

romvar:  	.byte 0x80,0,0,0
romvar2: 	.byte 0x00,0x8f,0,0xff
romvar3: 	.byte 0x80,0x7f,0,0


.align 3
var64:		.word 0x11223344,0x55667788

rdVal:		.word 0
rnVal:		.word 0
memVal:		.word 0

palette:
		.hword 0x0000,0x0300,0x0018,0x7fff,0x7318,0x0E1F

menuitems:	.word szArm1,szArm2,szArm3
		.word szTmb1,szTmb2,szTmb3
		.word szArm4

menulinks:	.word 0,2,4
		.word 11,12,13
		.word 5
		
jumptable: 	.word Test0,Test1,Test2,Test3
	   	.word Test4,Test5,Test6,Test7
	   	.word Test8,Test9,Menu,TestTmb,TestTmb,TestTmb
	   	
.global font
font:		.incbin "font8x8.pat"

szADC:		.asciz "ADC"
szADD:		.asciz "ADD"
szAND:		.asciz "AND"
szBIC:		.asciz "BIC"
szCMN:		.asciz "CMN"
szCMP:		.asciz "CMP"
szEOR:		.asciz "EOR"
szMOV:		.asciz "MOV"
szMVN:		.asciz "MVN"
szORR:		.asciz "ORR"
szRSB:		.asciz "RSB"
szRSC:		.asciz "RSC"
szSBC:		.asciz "SBC"
szSUB:		.asciz "SUB"
szTEQ:		.asciz "TEQ"
szTST:		.asciz "TST"

szMLA:		.asciz "MLA"
szMUL:		.asciz "MUL"
szUMULL: 	.asciz "UMULL"
szUMLAL: 	.asciz "UMLAL"
szSMULL: 	.asciz "SMULL"
szSMLAL:	.asciz "SMLAL"

szSWP:		.asciz "SWP"
szSWPB:		.asciz "SWPB"

szMRS:		.asciz "MRS"
szMSR:		.asciz "MSR"

szLDR:		.asciz "LDR"
szLDRH:		.asciz "LDRH"
szLDRB:		.asciz "LDRB"
szLDRSH:	.asciz "LDRSH"
szLDRSB:	.asciz "LDRSB"
szSTR:		.asciz "STR"
szSTRH:		.asciz "STRH"
szSTRB:		.asciz "STRB"

szLDM:		.asciz "LDM"
szSTM:		.asciz "STM"

szCLZ:		.asciz "CLZ"
szLDRD:		.asciz "LDRD"
szMRC:		.asciz "MRC"
szQADD:		.asciz "QADD"
szSMLABB:	.asciz "SMLABB"
szSMLABT:	.asciz "SMLABT"
szSMLATB:	.asciz "SMLATB"
szSMLATT:	.asciz "SMLATT"
szSMULBB:	.asciz "SMULBB"
szSMULBT:	.asciz "SMULBT"
szSMULTB:	.asciz "SMULTB"
szSMULTT:	.asciz "SMULTT"


.global szLDRtype
szLDRtype:	.ascii "&\\] "
		.byte 0
	  	.ascii "'\\] "
		.byte 0
	  	.ascii "&\\])"
		.byte 0
	  	.ascii "'\\])"
		.byte 0
	  	.ascii "&R] "
		.byte 0
	  	.ascii "'R] "
		.byte 0
	  	.ascii "&R])"
		.byte 0
	  	.ascii "'R])"
		.byte 0
	  	.ascii "]&\\ "
		.byte 0
	  	.ascii "]'\\ "
		.byte 0
	  	.ascii "]&R "
		.byte 0
	  	.ascii "]'R "
		.byte 0

szLDMtype:	.ascii "IB) "
		.byte 0
		.ascii "IA) "
		.byte 0
		.ascii "DB) "
		.byte 0
		.ascii "DA) "
		.byte 0
		.ascii "IBS)"
		.byte 0
		.ascii "IAS)"
		.byte 0
		.ascii "DBS)"
		.byte 0
		.ascii "DBA)"
		.byte 0
		
szOK:		.asciz "OK"
szBad:		.asciz "BAD"
szRd:		.asciz "R^"
szRn:		.asciz "R_"
szC:		.asciz "C"
szN:		.asciz "N"
szV:		.asciz "V"
szZ:		.asciz "Z"
szQ:		.asciz "Q"
szSel:		.asciz "`"
szMem:		.asciz "MEM"

szAsterixes:	.asciz "jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj"
szAster2:	.asciz "j                              j"
szArmwrestler:	.asciz "j        ARMWRESTLER DS        j"
szAuthor:	.asciz "j          Micg 2006           j"

szArm1:		.asciz "ARM ALU"
szArm2:		.asciz "ARM LDR/STR"
szArm3:		.asciz "ARM LDM/STM"
szTmb1:		.asciz "THUMB ALU"
szTmb2:		.asciz "THUMB LDR/STR"
szTmb3:		.asciz "THUMB LDM/STM"
szArm4:		.asciz "ARM V5TE"
szMarker:	.asciz "`"
szSelect:	.asciz "SELECT A TEST AND PRESS"

szALU1:		.asciz  "ALU TESTS PART 1"
szALU2:		.asciz  "ALU PT 2 / MISC"
szLS1: 		.asciz   "LOAD TESTS PART 1"
szLS2: 		.asciz   "LOAD TESTS PART 2"
szLS3: 		.asciz   "LOAD TESTS PART 3"
szLS4: 		.asciz   "LOAD TESTS PART 4"
szLS5: 		.asciz   "LOAD TESTS PART 5"
szLS6: 		.asciz   "LOAD TESTS PART 6"
szLS7: 		.asciz   "LOAD TESTS PART 7"
szLS8: 		.asciz   "LOAD TESTS PART 8"
szLDM1:		.asciz 	"LDM/STM TESTS 1"
szAV5:		.asciz  "ARM V5TE TESTS"

.global szSelect2
.global szNext
.global szStart
.global szMenu

szPressStart: 	.asciz "PRESS START"
szSelect2:	.asciz "SELECT:"
szNext:		.asciz "Nextg"
szStart:	.asciz "START:"
szStart2:	.asciz "START"
szMenu:		.asciz "Menu"
szSpace:	.asciz " "

szRomMsg:	.asciz "Message from ROM"

szHexNum:	.asciz "00000000"

.align
.pool
@.end
