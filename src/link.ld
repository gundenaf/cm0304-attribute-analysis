OUTPUT_FORMAT(elf32-i386)
ENTRY(radar_hook)
SECTIONS {
 . = 0x00F9E000;
 .text : { *(.text.hook) *(.text .text.*) *(.rodata .rodata.*) *(.data .data.*) *(.bss .bss.* COMMON) }
 /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) *(.got*) *(.plt*) *(.rel*) }
}
