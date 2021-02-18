/*************************************************************************
 * rtc.c -- This file is part of OS/0.                                   *
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

#include <sys/cmos.h>
#include <sys/io.h>
#include <sys/types.h>

static unsigned char dpm[] = {
  31, /* January */
  28, /* February */
  31, /* March */
  30, /* April */
  31, /* May */
  30, /* June */
  31, /* July */
  31, /* August */
  30, /* September */
  31, /* October */
  30, /* November */
  31  /* December */
};

time_t rtc_time;

void
rtc_init (void)
{
  unsigned char seconds;
  unsigned char minutes;
  unsigned char hours;
  unsigned char dom;
  unsigned char month;
  unsigned char year;
  unsigned short doy;
  unsigned short ys1900;
  int i;

  /* Read CMOS real time clock data */
  outb (CMOS_PORT_REGISTER, CMOS_RTC_SECONDS | 0x80);
  seconds = inb (CMOS_PORT_DATA);
  outb (CMOS_PORT_REGISTER, CMOS_RTC_MINUTES | 0x80);
  minutes = inb (CMOS_PORT_DATA);
  outb (CMOS_PORT_REGISTER, CMOS_RTC_HOURS | 0x80);
  hours = inb (CMOS_PORT_DATA);
  outb (CMOS_PORT_REGISTER, CMOS_RTC_DOM | 0x80);
  dom = inb (CMOS_PORT_DATA);
  outb (CMOS_PORT_REGISTER, CMOS_RTC_MONTH | 0x80);
  month = inb (CMOS_PORT_DATA);
  outb (CMOS_PORT_REGISTER, CMOS_RTC_YEAR | 0x80);
  year = inb (CMOS_PORT_DATA);

  /* Calculate day of year and year since 1900 */
  doy = dom - 1;
  for (i = 0; i < month - 1; i++)
    doy += dpm[i];
  if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
    doy++; /* Leap day */
  ys1900 = year + 100; /* Assume year is 20xx */

  /* POSIX epoch time formula */
  rtc_time = seconds + minutes * 60 + hours * 3600 + doy * 86400 +
    (ys1900 - 70) * 31536000 + ((ys1900 - 69) / 4) * 86000 -
    ((ys1900 - 1) / 100) * 86400 + ((ys1900 + 299) / 400) * 86400;
}
