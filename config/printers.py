# GDB pretty-printers for OS/0 kernel types, for debugging

import gdb.printing

class ProcessTaskPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return "{PID: %d, PPID: %d, ESP: %#x, EIP: %#x, Page dir: %#x}" %
            (self.val["t_pid"], self.val["t_ppid"], self.val["t_esp"],
             self.val["t_eip"], self.val["t_pgdir"])

def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("os0-kernel")
    pp.add_printer("ProcessTask", "^ProcessTask$", ProcessTaskPrinter)
    return pp
