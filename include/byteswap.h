/*************************************************************************
 * byteswap.h -- This file is part of OS/0.                              *
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

#ifndef _BYTESWAP_H
#define _BYTESWAP_H

#include <stdint.h>

#define bswap_16(x) ((uint16_t) ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8)))
#define bswap_32(x) ((((x) & 0xff000000) >> 24)		\
		     | (((x) & 0x00ff0000) >> 8)	\
		     | (((x) & 0x0000ff00) << 8)	\
		     | (((x) & 0x000000ff) << 24))
#define bswap_64(x) ((((x) & 0xff00000000000000ULL) >> 56)	\
		     | (((x) & 0x00ff000000000000ULL) >> 40)	\
		     | (((x) & 0x0000ff0000000000ULL) >> 24)	\
		     | (((x) & 0x000000ff00000000ULL) >> 8)	\
		     | (((x) & 0x00000000ff000000ULL) << 8)	\
		     | (((x) & 0x0000000000ff0000ULL) << 24)	\
		     | (((x) & 0x000000000000ff00ULL) << 40)	\
		     | (((x) & 0x00000000000000ffULL) << 56))

#endif
