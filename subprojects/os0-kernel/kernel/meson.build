kernel_src = [
  'boot.S',
  'main.c',
  'memory.c',
  'paging.c',
  'paging.S'
]

kernel = executable('kernel', kernel_src, link_args: [
		      '-T',
		      meson.current_source_dir() / 'linker.ld',
		      '-nostdlib',
		    ], link_with: [arch, drivers, libk],
		    include_directories: includedir, install_dir: '/boot',
		    link_depends: files('linker.ld'), install: true)

if grub_file.found()
  test('x86 multiboot', grub_file, args: ['--is-x86-multiboot', kernel])
endif
