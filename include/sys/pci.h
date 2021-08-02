/*************************************************************************
 * pci.h -- This file is part of OS/0.                                   *
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

#ifndef _SYS_PCI_H
#define _SYS_PCI_H

#include <sys/cdefs.h>
#include <stdint.h>

#define PCI_PORT_ADDRESS 0xcf8
#define PCI_PORT_DATA    0xcfc

/* PCI header types */

#define PCI_TYPE_DEVICE  0x00
#define PCI_TYPE_BRIDGE  0x01
#define PCI_TYPE_CARDBUS 0x02
#define PCI_TYPE_MULTI   0x80

#define PCI_DEVICE_BRIDGE 0x0604
#define PCI_DEVICE_NONE   0xffff

/* PCI configuration offsets */

#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_REVISION_ID     0x08
#define PCI_PROG_IF         0x09
#define PCI_SUBCLASS        0x0a
#define PCI_CLASS           0x0b
#define PCI_CACHE_LINE_SIZE 0x0c
#define PCI_LATENCY_TIMER   0x0d
#define PCI_HEADER_TYPE     0x0e
#define PCI_BIST            0x0f
#define PCI_SECONDARY_BUS   0x19
#define PCI_BAR4            0x20

#define PCI_DEVICES_PER_BUS  32
#define PCI_FUNCS_PER_DEVICE 8

__BEGIN_DECLS

static inline uint32_t
pci_make_addr (unsigned char bus, unsigned char device, unsigned char func)
{
  return ((uint32_t) bus << 16) | ((uint32_t) (device & 0x1f) << 11)
    | ((uint32_t) (func & 7) << 8);
}

unsigned char pci_inb (uint32_t addr, unsigned char offset);
uint16_t pci_inw (uint32_t addr, unsigned char offset);
uint32_t pci_inl (uint32_t addr, unsigned char offset);
void pci_outb (uint32_t addr, unsigned char offset, unsigned char value);
void pci_outw (uint32_t addr, unsigned char offset, uint16_t value);
void pci_outl (uint32_t addr, unsigned char offset, uint32_t value);
uint16_t pci_device_type (uint32_t addr);
uint32_t pci_check_function (uint16_t vendor_id, uint16_t device_id,
			     uint32_t addr);
uint32_t pci_check_device (uint16_t vendor_id, uint16_t device_id,
			   unsigned char bus, unsigned char device);
uint32_t pci_enumerate_bus (uint16_t vendor_id, uint16_t device_id,
			    unsigned char bus);
uint32_t pci_find_device (uint16_t vendor_id, uint16_t device_id);

__END_DECLS

#endif
