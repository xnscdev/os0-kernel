project('os0-kernel', 'c', version: '0.0.1',
	default_options: ['c_std=gnu99', 'b_staticpic=false'],
	license: 'GPL-3.0-or-later', meson_version: '>= 0.55.0')
add_project_arguments('-ffreestanding', '-fno-common', '-U_FILE_OFFSET_BITS',
		      '-DKERNEL=1', language: 'c', native: false)

cc = meson.get_compiler('c')
if cc.has_argument('-Wpointer-sign')
  add_project_arguments('-Wno-pointer-sign', language: 'c')
endif
if cc.has_argument('-Wrestrict')
  add_project_arguments('-Wno-restrict', language: 'c')
endif
if cc.has_argument('-Wformat')
  add_project_arguments('-Wno-format', language: 'c')
endif

grub_file = find_program('grub-file', required: false)

includedir = include_directories('.', 'include')

kernel_conf = configuration_data()
kernel_conf.set_quoted('VERSION', meson.project_version())

hostcpu = host_machine.cpu_family()
if hostcpu == 'x86'
  machine_arch = 'i386'
else
  error('Unsupported CPU type: ' + hostcpu)
endif

subdir('arch' / machine_arch)
kernel_conf.set10('ARCH_' + machine_arch.to_upper(), true)

kernel_conf.set('PROCESS_LIMIT', get_option('process_limit'))
kernel_conf.set('PROCESS_FILE_LIMIT', get_option('fd_limit'))
kernel_conf.set('PROCESS_SYS_FILE_LIMIT', get_option('sys_fd_limit'))
kernel_conf.set('PROCESS_MMAP_LIMIT', get_option('mmap_limit'))

kernel_conf.set('ATA_DMA', get_option('ata_dma'))

configure_file(input: 'kconfig.h.in', output: 'kconfig.h',
	       configuration: kernel_conf)

subdir('drivers')
subdir('fs')
subdir('libk')
subdir('uapi')

subdir('kernel')
subdir('include')
