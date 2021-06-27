/*************************************************************************
 * endian.h -- This file is part of OS/0.                                *
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

#ifndef _ENDIAN_H
#define _ENDIAN_H

#include <byteswap.h>

#define htobe16(x) bswap_16 (x)
#define htole16(x) (x)
#define be16toh(x) bswap_16 (x)
#define le16toh(x) (x)

#define htobe32(x) bswap_32 (x)
#define htole32(x) (x)
#define be32toh(x) bswap_32 (x)
#define le32toh(x) (x)

#define htobe64(x) bswap_64 (x)
#define htole64(x) (x)
#define be64toh(x) bswap_64 (x)
#define le64toh(x) (x)

#endif
