.global _start

_start: 
        sltiu   zero,zero, 0x101 # start tracing

        addi    x3, x4, 43

        sltiu   zero, zero, 0x102 # start ROI
        
        addi    x5, x6, 44

        j       x
        
x:      beq     x0, x5, z
        bne     x0, x0, t
        beq     x0, x0, t
t:      jal     y

        la      t5, z
        jr      t5
        
z:      sltiu   zero, zero, 0x103 # end ROI

        # exit(0)
        li      a0, 0
        li      a7, 93
        ecall

y:      la      a3, x
        mul     a1, a2, a3
        addiw   a1, a1, 4
        ret
        
