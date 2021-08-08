# Test suite for i386 targets
if machine_arch == 'i386'
  # If grub-file is available we can use it to check the kernel image for a
  # valid multiboot header.
  grub_file = find_program('grub-file', required: false)
  if grub_file.found()
    test('i386 multiboot', grub_file, args: ['--is-x86-multiboot', kernel])
  endif

  # If QEMU for i386 is available we can use it to test various utilities
  # in the kernel.
  qemu = find_program('qemu-system-i386', required: false, native: true,
		      version: '>= 2.0')
  if qemu.found()
    test_ldflags = [
      '-T',
      meson.current_source_dir() / 'testlink.ld',
      '-nostdlib',
      '-lgcc'
    ]

    # Testing utility library, will be linked into all test binaries
    testutil_src = [
      'start.S',
      '../arch/i386/memcmp.S',
      '../arch/i386/memcpy.S',
      '../arch/i386/memmove.S',
      '../arch/i386/memset.S',
      '../drivers/serial.c',
      '../libk/itoa.c',
      '../libk/printk.c',
      '../libk/string.c',
      'main.c'
    ]
    testutil = static_library('testutil', testutil_src,
			      include_directories: includedir,
			      c_args: ['-DTEST=1'])
    testutil_dep = declare_dependency(link_with: testutil,
				      include_directories: includedir)

    # Test driver program
    test_driver = executable('test-driver', 'test-driver.c', native: true)

    sha256 = executable('sha256', ['../libk/sha256.c', 'sha256.c'],
			dependencies: testutil_dep, link_args: test_ldflags)
    test('SHA-256', test_driver, args: [qemu.full_path(), sha256.full_path()])
  endif
endif
