/*--------------------------------------------------------------
 * Exynos4412 BL2 链接脚本
 *
 * 链接基址由 Makefile 通过 --defsym __LINK_BASE=0x02023400 传入:
 *   BL2 由 BL1 加载到 IRAM 0x02023400 运行 (与迅为已验证程序一致)
 *
 * 本阶段只做: 时钟 + DDR 初始化 + 用 iROM 拷贝函数把主程序搬到 DDR,
 * 所以镜像必须小于 14KB-4 字节。
 *--------------------------------------------------------------*/
ENTRY(_start)

SECTIONS
{
    . = __LINK_BASE;

    .text : {
        KEEP(*(.text.entry))      /* _start 必须位于镜像最前 */
        *(.text .text.*)
        . = ALIGN(4);
    }

    .rodata : {
        *(.rodata .rodata.*)
        . = ALIGN(4);
    }

    .data : {
        *(.data .data.*)
        . = ALIGN(4);
    }

    __bss_start = .;
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
    }
    __bss_end = .;

    /* 栈固定在 IRAM 顶部 (0x02020000 + 256KB) */
    __stack_top = 0x02060000;
}
