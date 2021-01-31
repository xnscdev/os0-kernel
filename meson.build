project('os0', 'c', version: '0.0.1', license: 'GPL-3.0-or-later')

subproject('os0-kernel')

install_data('config/grub.cfg', install_dir: '/boot/grub',
	     install_mode: 'r--r--r--')
