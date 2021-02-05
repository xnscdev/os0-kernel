ENTRY (_start)

OFFSET = 0xc0000000;

SECTIONS
{
  . = 1M;

  .multiboot.data :
  {
    *(.multiboot.data)
  }

  .multiboot.text :
  {
    *(.multiboot.text)
  }

  . += OFFSET;
  _kernel_start = .;

  .text ALIGN (4K) : AT (ADDR (.text) - OFFSET)
  {
    *(.text)
  }

  .rodata ALIGN (4K) : AT (ADDR (.rodata) - OFFSET)
  {
    *(.rodata)
  }

  .data ALIGN (4K) : AT (ADDR (.data) - OFFSET)
  {
    *(.data)
  }

  .bss ALIGN (4K) : AT (ADDR (.bss) - OFFSET)
  {
    *(COMMON)
    *(.bss)
    *(.bootstrap_stack)
  }

  _kernel_end = .;
}
