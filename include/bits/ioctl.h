/*************************************************************************
 * ioctl.h -- This file is part of OS/0.                                 *
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

#ifndef _BITS_IOCTL_H
#define _BITS_IOCTL_H

#ifndef _SYS_IOCTL_H
#error "<bits/ioctl.h> should not be included directly"
#endif

#define TCGETS          0x5401
#define TCSETS          0x5402
#define TCSETSW         0x5403
#define TCSETSF         0x5404
#define TCGETA          0x5405
#define TCSETA          0x5406
#define TCSETAW         0x5407
#define TCSETAF         0x5408
#define TCSBRK          0x5409
#define TCXONC          0x540a
#define TCFLSH          0x540b
#define TIOCEXCL        0x540c
#define TIOCNXCL        0x540d
#define TIOCSCTTY       0x540e
#define TIOCGPGRP       0x540f
#define TIOCSPGRP       0x5410
#define TIOCOUTQ        0x5411
#define TIOCSTI         0x5412
#define TIOCGWINSZ      0x5413
#define TIOCSWINSZ      0x5414
#define TIOCMGET        0x5415
#define TIOCMBIS        0x5416
#define TIOCMBIC        0x5417
#define TIOCMSET        0x5418
#define TIOCGSOFTCAR    0x5419
#define TIOCSSOFTCAR    0x541a
#define TIOCINQ         0x541b
#define FIONREAD        TIOCINQ
#define TIOCCONS        0x541d
#define TIOCGSERIAL     0x541e
#define TIOCSSERIAL     0x541f
#define TIOCPKT         0x5420
#define FIONBIO         0x5421
#define TIOCNOTTY       0x5422
#define TIOCSETD        0x5423
#define TIOCGETD        0x5424
#define TCSBRKP         0x5425
#define TIOCSBRK        0x5427
#define TIOCCBRK        0x5428
#define TIOCGSID        0x5429
#define TIOCGRS485      0x542e
#define TIOCSRS485      0x542f
#define TCGETX          0x5432
#define TCSETX          0x5433
#define TCSETXF         0x5434
#define TCSETXW         0x5435
#define TIOCVHANGUP     0x5437
#define FIONCLEX        0x5450
#define FIOCLEX         0x5451
#define FIOASYNC        0x5452
#define TIOCSERCONFIG   0x5453
#define TIOCSERGWILD    0x5454
#define TIOCSERSWILD    0x5455
#define TIOCGLCKTRMIOS  0x5456
#define TIOCSLCKTRMIOS  0x5457
#define TIOCSERGSTRUCT  0x5458
#define TIOCSERGETLSR   0x5459
#define TIOCSERGETMULTI 0x545a
#define TIOCSERSETMULTI 0x545b
#define TIOCMIWAIT      0x545c
#define TIOCGICOUNT     0x545d

#endif
