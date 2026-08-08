/*--------------------------------------------------------------
 * Exynos4412 BL2 启动文件 (参照 STM32 工程的 startup 风格编写)
 *
 * 启动流程:
 *   1. 关闭看门狗, 配置 0x1002330C (时钟启动相关)
 *   2. 进入 SVC 模式并关闭中断 (IRQ/FIQ)
 *   3. 关闭 MMU / D-Cache / I-Cache
 *   4. 建立栈 (IRAM 顶部), 清零 BSS
 *   5. 调用 SystemInit (时钟 + DDR 初始化)
 *   6. 用 iROM 拷贝函数把 SD 卡扇区 49 起的主程序 (512KB)
 *      搬运到 DDR 0x43E00000
 *   7. 跳转到 DDR 0x43E00000 运行 main
 *
 * 说明:
 *   - iROM 拷贝函数指针在 0x02020030 (BL1 启动后有效), 参数为
 *     copy_sd_mmc_to_mem(start_block, block_count, dest_addr), 每块 512 字节
 *   - BL2 只做搬运, 全部业务代码 (LCD/串口等) 都在 main.bin 里,
 *     从而避开 IRAM 14KB 空间限制
 *--------------------------------------------------------------*/
.section .text.entry, "ax"
.arm

/* BL2 头部: 与迅为已验证汇编程序完全一致 (0x2000 0x0 0x0 0x0) */
.word 0x2000
.word 0x0
.word 0x0
.word 0x0

.global _start

_start:
    /* 0. 关闭看门狗 (WTCON @ 0x10060000 = 0):
     *    iROM/BL1 会开启看门狗, 若不关闭, 每隔几秒系统自动复位,
     *    导致程序不断重启、串口只看到零星字节 */
    ldr     r0, =0x10060000
    mov     r1, #0
    str     r1, [r0]

    /* 0. 与迅为 iTOP-4412 已验证可用的汇编程序保持一致:
     *    把 0x1002330C 的 bit[9:8] 置 1 (时钟启动相关配置, 见 4412 手册) */
    ldr     r0, =0x1002330C
    ldr     r1, [r0]
    orr     r1, r1, #0x300
    str     r1, [r0]

    /* 0b. 与已验证程序一致: GPX0PUD (0x11000C08) 清零, 关闭 GPX0 上下拉 */
    ldr     r0, =0x11000C08
    mov     r1, #0
    str     r1, [r0]

    /* 1. 进入 SVC 模式, 关闭 IRQ/FIQ */
    mrs     r0, cpsr
    bic     r0, r0, #0x1F
    orr     r0, r0, #0xD3
    msr     cpsr, r0

    /* 2. 关闭 MMU / D-Cache / I-Cache */
    mrc     p15, 0, r0, c1, c0, 0
    bic     r0, r0, #0x00002000   /* I(bit12): 关闭 I-Cache */
    bic     r0, r0, #0x00000007   /* M/C/A(bit0-2): 关闭 MMU/D-Cache/对齐 */
    bic     r0, r0, #0x00000002   /* A(bit1): 保持关闭对齐检查 (允许未对齐访问) */
    orr     r0, r0, #0x00000800   /* Z(bit11): 开启 BTB */
    mcr     p15, 0, r0, c1, c0, 0

    /* 无效化 TLB 和 I-Cache */
    mov     r0, #0
    mcr     p15, 0, r0, c8, c7, 0
    mcr     p15, 0, r0, c7, c5, 0
    dsb
    isb

    /* 3. 栈指针指向 IRAM 顶部 (链接脚本定义 __stack_top) */
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

    /* 4. 系统初始化 (时钟 / DDR, 见 system_4412.c) */
    bl      SystemInit

    /* 5. 快速初始化 UART2 (仅用于 BL2 阶段调试打印) */
    bl      UART2_QuickInit

    ldr     r0, =msg_init_ok
    bl      UART2_Print

    /* 6. 通过 iROM 拷贝函数搬运主程序:
     *    copy_sd_mmc_to_mem(49, 1024, 0x43E00000)
     *    扇区 49 起 1024 块 = 512KB, 与烧录脚本布局一致 */
    ldr     r3, =0x02020030
    ldr     r3, [r3]              /* 取 iROM 函数指针 */
    mov     r0, #49               /* 起始扇区 */
    mov     r1, #1024             /* 块数 (每块 512 字节) */
    ldr     r2, =0x43E00000       /* 目标地址 (DDR) */
    blx     r3

    /* 打印搬运后主程序首 4 字节, 用于校验 SD 读取是否正确 */
    ldr     r0, =msg_copy_ok
    bl      UART2_Print
    ldr     r0, =0x43E00000
    ldr     r0, [r0]
    bl      UART2_PrintHex32
    ldr     r0, =msg_crlf
    bl      UART2_Print

    /* 7. 跳转到 DDR 运行主程序 */
    ldr     pc, =0x43E00000

