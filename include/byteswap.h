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

#define bswap_16(x) (((x) & 0xff) << 8) | ((x) >> 8))
#define bswap_32(x) (((uint32_t) bswap_16 ((uint16_t) ((x & 0xffff)) << 16) | \
		      (uint32_t) bswap_16 ((uint16_t) ((x) >> 16)))
#define bswap_64(x) (((uint64_t) bswap_32 ((uint32_t) ((x) & 0xffffffff)) \
		      << 32) | (uint64_t) bswap_32 ((uint32_t) ((x) >> 32)))

#endif
