/*************************************************************************
 * acpi.c -- This file is part of OS/0.                                  *
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

#include <sys/acpi.h>
#include <sys/io.h>
#include <sys/memory.h>
#include <sys/timer.h>
#include <stddef.h>
#include <stdlib.h>

#define acpi_reloc(x) (*((__typeof__ (x) *) ((char *) &(x) + RELOC_VADDR)))

static __low_data unsigned int acpi_smi_cmd;
static __low_data unsigned char acpi_cmd_enable;
static __low_data unsigned char acpi_cmd_disable;
static __low_data unsigned int acpi_pm1a_control;
static __low_data unsigned int acpi_pm1b_control;
static __low_data uint16_t acpi_slp_typa;
static __low_data uint16_t acpi_slp_typb;
static __low_data uint16_t acpi_slp_en;
static __low_data uint16_t acpi_sci_en;
static __low_data unsigned char acpi_pm1_control_len;

static __low_text int
acpi_signature_match (const char *a, const char *b, size_t len)
{
  size_t i;
  for (i = 0; i < len; i++)
    {
      if (a[i] != b[i])
	return 0;
    }
  return 1;
}

static __low_text int
acpi_valid_header (ACPISDTHeader *header, const char *value)
{
  if (acpi_signature_match (header->h_signature, value, 4))
    {
      unsigned char *c = (unsigned char *) header;
      unsigned char checksum = 0;
      uint32_t len = header->h_len;
      while (len-- > 0)
	checksum += *c++;
      if (checksum == 0)
	return 1;
    }
  return 0;
}

static __low_text ACPIRSDP *
acpi_check_rsdp (uint32_t addr)
{
  ACPIRSDP *rsdp = (ACPIRSDP *) addr;
  if (acpi_signature_match (rsdp->r_signature, LOW_STRING ("RSD PTR "), 8))
    {
      unsigned char *c = (unsigned char *) rsdp;
      unsigned char checksum = 0;
      size_t i;
      for (i = 0; i < sizeof (ACPIRSDP); i++)
	checksum += *c++;
      if (checksum == 0)
	return rsdp;
    }
  return NULL;
}

static __low_text ACPISDTHeader *
acpi_get_rsdt (void)
{
  uint32_t addr;
  uint32_t ebda;
  ACPIRSDP *rsdp;

  /* Search low memory for RSDP signature */
  for (addr = 0x000e0000; addr < 0x00100000; addr += 16)
    {
      rsdp = acpi_check_rsdp (addr);
      if (rsdp != NULL)
	return (ACPISDTHeader *) rsdp->r_addr;
    }

  /* Search EBDA for RSDP signature */
  ebda = (*((uint16_t *) 0x0000040e) << 4) & 0x000fffff;
  for (addr = ebda; addr < ebda + 1024; addr += 16)
    {
      rsdp = acpi_check_rsdp (addr);
      if (rsdp != NULL)
	return (ACPISDTHeader *) rsdp->r_addr;
    }
  return NULL;
}

__low_text void
acpi_init (void)
{
  ACPISDTHeader *rsdt = acpi_get_rsdt ();
  uint32_t *tables;
  ACPIFADT *fadt;
  unsigned int dsdt_len;
  char *s5_addr;
  unsigned int entries;
  unsigned int i;
  if (rsdt == NULL)
    low_abort (LOW_STRING ("acpi: could not locate RSDT"));
  if (!acpi_valid_header (rsdt, LOW_STRING ("RSDT")))
    low_abort (LOW_STRING ("acpi: invalid RSDT header"));

  tables = (uint32_t *) ((char *) rsdt + sizeof (ACPISDTHeader));
  entries = (rsdt->h_len - sizeof (ACPISDTHeader)) / 4;
  for (i = 0; i < entries; i++)
    {
      if (acpi_valid_header ((ACPISDTHeader *) tables[i], LOW_STRING ("FACP")))
	break;
    }
  if (i == entries)
    low_abort (LOW_STRING ("acpi: could not locate FADT"));
  fadt = (ACPIFADT *) tables[i];

  if (!acpi_valid_header ((ACPISDTHeader *) fadt->f_dsdt, LOW_STRING ("DSDT")))
    low_abort (LOW_STRING ("acpi: invalid DSDT header"));
  s5_addr = (char *) fadt->f_dsdt + sizeof (ACPISDTHeader);
  dsdt_len = *((uint32_t *) (fadt->f_dsdt + 4)) - sizeof (ACPISDTHeader);
  for (i = 0; i < dsdt_len; i++)
    {
      if (acpi_signature_match (s5_addr, LOW_STRING ("_S5_"), 4))
	break;
      s5_addr++;
    }
  if (i == dsdt_len)
    low_abort (LOW_STRING ("acpi: could not find \\_S5 object"));

  if ((s5_addr[-1] == 0x08 || (s5_addr[-2] == 0x08 && s5_addr[-1] == '\\'))
      && s5_addr[4] == 0x12)
    {
      s5_addr += 5;
      s5_addr += ((*s5_addr & 0xc0) >> 6) + 2;

      if (*s5_addr == 0x0a)
	s5_addr++;
      acpi_slp_typa = *s5_addr << 10;
      s5_addr++;

      if (*s5_addr == 0x0a)
	s5_addr++;
      acpi_slp_typb = *s5_addr << 10;

      acpi_smi_cmd = fadt->f_smi_cmd;
      acpi_cmd_enable = fadt->f_acpi_enable;
      acpi_cmd_disable = fadt->f_acpi_disable;
      acpi_pm1a_control = fadt->f_pm1a_control_block;
      acpi_pm1b_control = fadt->f_pm1b_control_block;
      acpi_pm1_control_len = fadt->f_pm1_control_len;
      acpi_slp_en = 1 << 13;
      acpi_sci_en = 1;
    }
  else
    low_abort (LOW_STRING ("acpi: failed to parse \\_S5 object"));
}

int
acpi_enable (void)
{
  if (inw (acpi_reloc (acpi_pm1a_control)) & acpi_reloc (acpi_sci_en))
    return 1; /* ACPI already enabled */
  if (acpi_reloc (acpi_smi_cmd) != 0 && acpi_reloc (acpi_cmd_enable) != 0)
    {
      int i;
      outb (acpi_reloc (acpi_cmd_enable), acpi_reloc (acpi_smi_cmd));
      for (i = 0; i < 300; i++)
	{
	  if ((inw (acpi_reloc (acpi_pm1a_control)) &
	       acpi_reloc (acpi_sci_en)) == 1)
	    break;
	  msleep (10);
	}
      if (acpi_reloc (acpi_pm1b_control) != 0)
	{
	  for (; i < 300; i++)
	    {
	      if ((inw (acpi_reloc (acpi_pm1b_control)) &
		   acpi_reloc (acpi_sci_en)) == 1)
		break;
	      msleep (10);
	    }
	}
      if (i < 300)
	return 1;
    }
  return 0;
}
