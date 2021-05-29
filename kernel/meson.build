kernel_src = [
  'boot.S', # First so multiboot is in first 8K of file */
  'fcntl.c',
  'fork.S',
  'heap.c',
  'ioctl.c',
  'main.c',
  'memory.c',
  'process.c',
  'wait.c'
]

kernel = executable('kernel', kernel_src, link_args: [
		      '-T',
		      meson.current_source_dir() / 'linker.ld',
		      '-nostdlib',
		      '-lgcc',
		    ], link_with: [arch, drivers, fs, libk, syscall],
		    include_directories: includedir, install_dir: '/boot',
		    link_depends: files('linker.ld'), install: true)

if grub_file.found()
  test('x86 multiboot', grub_file, args: ['--is-x86-multiboot', kernel])
endif
