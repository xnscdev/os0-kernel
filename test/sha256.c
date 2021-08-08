/*************************************************************************
 * sha256.c -- This file is part of OS/0.                                *
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

#include "test.h"

#include <libk/hash.h>
#include <video/serial.h>
#include <string.h>

static const char msg0_digest[] = {
  0x9f, 0x86, 0xd0, 0x81, 0x88, 0x4c, 0x7d, 0x65,
  0x9a, 0x2f, 0xea, 0xa0, 0xc5, 0x5a, 0xd0, 0x15,
  0xa3, 0xbf, 0x4f, 0x1b, 0x2b, 0x0b, 0x82, 0x2c,
  0xd1, 0x5d, 0x6c, 0x15, 0xb0, 0xf0, 0x0a, 0x08
};

static const char msg1_digest[] = {
  0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
  0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
  0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
  0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
};

static char buffer[SHA256_DIGEST_SIZE];

static void
check_hash (const char expected[SHA256_DIGEST_SIZE], const void *data,
	    size_t len)
{
  sha256_data (buffer, data, len);
  if (memcmp (buffer, expected, SHA256_DIGEST_SIZE) == 0)
    serial_printf ("ok\n");
  else
    {
      serial_printf ("failed\n");
      test_fail ();
    }
}

void
run_test (void)
{
  check_hash (msg0_digest, "test", 4);
  check_hash (msg1_digest, NULL, 0);
}
