configure_file(input: 'syslimits.h.in', output: 'syslimits.h',
	       configuration: kernel_conf,
	       install_dir: get_option('includedir') / 'sys')

install_headers('cdefs.h',
		'errno.h',
		'fcntl.h',
		'io.h',
		'ioctl.h',
		'param.h',
		'signal.h',
		'sysmacros.h',
		'types.h',
		'unistd.h', subdir: 'sys')
