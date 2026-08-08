/*--------------------------------------------------------------
 * Exynos4412 主程序 (DDR) 链接脚本
 *
 * 镜像被 BL2 从 SD 卡扇区 49 搬运到 DDR 0x43E00000 运行,
 * 所以链接地址固定为 0x43E00000 (与 U-Boot 的 CFG_PHY_UBOOT_BASE 相同)。
 *
 * 内存布局:
 *   0x43E00000 : main.bin (代码 + 只读数据 + 数据, 容量 512KB)
 *   0x50000000 : LCD 帧缓冲 (800*1280*4 字节 ≈ 4MB)
 *   0x7FFFE000 : 栈顶 (向下增长)
 *--------------------------------------------------------------*/
ENTRY(_start)

SECTIONS
{
    . = 0x43E00000;

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

    /* 栈固定在 1GB DDR 顶部 (0x40000000 + 1GB = 0x80000000, 留 8KB) */
    __stack_top = 0x7FFFE000;
}
