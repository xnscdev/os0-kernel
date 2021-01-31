project('os0-kernel', 'c', version: '0.0.1', default_options: 'c_std=gnu99',
	license: 'GPL-3.0-or-later', meson_version: '>= 0.55.0')
add_project_arguments('-DVERSION="@0@"'.format(meson.project_version()),
		      '-ffreestanding', language: 'c', native: false)

grub_file = find_program('grub-file', required: false)

includedir = include_directories('include')

if host_machine.cpu_family() == 'x86'
  subdir('arch/x86')
else
  error('host CPU type not supported or invalid')
endif

subdir('drivers')
subdir('libk')

subdir('kernel')
