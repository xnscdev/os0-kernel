subdir('ext2')

fs_src = [
  'devfs.c',
  'fsguess.c',
  'path.c',
  'perm.c',
  'pipe.c',
  'vfs.c'
]

fs = static_library('fs', fs_src, include_directories: includedir,
		    link_with: fs_ext2)
