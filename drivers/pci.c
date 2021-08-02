/*************************************************************************
 * pci.c -- This file is part of OS/0.                                   *
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

#include <sys/ata.h>
#include <sys/io.h>
#include <sys/pci.h>

unsigned char
pci_inb (uint32_t addr, unsigned char offset)
{
  outl (addr | (offset & 0xfc) | 0x80000000, PCI_PORT_ADDRESS);
  return (inl (PCI_PORT_DATA) >> ((offset & 3) * 8)) & 0xff;
}

uint16_t
pci_inw (uint32_t addr, unsigned char offset)
{
  outl (addr | (offset & 0xfc) | 0x80000000, PCI_PORT_ADDRESS);
  return (inl (PCI_PORT_DATA) >> ((offset & 2) * 8)) & 0xffff;
}

uint32_t
pci_inl (uint32_t addr, unsigned char offset)
{
  outl (addr | (offset & 0xfc) | 0x80000000, PCI_PORT_ADDRESS);
  return inl (PCI_PORT_DATA);
}

void
pci_outb (uint32_t addr, unsigned char offset, unsigned char value)
{
  uint32_t temp;
  outl (addr | (offset & 0xfc) | 0x80000000, PCI_PORT_ADDRESS);
  temp = inl (PCI_PORT_DATA) & ~(0xff << ((offset & 3) * 8));
  temp |= (uint32_t) value << ((offset & 3) * 8);
  outl (temp, PCI_PORT_DATA);
}

void
pci_outw (uint32_t addr, unsigned char offset, uint16_t value)
{
  uint32_t temp;
  outl (addr | (offset & 0xfc) | 0x80000000, PCI_PORT_ADDRESS);
  temp = inl (PCI_PORT_DATA) & ~(0xffff << ((offset & 2) * 8));
  temp |= (uint32_t) value << ((offset & 2) * 8);
  outl (temp, PCI_PORT_DATA);
}

void
pci_outl (uint32_t addr, unsigned char offset, uint32_t value)
{
  outl (addr | (offset & 0xfc) | 0x80000000, PCI_PORT_ADDRESS);
  outl (value, PCI_PORT_DATA);
}

uint16_t
pci_device_type (uint32_t addr)
{
  return (pci_inb (addr, PCI_CLASS) << 8) | pci_inb (addr, PCI_SUBCLASS);
}

uint32_t
pci_check_function (uint16_t vendor_id, uint16_t device_id, uint32_t addr)
{
  if (pci_device_type (addr) == PCI_DEVICE_BRIDGE)
    pci_enumerate_bus (vendor_id, device_id, pci_inb (addr, PCI_SECONDARY_BUS));
  if (vendor_id == pci_inw (addr, PCI_VENDOR_ID)
      && device_id == pci_inw (addr, PCI_DEVICE_ID))
    return addr;
  return 0;
}

uint32_t
pci_check_device (uint16_t vendor_id, uint16_t device_id, unsigned char bus,
		  unsigned char device)
{
  uint32_t dev;
  uint32_t addr = pci_make_addr (bus, device, 0);
  if (pci_inw (addr, PCI_VENDOR_ID) == PCI_DEVICE_NONE)
    return 0; /* Device does not exist */
  dev = pci_check_function (vendor_id, device_id, addr);
  if (dev != 0)
    return dev;
  /* Check multiple device functions if supported */
  if (pci_inb (addr, PCI_HEADER_TYPE) & PCI_TYPE_MULTI)
    {
      unsigned char func;
      for (func = 0; func < 8; func++)
	{
	  addr = pci_make_addr (bus, device, func);
	  if (pci_inw (addr, PCI_VENDOR_ID) != PCI_DEVICE_NONE)
	    {
	      dev = pci_check_function (vendor_id, device_id, addr);
	      if (dev != 0)
		return dev;
	    }
	}
    }
  return 0;
}

uint32_t
pci_enumerate_bus (uint16_t vendor_id, uint16_t device_id, unsigned char bus)
{
  unsigned char device;
  for (device = 0; device < PCI_DEVICES_PER_BUS; device++)
    {
      uint32_t dev = pci_check_device (vendor_id, device_id, bus, device);
      if (dev != 0)
	return dev;
    }
  return 0;
}

uint32_t
pci_find_device (uint16_t vendor_id, uint16_t device_id)
{
  uint32_t addr = pci_enumerate_bus (vendor_id, device_id, 0);
  unsigned char func;
  if (addr != 0)
    return addr;
  for (func = 1; func < PCI_FUNCS_PER_DEVICE; func++)
    {
      addr = pci_make_addr (0, 0, func);
      if (pci_inw (addr, PCI_VENDOR_ID) == PCI_DEVICE_NONE)
	break;
      addr = pci_enumerate_bus (vendor_id, device_id, func);
      if (addr != 0)
	return addr;
    }
  return 0;
}
