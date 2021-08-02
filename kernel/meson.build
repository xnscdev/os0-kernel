if host_machine.cpu_family() == 'x86'
  boot_obj = 'boot-i386.S'
else
  error('No suitable boot object for architecture')
endif

kernel_src = [
  boot_obj, # Needs to be first so it is in first 8K of image
  'fcntl.c',
  'heap.c',
  'ioctl.c',
  'main.c',
  'memory.c',
  'process.c',
  'rtld.c',
  'wait.c'
]

kernel = executable('kernel', kernel_src, link_args: [
		      '-T',
		      meson.current_source_dir() / 'linker.ld',
		      '-nostdlib',
		      '-lgcc',
		    ], link_with: [arch, drivers, fs, libk, uapi],
		    include_directories: includedir, install_dir: '/boot',
		    link_depends: files('linker.ld'), install: true)

if grub_file.found()
  test('x86 multiboot', grub_file, args: ['--is-x86-multiboot', kernel])
endif
