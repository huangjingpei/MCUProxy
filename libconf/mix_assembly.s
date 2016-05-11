        .syntax unified
        .arch armv7-a
        .eabi_attribute 27, 3
        .fpu neon
        .eabi_attribute 20, 1
        .eabi_attribute 21, 1
        .eabi_attribute 23, 3
        .eabi_attribute 24, 1
        .eabi_attribute 25, 1
        .eabi_attribute 26, 2
        .eabi_attribute 30, 4
        .eabi_attribute 18, 4
        .thumb
        .text
        .align 2
@----------------------------------------------------------------------
        .global fastmix_pre_amp
        .type   fastmix_pre_amp, %function
fastmix_pre_amp:
        .fnstart
        @ args = 0, pretend = 0, frame = 0
        @ frame_needed = 0, uses_anonymous_args = 0
in_ptr  .req r0
out_ptr .req r1
len     .req r2


        push    {r4, r5, r6, r7, r8, r9, sl, lr}
        vpush   {d8, d9, d10, d11, d12, d13, d14, d15}


        cmp     r1, #0
        ble     .L7

        mov     r5, r0
        mov     r4, r1
        mov     sl, r2

        mov     r9, #0
        lsl     r7, r2, #1              @ nLen << 1
        b       .L15
.L10:
        add     r9, r9, #1
        adds    r5, r5, #8
        cmp     r9, r4
        beq     .L7
