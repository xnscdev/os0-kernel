/*************************************************************************
 * termios.h -- This file is part of OS/0.                               *
 * Copyright (C) 2021 XNSC                                               *
 *                                                                       *
 * OS/0 is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * OS/0 is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with OS/0. If not, see <https://www.gnu.org/licenses/>.         *
 *************************************************************************/

#ifndef _BITS_TERMIOS_H
#define _BITS_TERMIOS_H

#ifndef _TERMIOS_H
#error  "<bits/termios.h> should not be included directly"
#endif

#define NCCS 32

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

struct termios
{
  tcflag_t c_iflag;
  tcflag_t c_oflag;
  tcflag_t c_cflag;
  tcflag_t c_lflag;
  cc_t c_line;
  cc_t c_cc[NCCS];
  speed_t c_ispeed;
  speed_t c_ospeed;
};

#define VINTR    0
#define VQUIT    1
#define VERASE   2
#define VKILL    3
#define VEOF     4
#define VTIME    5
#define VMIN     6
#define VSWTC    7
#define VSTART   8
#define VSTOP    9
#define VSUSP    10
#define VEOL     11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE  14
#define VLNEXT   15
#define VEOL2    16

#define IGNBRK   0x0001
#define BRKINT   0x0002
#define IGNPAR   0x0004
#define PARMRK   0x0008
#define INPCK    0x0010
#define ISTRIP   0x0020
#define INLCR    0x0040
#define IGNCR    0x0080
#define ICRNL    0x0100
#define IUCLC    0x0200
#define IXON     0x0400
#define IXANY    0x0800
#define IXOFF    0x1000
#define IMAXBEL  0x2000
#define IUTF8    0x4000

#define OPOST  0x01
#define OLCUC  0x02
#define ONLCR  0x04
#define OCRNL  0x08
#define ONOCR  0x10
#define ONLRET 0x20
#define OFILL  0x40
#define OFDEL  0x80

#define NLDLY  0x0100
#define NL0    0x0000
#define NL1    0x0100

#define CRDLY  0x0600
#define CR0    0x0000
#define CR1    0x0200
#define CR2    0x0400
#define CR3    0x0600

#define TABDLY 0x1800
#define TAB0   0x0000
#define TAB1   0x0800
#define TAB2   0x1000
#define TAB3   0x1800
#define XTABS  0x1800

#define BSDLY  0x2000
#define BS0    0x0000
#define BS1    0x2000

#define VTDLY  0x4000
#define VT0    0x0000
#define VT1    0x4000

#define FFDLY  0x8000
#define FF0    0x0000
#define FF1    0x8000

#define B0     0x0
#define B50    0x1
#define B75    0x2
#define B110   0x3
#define B134   0x4
#define B150   0x5
#define B200   0x6
#define B300   0x7
#define B600   0x8
#define B1200  0x9
#define B1800  0xa
#define B2400  0xb
#define B4800  0xc
#define B9600  0xd
#define B19200 0xe
#define B38400 0xf

#define CSIZE  0x0030
#define CS5    0x0000
#define CS6    0x0010
#define CS7    0x0020
#define CS8    0x0030

#define CSTOPB 0x0040
#define CREAD  0x0080
#define PARENB 0x0100
#define PARODD 0x0200
#define HUPCL  0x0400
#define CLOCAL 0x0800

#define B57600   0x1001
#define B115200  0x1002
#define B230400  0x1003
#define B460800  0x1004
#define B500000  0x1005
#define B576000  0x1006
#define B921600  0x1007
#define B1000000 0x1008
#define B1152000 0x1009
#define B1500000 0x100a
#define B2000000 0x100b
#define B2500000 0x100c
#define B3000000 0x100d
#define B3500000 0x100e
#define B4000000 0x100f

#define ISIG    0x0001
#define ICANON  0x0002
#define XCASE   0x0004
#define ECHO    0x0008
#define ECHOE   0x0010
#define ECHOK   0x0020
#define ECHONL  0x0040
#define NOFLSH  0x0080
#define TOSTOP  0x0100
#define ECHOCTL 0x0200
#define ECHOPRT 0x0400
#define ECHOKE  0x0800
#define FLUSHO  0x1000
#define PENDIN  0x4000
#define IEXTEN  0x8000
#define EXTPROC 0x10000

#define TCOOFF 0
#define TCOON  1
#define TCIOFF 2
#define TCION  3

#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#endif
