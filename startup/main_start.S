/*--------------------------------------------------------------
 * Exynos4412 主程序 (DDR) 入口文件
 *
 * 由 BL2 把本镜像从 SD 卡搬运到 DDR 0x43E00000 后跳转到这里:
 *   1. 栈指针指向 DDR 顶部 0x7FFFE000 (1GB DDR 顶端留 8KB 余量)
 *   2. 清零 BSS
 *   3. 跳转 C 语言的 main()
 *
 * 说明: 所有业务代码 (LCD/串口/LED 等) 都运行在 DDR 中,
 *       不再受 IRAM 14KB 的 BL2 空间限制。
 *--------------------------------------------------------------*/
.section .text.entry, "ax"
.arm

.global _start

_start:
    /* 关闭对齐检查 (A=0): 面板字符串拼接会产生未对齐的半字存储,
     * 若开启对齐检查会触发数据对齐异常 (FAULT D, DFSR=0x801) */
    mrc     p15, 0, r0, c1, c0, 0
    bic     r0, r0, #0x00000002
    mcr     p15, 0, r0, c1, c0, 0
    dsb
    isb

    /* 设置异常向量表基地址 (诊断用: 捕获数据访问异常并打印故障信息) */
    ldr     r0, =VectorTable
    mcr     p15, 0, r0, c12, c0, 0
    dsb
    isb

    /* 栈指针 = DDR 顶部 (链接脚本定义 __stack_top = 0x7FFFE000) */
    ldr     sp, =__stack_top

    /* 清零 BSS */
    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    mov     r2, #0
1:
    cmp     r0, r1
    bge     2f
    str     r2, [r0], #4
    b       1b
2:

    /* 进入 C 主函数 */
    bl      main

/* main 返回后死循环 */
3:
    b       3b

/*--------------------------------------------------------------
 * 异常向量表 + 故障打印 (诊断用)
 *
 * 若程序发生数据访问异常/未定义指令等, CPU 会跳到对应向量,
 * 这里打印故障类型 + DFSR(故障状态) + DFAR(故障地址) + PC,
 * 然后死循环, 便于定位是"异常"还是"死循环"导致的卡死。
 *--------------------------------------------------------------*/
.section .text.vectors, "ax"
.balign 32
.global VectorTable
VectorTable:
    b   .Lvec_reset
    b   .Lvec_undef
    b   .Lvec_swi
    b   .Lvec_prefetch
    b   .Lvec_dabort
    b   .Lvec_reserved
    b   .Lvec_irq
    b   .Lvec_fiq

.Lvec_reset:   b .Lvec_reset
.Lvec_undef:   mov r0, #'U'; b .Lvec_fault
.Lvec_swi:     mov r0, #'S'; b .Lvec_fault
.Lvec_prefetch: mov r0, #'P'; b .Lvec_fault
.Lvec_dabort:  mov r0, #'D'; b .Lvec_fault
.Lvec_reserved: b .Lvec_reserved
.Lvec_irq:     mov r0, #'I'; b .Lvec_fault
.Lvec_fiq:     mov r0, #'F'; b .Lvec_fault

/* r0 = 故障类型字符; 保存 DFSR/DFAR/PC 后打印并挂起 */
.Lvec_fault:
    ldr     r1, =FaultRec
    str     r0, [r1, #0]          /* 类型 */
    mrc     p15, 0, r2, c5, c0, 0 /* DFSR */
    str     r2, [r1, #4]
    mrc     p15, 0, r2, c6, c0, 0 /* DFAR */
    str     r2, [r1, #8]
    mov     r2, lr
    sub     r2, r2, #8            /* 故障 PC */
    str     r2, [r1, #12]
    /* 切到 SVC 模式, 屏蔽中断, 使用主栈 */
    mrs     r2, cpsr
    bic     r2, r2, #0x1F
    orr     r2, r2, #0xD3
    msr     cpsr, r2
    /* 打印 "\r\nFAULT " */
    ldr     r5, =0x13820000       /* UART2 */
    ldr     r6, =FaultMsg
1:  ldrb    r2, [r6], #1
    cmp     r2, #0
    beq     2f
    str     r2, [r5, #0x20]       /* UTXH */
    bl      .Lvec_delay
    b       1b
2:
    /* 打印类型字符 */
    ldr     r2, [r1, #0]
    str     r2, [r5, #0x20]
    bl      .Lvec_delay
    /* 打印 DFSR / DFAR / PC (从 offset 4 起 3 个 32 位) */
    mov     r4, #3
3:
    ldr     r0, [r1, r4, lsl #2]
    bl      .Lvec_hex32
    subs    r4, r4, #1
    bne     3b
.Lvec_hang:
    b       .Lvec_hang

/* r0 = 32 位值, r5 = UART2 基址; 打印 8 位十六进制 */
.Lvec_hex32:
    mov     r7, #8
4:
    mov     r2, r0, lsr #28
    and     r2, r2, #0xF
    ldr     r8, =FaultHex
    ldrb    r2, [r8, r2]
    str     r2, [r5, #0x20]
    /* 内联延时 (不能用 bl, 会覆盖 lr) */
    mov     r9, #0x8000
5:  subs    r9, r9, #1
    bne     5b
    mov     r0, r0, lsl #4
    subs    r7, r7, #1
    bne     4b
    mov     pc, lr

.Lvec_delay:
    mov     r9, #0x8000
5:  subs    r9, r9, #1
    bne     5b
    mov     pc, lr

.section .rodata
FaultMsg:
    .asciz  "\r\nFAULT "
FaultHex:
    .ascii  "0123456789ABCDEF"

.section .bss
.balign 8
FaultRec:
    .space  16

.end
