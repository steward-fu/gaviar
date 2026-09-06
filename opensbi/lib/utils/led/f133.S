    .equ GPIO_BASE,     0x02000000
    .equ CCU_BASE,      0x02001000
    .equ GPIO_BASE,     0x02000000
    .equ PE_CFG0,       0x00c0
    .equ PE_DAT,        0x00d0
    .equ _50NS,         10
    .equ _100NS,        20
    .equ _200NS,        40
    .equ _300NS,        60
    .equ _400NS,        80
    .equ _450NS,        90
    .equ _800NS,        160
    .equ _850NS,        170
    .equ _1US,          200
    .equ _1S,           200000000
    
    .section .text, "ax", %progbits
	.align
    .global f133_led_blink
f133_led_blink:
    li t0, 0x100000
    li a0, GPIO_BASE + PE_CFG0
    sw t0, 0(a0)

    li a0, GPIO_BASE + PE_DAT
0:
    li t0, (1 << 12)
    jal b24

    li t0, _1S
    jal decloop

    li t0, (0 << 0)
    jal b24
    
    li t0, _1S
    jal decloop
    j 0b

b24:
    move t2, ra
    move t3, t0
    li t4, 24
0:
    and t0, t3, 1
    srl t3, t3, 1
    beqz t0, 1f
    jal b1
    j 2f
1:
    jal b0
2:
    addi t4, t4, -1
    bgtz t4, 0b
    jr t2

b0:
    move t1, ra
    li t0, (1 << 5)
    sw t0, 0(a0)

    li t0, _300NS
    jal decloop

    li t0, (0 << 5)
    sw t0, 0(a0)

    li t0, _850NS
    jal decloop
    jr t1

b1:
    move t1, ra
    li t0, (1 << 5)
    sw t0, 0(a0)

    li t0, _800NS
    jal decloop

    li t0, (0 << 5)
    sw t0, 0(a0)

    li t0, _400NS
    jal decloop
    jr t1

decloop:
    addi t0, t0, -1
    bgtz t0, decloop
    jr ra
