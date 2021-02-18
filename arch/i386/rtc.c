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
#include <stddef.h>
#include <time.h>

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

static void
rtc_wait_update (void)
{
  unsigned char x;
  do
    {
      outb (CMOS_RTC_ASTAT, CMOS_PORT_REGISTER);
      x = inb (CMOS_PORT_DATA);
    }
  while (x & 0x80);
}

static unsigned char
rtc_read (unsigned char reg)
{
  outb_p (reg, CMOS_PORT_REGISTER);
  return inb (CMOS_PORT_DATA);
}

void
rtc_init (void)
{
  unsigned char last_second;
  unsigned char last_minute;
  unsigned char last_hour;
  unsigned char last_dom;
  unsigned char last_month;
  unsigned char last_year;
  unsigned char second;
  unsigned char minute;
  unsigned char hour;
  unsigned char dom;
  unsigned char month;
  unsigned char year;
  unsigned char bstat;
  unsigned char doy;
  unsigned short posix_year; /* Year since 1900 */
  int i;

  rtc_wait_update ();
  second = rtc_read (CMOS_RTC_SECONDS);
  minute = rtc_read (CMOS_RTC_MINUTES);
  hour = rtc_read (CMOS_RTC_HOURS);
  dom = rtc_read (CMOS_RTC_DOM);
  month = rtc_read (CMOS_RTC_MONTH);
  year = rtc_read (CMOS_RTC_YEAR);

  /* Keep re-reading until the same values are read twice */
  do
    {
      last_second = second;
      last_minute = minute;
      last_hour = hour;
      last_dom = dom;
      last_month = month;
      last_year = year;

      rtc_wait_update ();
      second = rtc_read (CMOS_RTC_SECONDS);
      minute = rtc_read (CMOS_RTC_MINUTES);
      hour = rtc_read (CMOS_RTC_HOURS);
      dom = rtc_read (CMOS_RTC_DOM);
      month = rtc_read (CMOS_RTC_MONTH);
      year = rtc_read (CMOS_RTC_YEAR);
    }
  while (second != last_second || minute != last_minute || hour != last_hour
	 || dom != last_dom || month != last_month || year != last_year);

  /* Normalize values */
  bstat = rtc_read (CMOS_RTC_BSTAT);
  if ((bstat & 0x04) == 0)
    {
      /* Convert BCD to binary */
      second = (second & 0x0f) + (second >> 4) * 10;
      minute = (minute & 0x0f) + (minute >> 4) * 10;
      hour = ((hour & 0x0f) + ((hour & 0x70) >> 4) * 10) | (hour & 0x80);
      dom = (dom & 0x0f) + (dom >> 4) * 10;
      month = (month & 0x0f) + (month >> 4) * 10;
      year = (year & 0x0f) + (year >> 4) * 10;
    }
  if ((bstat & 0x02) == 0 && hour & 0x80)
    hour = ((hour & 0x7f) + 12) % 24; /* Convert 12 to 24-hour time */

  posix_year = year + 100; /* Assume 20xx year */
  doy = dom - 1;
  for (i = 0; i < month - 1; i++)
    doy += dpm[i];

  /* POSIX epoch time formula */
  rtc_time = second + minute * 60 + hour * 3600 + doy * 86400 +
    (posix_year - 70) * 31536000 + ((posix_year - 69) / 4) * 86000 -
    ((posix_year - 1) / 100) * 86400 + ((posix_year + 299) / 400) * 86400;
}

time_t
time (time_t *t)
{
  /* TODO Update rtc_time using PIT */
  if (t != NULL)
    *t = rtc_time;
  return rtc_time;
}
