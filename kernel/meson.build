# The boot source file needs to be the first object linked into the kernel image
# so the multiboot header will be in the first 8 KiB of the file and GRUB will
# be able to find it
kernel_src = [
  'boot-' + machine_arch + '.S',
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