/* 主程序跳转失败时死循环 */
3:
    b       3b

/*--------------------------------------------------------------
 * UART2 快速初始化 (GPA1_0/1 复用功能 2, 115200 8N1, SCLK_UART=100MHz)
 * 与主程序中的 UART_Init 参数一致: ULCON=0x3, UCON=0x3C5,
 * UFCON=0x111, UBRDIV=53, UDIVSLOT=4
 *--------------------------------------------------------------*/
UART2_QuickInit:
    /* GPA1CON = 0x22222222 (全部复用功能 2) */
    ldr     r0, =0x11400020
    ldr     r1, =0x22222222
    str     r1, [r0]
    /* GPA1PUD = 0 (无上下拉) */
    ldr     r0, =0x11400028
    mov     r1, #0
    str     r1, [r0]

    /* UART2 寄存器初始化 */
    ldr     r0, =0x13820000
    ldr     r1, =0x3              /* ULCON: 8N1 */
    str     r1, [r0]
    ldr     r1, =0x3C5            /* UCON: 收发使能, SCLK_UART */
    str     r1, [r0, #0x04]
    ldr     r1, =0x111            /* UFCON: FIFO 使能 */
    str     r1, [r0, #0x08]
    mov     r1, #0                /* UMCON: 无流控 */
    str     r1, [r0, #0x0C]
    ldr     r1, =53               /* UBRDIV: 115200 */
    str     r1, [r0, #0x28]
    ldr     r1, =4                /* UDIVSLOT */
    str     r1, [r0, #0x2C]
    mov     pc, lr

/*--------------------------------------------------------------
 * UART2 打印字符串 (r0 = 字符串地址, 盲写 + 延时, 不等待标志)
 *--------------------------------------------------------------*/
UART2_Print:
    ldr     r1, =0x13820000
1:
    ldrb    r2, [r0], #1
    cmp     r2, #0
    moveq   pc, lr
    str     r2, [r1, #0x20]       /* UTXH */
    ldr     r3, =0x9000
2:
    subs    r3, r3, #1
    bne     2b
    b       1b

/*--------------------------------------------------------------
 * UART2 打印 32 位十六进制 (r0 = 数据)
 *--------------------------------------------------------------*/
UART2_PrintHex32:
    stmfd   sp!, {r4-r6, lr}
    mov     r4, r0                /* 保存数据 */
    ldr     r5, =hex_chars
    mov     r6, #8                /* 8 个十六进制字符 */
1:
    mov     r0, r4, lsr #28
    ldrb    r0, [r5, r0]
    ldr     r1, =0x13820000
    str     r0, [r1, #0x20]
    ldr     r2, =0x9000
2:
    subs    r2, r2, #1
    bne     2b
    mov     r4, r4, lsl #4
    subs    r6, r6, #1
    bne     1b
    ldmfd   sp!, {r4-r6, pc}

/* 只读字符串与数据 (放在 .rodata, 避免 BL2 空间浪费) */
.section .rodata
msg_init_ok:
    .asciz  "BL2: clocks+DDR ok\r\n"
msg_copy_ok:
    .asciz  "BL2: main copied to 0x43E00000, head="
msg_crlf:
    .asciz  "\r\n"
hex_chars:
    .asciz  "0123456789ABCDEF"

.end
