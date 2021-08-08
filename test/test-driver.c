/*************************************************************************
 * test-driver.c -- This file is part of OS/0.                           *
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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char buffer[BUFSIZ];

int
main (int argc, char **argv)
{
  FILE *stream;
  if (argc != 3)
    {
      fprintf (stderr, "usage: test-driver <QEMU> <FILE>\n");
      exit (1);
    }
  sprintf (buffer, "%s -kernel %s -serial stdio -monitor null -nographic",
	   argv[1], argv[2]);

  stream = popen (buffer, "r");
  while (fgets (buffer, BUFSIZ, stream) != NULL)
    ;
  fclose (stream);
  if (strcmp (buffer, "PASS\n") != 0)
    exit (1);
  return 0;
}
