project('os0-kernel', 'c', version: '0.0.1',
	default_options: ['c_std=gnu99', 'b_staticpic=false'],
	license: 'GPL-3.0-or-later', meson_version: '>= 0.55.0')
add_project_arguments('-ffreestanding', '-fno-common', language: 'c',
		      native: false)

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

configure_file(input: 'kconfig.h.in', output: 'kconfig.h',
	       configuration: kernel_conf)

subdir('drivers')
subdir('fs')
subdir('libk')

subdir('kernel')
subdir('include/bits')
subdir('include/sys')
