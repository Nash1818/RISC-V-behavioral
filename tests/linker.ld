ENTRY(_start)

MEMORY
{
  ram (rwx) : ORIGIN = 0x80000000, LENGTH = 64M
}

SECTIONS
{
  .text   : { *(.text*) } > ram
  .rodata : { *(.rodata*) } > ram
  .data   : { *(.data*) }  > ram
  .bss    : { *(.bss*) }   > ram

  /* --- stack 256 KiB above the code, well inside ±1 MiB --- */
  . = 0x80040000;                 /* pick any addr < 0x80080000 */
  PROVIDE(__stack_top = .);
}
