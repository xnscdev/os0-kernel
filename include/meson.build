configure_file(input: 'endian.h.in', output: 'endian.h',
	       configuration: kernel_conf,
	       install_dir: get_option('includedir'))

install_headers('byteswap.h',
		'elf.h')

subdir('bits')
subdir('sys')
