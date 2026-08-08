/*--------------------------------------------------------------
 * 软件除法辅助函数 (供 -nostdlib 工程使用)
 *
 * 在 ARM Cortex-A9 上没有硬件除法指令, GCC 对 32 位整数除法
 * 会生成对 __aeabi_uidiv / __aeabi_uidivmod 的调用, 这里给出
 * 最小实现, 避免依赖 libgcc。
 *
 * 入口: r0 = 被除数, r1 = 除数
 * 出口: r0 = 商, r1 = 余数 (uidivmod)
 *--------------------------------------------------------------*/
.text
.arm

.global __aeabi_uidiv
.global __aeabi_uidivmod

__aeabi_uidivmod:
__aeabi_uidiv:
    stmfd   sp!, {r4, lr}

    cmp     r1, #0
    beq     .Ldiv_zero

    mov     r2, r0              /* r2 = 被除数 */
    mov     r4, r1              /* r4 = 保存除数 */
    mov     r0, #0              /* r0 = 商 */
    mov     r3, #1              /* r3 = 当前位权 */

    /* 把除数左移, 直到大于 被除数/2 */
.Lalign:
    cmp     r1, r2, lsr #1
    bhi     .Lloop
    mov     r1, r1, lsl #1
    mov     r3, r3, lsl #1
    b       .Lalign

.Lloop:
    cmp     r2, r1
    subhs   r2, r2, r1
    orrhs   r0, r0, r3
    mov     r1, r1, lsr #1
    movs    r3, r3, lsr #1
    bne     .Lloop

    mov     r1, r2              /* 余数 */
    ldmfd   sp!, {r4, pc}

.Ldiv_zero:
    mov     r0, #0
    mov     r1, #0
    ldmfd   sp!, {r4, pc}

.end
