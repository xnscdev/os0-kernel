import gdb

def stringconv(val, max_len=0):
    if max_len > 0:
        return val.string(encoding="ascii", length=max_len)
    else:
        return val.string(encoding="ascii")
