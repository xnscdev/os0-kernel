import gdb.printing
from util import stringconv

DIR_FILE_TYPES = (
    "Unknown",
    "Regular file",
    "Directory",
    "Character device",
    "Block device",
    "FIFO",
    "Socket",
    "Symbolic link"
)

class Ext2DirEntryPrinter:
    "Print a formatted ext2 directory entry"

    def __init__(self, val):
        self.val = val
        self.name_len = self.val["d_name_len"]
        self.file_type = 0
        if self.name_len > 0xff:
            self.file_type = self.name_len >> 8
            self.name_len &= 0xff
        self.file_type %= 8

    def to_string(self):
        s = "{Inode: " + str(self.val["d_inode"]) + ", "
        s += "File type: " + DIR_FILE_TYPES[self.file_type] + ", "
        s += "Record length: " + str(self.val["d_rec_len"]) + ", "
        s += "Name length: " + str(self.name_len) + ", "
        s += "Name: " + stringconv(self.val["d_name"], self.name_len) + "}"
        return s
