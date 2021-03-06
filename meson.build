project('os0-kernel', 'c', version: '0.0.1',
	default_options: ['c_std=gnu99', 'b_staticpic=false'],
	license: 'GPL-3.0-or-later', meson_version: '>= 0.55.0')
add_project_arguments('-ffreestanding', '-fno-common', '-U_FILE_OFFSET_BITS',
		      language: 'c', native: false)

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

if host_machine.cpu_family() == 'x86'
  subdir('arch/i386')
  kernel_conf.set10('ARCH_I386', true)
  kernel_conf.set10('INVLPG_SUPPORT',
		    host_machine.cpu().version_compare('>= i486'))
else
  error('host CPU type not supported or invalid')
endif

kernel_conf.set('PROCESS_LIMIT', get_option('process_limit'))
kernel_conf.set('PROCESS_FILE_LIMIT', get_option('fd_limit'))
kernel_conf.set('PROCESS_SYS_FILE_LIMIT', get_option('sys_fd_limit'))
kernel_conf.set('PROCESS_MMAP_LIMIT', get_option('mmap_limit'))

configure_file(input: 'kconfig.h.in', output: 'kconfig.h',
	       configuration: kernel_conf)

subdir('drivers')
subdir('fs')
subdir('libk')
subdir('uapi')

subdir('kernel')
subdir('include')