.L15:
        ldr     r3, [r5, #0]            @ pInput[j].amp
        adds    r2, r3, #1
        beq     .L19                    @ pIntput[j].amp == MIX_DISABLE
        cmp     r3, #1
        beq     .L20                    @ pIntput[j].amp == MIX_SUPP_3dB
        cmp     r3, #3
        beq     .L21                    @ pIntput[j].amp == MIX_GAIN_3dB
        b       .L10
.L19:
        cmp     sl, #0                  @ nLen cycle 0
        ble     .L10
        movs    r3, #0
.L11:
        ldr     r2, [r5, #4]
        movs    r1, #0
        strh    r1, [r2, r3]    @ movhi
        adds    r3, r3, #2
        cmp     r3, r7
        bne     .L11
@///////////////////////////////////////////////////////////
        @sub    r3, r2, #2
        @vmov.I16       q0, #0
        @vmov.I16       q1, #0
        @vmov.I16       q2, #0
        @vmov.I16       q3, #0
        @vmov.I16       q4, #0
        @vmov.I16       q5, #0
        @vmov.I16       q6, #0
        @vmov.I16       q7, #0
        @vst1.16                {d0, d1, d2, d3}, [r3, #2]!
        @vst1.16                {d4, d5, d6, d7}, [r3, #2]!
        @vst1.16                {d8, d9, d10, d11}, [r3, #2]!
        @vst1.16                {d12, d13, d14, d15}, [r3, #2]!
@///////////////////////////////////////////////////////////
        b       .L10
.L20:
        cmp     sl, #0
        ble     .L10
        movs    r3, #0
.L13:
        ldr     r2, [r5, #4]
        ldrsh   r1, [r2, r3]
        asrs    r1, r1, #1
        strh    r1, [r2, r3]    @ movhi
        adds    r3, r3, #2
        cmp     r3, r7
        bne     .L13
@///////////////////////////////////////////////////////////
        @sub    r3, r2, #2
        @vld1.16        {d0, d1, d2, d3}, [r3, #2]!
        @vld1.16        {d4, d5, d6, d7}, [r3, #2]!
        @vld1.16        {d8, d9, d10, d11}, [r3, #2]!
        @vld1.16        {d12, d13, d14, d15}, [r3, #2]!
        @vshr.s16    q0, q0, #1
        @vshr.s16    q1, q1, #1
        @vshr.s16    q2, q2, #1
        @vshr.s16    q3, q3, #1
        @vshr.s16    q4, q4, #1
        @vshr.s16    q5, q5, #1
        @vshr.s16    q6, q6, #1
        @vshr.s16    q7, q7, #1
        @sub    r3, r2, #2
        @vst1.16        {d0, d1, d2, d3}, [r3, #2]!
        @vst1.16        {d4, d5, d6, d7}, [r3, #2]!
        @vst1.16        {d8, d9, d10, d11}, [r3, #2]!
        @vst1.16        {d12, d13, d14, d15}, [r3, #2]!
@///////////////////////////////////////////////////////////
        b       .L10
.L21:
        cmp     sl, #0
        ble     .L10
        movs    r6, #0
.L14:
        ldr     r8, [r5, #4]
        @ldrsh  r0, [r8, r6]
        @ssat   r0, #16, r0, lsl #1
        @strh   r0, [r8, r6]    @ movhi
        @adds   r6, r6, #2
        @cmp    r6, r7
        @bne    .L14
@///////////////////////////////////////////////////////////
        mov     r3, r8
        vld1.16 {d0, d1, d2, d3}, [r3]!
        vld1.16 {d4, d5, d6, d7}, [r3]!
        vld1.16 {d8, d9, d10, d11}, [r3]!
        vld1.16 {d12, d13, d14, d15}, [r3]!
        vqshl.s16    q0, q0, #1
        vqshl.s16    q1, q1, #1
        vqshl.s16    q2, q2, #1
        vqshl.s16    q3, q3, #1
        vqshl.s16    q4, q4, #1
        vqshl.s16    q5, q5, #1
        vqshl.s16    q6, q6, #1
        vqshl.s16    q7, q7, #1
        mov     r3, r8
        vst1.16 {d0, d1, d2, d3}, [r3]!
        vst1.16 {d4, d5, d6, d7}, [r3]!
        vst1.16 {d8, d9, d10, d11}, [r3]!
        vst1.16 {d12, d13, d14, d15}, [r3]!
@///////////////////////////////////////////////////////////
        b       .L10
.L7:
        vpop    {d8, d9, d10, d11, d12, d13, d14, d15}
        pop     {r4, r5, r6, r7, r8, r9, sl, pc}

        .fnend
        .size   fastmix_pre_amp, .-fastmix_pre_amp
@----------------------------------------------------------------------
        .section        .text.fastmix_sat_amp,"ax",%progbits
        .align  1
        .global fastmix_sat_amp
        .thumb
        .thumb_func
        .type   fastmix_sat_amp, %function
fastmix_sat_amp:
        .fnstart
.LFB16:
        @ args = 0, pretend = 0, frame = 0
        @ frame_needed = 0, uses_anonymous_args = 0
        push    {r4, r5, r6, r7, r8, lr}

        ldr     r3, [r1, #0]
        mov     r4, r1

        adds    r1, r3, #1
        beq     .L36                    @pOutput->amp == MIX_DISABLE
        cmp     r3, #1
        beq     .L37                    @pOutput->amp == MIX_SUPP_3dB
        cmp     r3, #3
        beq     .L28                    @pOutput->amp == MIX_GAIN_3dB

        cmp     r2, #0
        ble     .L38
        subs    r6, r0, #4
        lsl     r8, r2, #1
        movs    r5, #0
.L31:
        ldr     r0, [r6, #4]!
        ldr     r7, [r4, #4]
@///////////////////////////////////////////////////////////
        @add    r3, r6, #4
        @vld1.16        {d0, d1, d2, d3}, [r3]!
        @vld1.16        {d4, d5, d6, d7}, [r3]!
        @vld1.16        {d8, d9, d10, d11}, [r3]!
        @vld1.16        {d12, d13, d14, d15}, [r3]!
        @vqshl.s16    q0, q0, #0
        @vqshl.s16    q1, q1, #0
        @vqshl.s16    q2, q2, #0
        @vqshl.s16    q3, q3, #0
        @vqshl.s16    q4, q4, #0
        @vqshl.s16    q5, q5, #0
        @vqshl.s16    q6, q6, #0
        @vqshl.s16    q7, q7, #0
        @sub    r3, r7, #2
        @vst1.16        {d0, d1, d2, d3}, [r3, #2]!
        @vst1.16        {d4, d5, d6, d7}, [r3, #2]!
        @vst1.16        {d8, d9, d10, d11}, [r3, #2]!
        @vst1.16        {d12, d13, d14, d15}, [r3, #2]!
@///////////////////////////////////////////////////////////
        ssat    r0, #16, r0
        strh    r0, [r7, r5]    @ movhi
        adds    r5, r5, #2
        cmp     r5, r8
        bne     .L31
        b       .L38
.L36:
        cmp     r2, #0
        ble     .L38
        lsl     r8, r2, #1
        movs    r3, #0
.L25:
        ldr     r1, [r4, #4]
@///////////////////////////////////////////////////////////
        @sub    r3, r1, #2
        @vmov.I16       q0, #0
        @vmov.I16       q1, #0
        @vmov.I16       q2, #0
        @vmov.I16       q3, #0
        @vmov.I16       q4, #0
        @vmov.I16       q5, #0
        @vmov.I16       q6, #0
        @vmov.I16       q7, #0
        @vst1.16        {d0, d1, d2, d3}, [r3, #2]!
        @vst1.16        {d4, d5, d6, d7}, [r3, #2]!
        @vst1.16        {d8, d9, d10, d11}, [r3, #2]!
        @vst1.16        {d12, d13, d14, d15}, [r3, #2]!
@///////////////////////////////////////////////////////////
        movs    r2, #0
        strh    r2, [r1, r3]    @ movhi
        adds    r3, r3, #2
        cmp     r3, r8
        bne     .L25
        b       .L38
.L37:
        cmp     r2, #0
        ble     .L38
        subs    r6, r0, #4
        lsl     r8, r2, #1
        movs    r5, #0
.L27:
        ldr     r0, [r6, #4]!
        ldr     r7, [r4, #4]
@///////////////////////////////////////////////////////////
        @add    r3, r6, #4
        @vld1.16        {d0, d1, d2, d3}, [r3]!
        @vld1.16        {d4, d5, d6, d7}, [r3]!
        @vld1.16        {d8, d9, d10, d11}, [r3]!
        @vld1.16        {d12, d13, d14, d15}, [r3]!
        @vqshrn.s16    d0, q0, #1
        @vqshrn.s16    d1, q1, #1
        @vqshrn.s16    d2, q2, #1
        @vqshrn.s16    d3, q3, #1
        @vqshrn.s16    d4, q4, #1
        @vqshrn.s16    d5, q5, #1
        @vqshrn.s16    d6, q6, #1
        @vqshrn.s16    d7, q7, #1
        @sub    r3, r7, #2
        @vst1.8 {d0, d1, d2, d3}, [r3, #2]!
        @vst1.8 {d4, d5, d6, d7}, [r3, #2]!
@///////////////////////////////////////////////////////////
        ssat    r0, #16, r0, asr #1
        strh    r0, [r7, r5]    @ movhi
        adds    r5, r5, #2
        cmp     r5, r8
        bne     .L27
        b       .L38
.L28:
        cmp     r2, #0
        ble     .L38
        subs    r6, r0, #4
        lsl     r8, r2, #1
        movs    r5, #0
.L30:
        ldr     r0, [r6, #4]!
        ldr     r7, [r4, #4]
@///////////////////////////////////////////////////////////
        @add    r3, r6, #4
        @vld1.16        {d0, d1, d2, d3}, [r3]!
        @vld1.16        {d4, d5, d6, d7}, [r3]!
        @vld1.16        {d8, d9, d10, d11}, [r3]!
        @vld1.16        {d12, d13, d14, d15}, [r3]!
        @vqshl.s16    q0, q0, #1
        @vqshl.s16    q1, q1, #1
        @vqshl.s16    q2, q2, #1
        @vqshl.s16    q3, q3, #1
        @vqshl.s16    q4, q4, #1
        @vqshl.s16    q5, q5, #1
        @vqshl.s16    q6, q6, #1
        @vqshl.s16    q7, q7, #1
        @mov    r3, r7
        @vst1.16        {d0, d1, d2, d3}, [r3]!
        @vst1.16        {d4, d5, d6, d7}, [r3]!
        @vst1.16        {d8, d9, d10, d11}, [r3]!
        @vst1.16        {d12, d13, d14, d15}, [r3]!
@///////////////////////////////////////////////////////////
        ssat    r0, #16, r0, lsl #1
        strh    r0, [r7, r5]    @ movhi
        adds    r5, r5, #2
        cmp     r5, r8
        bne     .L30
.L38:
        pop     {r4, r5, r6, r7, r8, pc}
        .fnend
        .size   fastmix_sat_amp, .-fastmix_sat_amp

@----------------------------------------------------------------------
        .section        .text.fastmix_add,"ax",%progbits
        .align  1
        .global fastmix_add
        .thumb
        .thumb_func
        .type   fastmix_add, %function
fastmix_add:
        .fnstart
        @ args = 4, pretend = 0, frame = 0
        @ frame_needed = 0, uses_anonymous_args = 0
        subs    r1, r1, #4
        cmp     r3, #0
        push    {r4, r5, r6, r7}

        mov     ip, #0

        ble     .L39
.L41:
        movs    r5, #0
        cmp     r2, #0
        ble     .L43
        lsl     r7, ip, #1

        movs    r5, #0
        movs    r4, #0

.L42:

        add     r6, r0, r4, lsl #3
        ldr     r6, [r6, #4]
        adds    r4, r4, #1
        cmp     r4, r2
        ldrsh   r6, [r6, r7]
        add     r5, r5, r6

        bne     .L42

.L43:

        add     ip, ip, #1

        cmp     ip, r3

        str     r5, [r1, #4]!

        bne     .L41

.L39:

        pop     {r4, r5, r6, r7}
        bx      lr

        .fnend
        .size   fastmix_add, .-fastmix_add
@----------------------------------------------------------------------
        .section        .text.fastmix_sub_amp,"ax",%progbits
        .align  1
        .global fastmix_sub_amp
        .thumb
        .thumb_func
        .type   fastmix_sub_amp, %function
fastmix_sub_amp:
buf_ptr .req r0
in_ptr  .req r1
out_ptr .req r2
num     .req r3
out_cur .req r5
in_cur  .req r7
buf_cur .req r9
        .fnstart
        @ args = 4, pretend = 0, frame = 8
        @ frame_needed = 0, uses_anonymous_args = 0

        push    {r4, r5, r6, r7, r8, r9, sl, fp, lr}
        @cmp     r3, #0

        @sub     sp, sp, #20

        ldr     r4, [sp, #36]
        lsls    ip, r3, #3      @ sizeof(fastmix_io_t) = 8, assume no overflow
        mov     r9, r0
        mov     r7, r1
        mov     r5, r2

        ble     .L46

        add     ip, ip, r2
        lsls    r6, r4, #1

.L57:   @ outer loop
        ldr    r1, [r5]

        movs    r4, #0
        ldr     r3, [r7, #4]
        mov     r8, r9
        ldr     r2, [r5, #4]

        adds    r0, r1, #1
        beq     .L64    @ bzero

        cmp     r1, #1
        beq     .L65    @ -3dB
        cmp     r1, #3
        beq     .L53    @ 3dB

        @ normal case

.L56:
        ldr     r0, [r8], #4
        ldrsh   r1, [r3, r4]
        subs    r0, r0, r1
        ssat    r0, #16, r0
        strh    r0, [r2, r4]    @ movhi
        adds    r4, r4, #2
        cmp     r4, r6
        bne     .L56

.L49:
        add     r5, r5, #8
        add     r7, r7, #8
        cmp     r5, ip
        bne     .L57
        b       .L46

.L64:   @ bzero case

        movs    r3, #0
.L50:
        strh    r3, [r2, r4]    @ movhi
        adds    r4, r4, #2
        cmp     r4, r6
        bne     .L50
        b       .L49

.L65:   @ -3dB case
.L52:
        ldr     r0, [r8], #4

        ldrsh   r1, [r3, r4]
        subs    r0, r0, r1
        @asrs   r0, r0, #1
        ssat    r0, #16, r0, asr #1
        strh    r0, [r2, r4]    @ movhi
        adds    r4, r4, #2
        cmp     r4, r6
        bne     .L52
        b       .L49

.L53:   @ +3dB case
.L55:
        ldr     r0, [r8], #4

        ldrsh  r1, [r3, r4]
        subs   r0, r0, r1
        ssat   r0, #16, r0, lsl #1
        strh   r0, [r2, r4]    @ movhi
        adds   r4, r4, #2
        cmp    r4, r6
        bne    .L55
@///////////////////////////////////////////////////////////
        @vld1.16 {d16, d17, d18, d19}, [r3]!
        @vld1.16 {d20, d21, d22, d23}, [r3]!
        @vld1.16 {d24, d25, d26, d27}, [r3]!
        @vld1.16 {d28, d29, d30, d31}, [r3]!
        @add     r3, r8, #4
        @vld1.16 {d0, d1, d2, d3}, [r3]!
        @vld1.16 {d4, d5, d6, d7}, [r3]!
        @vld1.16 {d8, d9, d10, d11}, [r3]!
        @vld1.16 {d12, d13, d14, d15}, [r3]!
        @vsub.i16        q0, q0, q8
        @vsub.i16        q1, q1, q9
        @vsub.i16        q2, q2, q10
        @vsub.i16        q3, q3, q11
        @vsub.i16        q4, q4, q12
        @vsub.i16        q5, q5, q13
        @vsub.i16        q6, q6, q14
        @vsub.i16        q7, q7, q15
        @vqshl.s16    q0, q0, #1
        @vqshl.s16    q1, q1, #1
        @vqshl.s16    q2, q2, #1
        @vqshl.s16    q3, q3, #1
        @vqshl.s16    q4, q4, #1
        @vqshl.s16    q5, q5, #1
        @vqshl.s16    q6, q6, #1
        @vqshl.s16    q7, q7, #1
        @add     r3, r6, #4
        @vst1.16 {d0, d1, d2, d3}, [r3]!
        @vst1.16 {d4, d5, d6, d7}, [r3]!
        @vst1.16 {d8, d9, d10, d11}, [r3]!
        @vst1.16 {d12, d13, d14, d15}, [r3]!
@///////////////////////////////////////////////////////////
        b       .L49
.L46:
        @add     sp, sp, #20
        pop     {r4, r5, r6, r7, r8, r9, sl, fp, pc}
        .fnend
        .size   fastmix_sub_amp, .-fastmix_sub_amp
