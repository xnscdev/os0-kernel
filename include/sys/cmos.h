/*************************************************************************
 * cmos.h -- This file is part of OS/0.                                  *
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

#ifndef _SYS_CMOS_H
#define _SYS_CMOS_H

#define CMOS_PORT_REGISTER 0x70
#define CMOS_PORT_DATA     0x71

#define CMOS_RTC_SECONDS 0x00
#define CMOS_RTC_MINUTES 0x02
#define CMOS_RTC_HOURS   0x04
#define CMOS_RTC_WEEKDAY 0x06
#define CMOS_RTC_DOM     0x07
#define CMOS_RTC_MONTH   0x08
#define CMOS_RTC_YEAR    0x09
#define CMOS_RTC_CENTURY 0x32
#define CMOS_RTC_ASTAT   0x0a
#define CMOS_RTC_BSTAT   0x0b
#define CMOS_RTC_CSTAT   0x0c

#endif
