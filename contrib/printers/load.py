import gdb.printing
import inspect
import os
import sys

# Add this directory to the python path
d = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
if not d in sys.path:
    sys.path.insert(0, d)

import ext2

def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("os0")
    pp.add_printer("Ext2DirEntry", "^Ext2DirEntry$", ext2.Ext2DirEntryPrinter)
    return pp

# Register the printers
gdb.printing.register_pretty_printer(gdb.current_objfile(),
                                     build_pretty_printer())
