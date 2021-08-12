kernel_conf.set10('INVLPG_SUPPORT', hostcpu.version_compare('>= i486'))

arch_src = [
  'gdt-load.S',
  'features.c',
  'features.S',
  'gdt.c',
  'halt.S',
  'isr.c',
  'isr.S',
  'memcmp.S',
  'memchr.S',
  'memcpy.S',
  'memmove.S',
  'memset.S',
  'paging.c',
  'paging.S',
  'pic.c',
  'pregs.S',
  'random.S',
  'syscall.S',
  'rtc.c',
  'task.c',
  'task.S',
  'timer.c',
  'tss.S'
]

arch = static_library('arch', arch_src, include_directories: includedir)
