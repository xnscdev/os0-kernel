# GDB pretty-printer auto-load script

import gdb.printing
import printers

gdb.printing.register_pretty_printer(gdb.current_objfile(),
                                     printers.build_pretty_printer())
