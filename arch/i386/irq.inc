/* Exception handlers
   First parameter: exception code
   Second parameter: whether to pop four bytes after calling handler */

EXC (0, 0)
EXC (1, 0)
EXC (2, 0)
EXC (3, 0)
EXC (4, 0)
EXC (5, 0)
EXC (6, 0)
EXC (7, 0)
EXC (8, 1)
EXC (10, 1)
EXC (11, 1)
EXC (12, 1)
EXC (13, 1)
EXC (14, 1)
EXC (16, 0)
EXC (17, 1)
EXC (18, 0)
EXC (19, 0)
EXC (20, 0)
EXC (30, 1)

/* Interrupt handlers
   First parameter: IRQ number */

IRQ (0)
IRQ (1)
IRQ (2)
IRQ (3)
IRQ (4)
IRQ (5)
IRQ (6)
IRQ (7)
#ifndef IRQ_SKIP_8
IRQ (8)
#endif
IRQ (9)
IRQ (10)
IRQ (11)
IRQ (12)
IRQ (13)
IRQ (14)
IRQ (15)
