project('OS/0 kernel', 'c', version: '0.0.1', default_options: 'c_std=gnu99',
	license: 'GPL-3.0-or-later', meson_version: '>= 0.55.0')
add_project_arguments('-DVERSION="@0@"'.format(meson.project_version()),
		      '-ffreestanding', language: 'c', native: false)

grub_file = find_program('grub-file', required: false)

kernel_inc = include_directories(['drivers', 'kernel'])

subdir('drivers')
subdir('kernel')
