/*************************************************************************
 * ata.c -- This file is part of OS/0.                                   *
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

#include <libk/libk.h>
#include <sys/ata.h>
#include <sys/io.h>
#include <sys/pci.h>
#include <sys/timer.h>
#include <vm/paging.h>

static unsigned char ata_buffer[2048];

static const char *ide_channel_names[] = {
  "primary",
  "secondary"
};

static const char *ide_drive_names[] = {
  "master",
  "slave"
};

static const char *ide_type_names[] = {
  "ATA",
  "ATAPI"
};

static const char ata_flush_cmds[] = {
  ATA_CMD_CACHE_FLUSH,
  ATA_CMD_CACHE_FLUSH,
  ATA_CMD_CACHE_FLUSH_EXT
};

IDEChannelRegisters ata_channels[2];
IDEDevice ata_devices[4];
int ata_irq;
uint32_t ata_pci_device;

void
ata_init (void)
{
  int i;
  int j;
  int k;
  int count = 0;
  uint32_t bar4;
  uint16_t pci_cmd;

  /* Search for ATA PCI device */
  ata_pci_device = pci_find_device (ATA_VENDOR_ID, ATA_DEVICE_ID);
  if (unlikely (ata_pci_device == 0))
    {
      printk ("ata: failed to locate PCI device\n");
      return;
    }
  bar4 = pci_inl (ata_pci_device, PCI_BAR4);
  if (!(bar4 & 1))
    {
      printk ("ata: memory mapped PCI BAR not supported\n");
      return;
    }

  /* Enable PCI bus mastering if necessary */
  pci_cmd = pci_inw (ata_pci_device, PCI_COMMAND);
  if (!(pci_cmd & 4))
    pci_outw (ata_pci_device, PCI_COMMAND, pci_cmd | 4);

  ata_channels[ATA_PRIMARY].icr_base = PATA_BAR0;
  ata_channels[ATA_PRIMARY].icr_ctrl = PATA_BAR1;
  ata_channels[ATA_SECONDARY].icr_base = PATA_BAR2;
  ata_channels[ATA_SECONDARY].icr_ctrl = PATA_BAR3;
  ata_channels[ATA_PRIMARY].icr_bmide = bar4 & 0xfffffffc;
  ata_channels[ATA_SECONDARY].icr_bmide = (bar4 & 0xfffffffc) + 8;

  ata_write (ATA_PRIMARY, ATA_REG_CONTROL, 2);
  ata_write (ATA_SECONDARY, ATA_REG_CONTROL, 2);

  /* Detect and assign IDE drives */
  for (i = 0; i < 2; i++)
    {
      for (j = 0; j < 2; j++)
	{
	  unsigned char err = 0;
	  unsigned char type = IDE_ATA;
	  unsigned char status;
	  uint32_t prdt_paddr;
	  uint32_t prdt_vaddr;
	  ata_devices[count].id_reserved = 0;

	  ata_write (i, ATA_REG_HDDEVSEL, 0xa0 | j << 4);
	  msleep (1);

	  ata_write (i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
	  msleep (1);

	  if (ata_read (i, ATA_REG_STATUS) == 0)
	    continue;
	  while (1)
	    {
	      status = ata_read (i, ATA_REG_STATUS);
	      if (status & ATA_SR_ERR)
		{
		  err = 1;
		  break;
		}
	      if ((status & ATA_SR_BSY) == 0 && status & ATA_SR_DRQ)
		break;
	    }

	  if (err != 0)
	    {
	      unsigned char cl = ata_read (i, ATA_REG_LBA1);
	      unsigned char ch = ata_read (i, ATA_REG_LBA2);

	      if ((cl == 0x14 && ch == 0xeb) || (cl == 0x69 && ch == 0x96))
		type = IDE_ATAPI;
	      else
		continue;

	      ata_write (i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
	      msleep (1);
	    }

	  /* Fill device properties */
	  ata_readbuf (i, ATA_REG_DATA, ata_buffer, 128);
	  ata_devices[count].id_reserved = 1;
	  ata_devices[count].id_type = type;
	  ata_devices[count].id_channel = i;
	  ata_devices[count].id_drive = j;
	  ata_devices[count].id_sig =
	    *((uint16_t *) (ata_buffer + ATA_IDENT_DEVICETYPE));
	  ata_devices[count].id_cap =
	    *((uint16_t *) (ata_buffer + ATA_IDENT_CAPABILITIES));
	  ata_devices[count].id_cmdset =
	    *((uint32_t *) (ata_buffer + ATA_IDENT_COMMANDSETS));

	  if (ata_devices[count].id_cmdset & 1 << 26)
	    ata_devices[count].id_size =
	      *((uint32_t *) (ata_buffer + ATA_IDENT_MAX_LBA_EXT));
	  else
	    ata_devices[count].id_size =
	      *((uint32_t *) (ata_buffer + ATA_IDENT_MAX_LBA));

	  for (k = 0; k < 40; k += 2)
	    {
	      ata_devices[count].id_model[k] =
		ata_buffer[ATA_IDENT_MODEL + k + 1];
	      ata_devices[count].id_model[k + 1] =
		ata_buffer[ATA_IDENT_MODEL + k];
	    }
	  ata_devices[count].id_model[40] = '\0';

	  prdt_paddr = alloc_page ();
	  assert (prdt_paddr != 0);
	  prdt_vaddr = ATA_PRDT_VADDR + count * PAGE_SIZE;
	  map_page (curr_page_dir, prdt_paddr, prdt_vaddr, PAGE_FLAG_WRITE);
	  vm_page_inval_386 (prdt_vaddr);
	  memset ((void *) prdt_vaddr, 0, PAGE_SIZE);
	  ata_devices[count].id_prdt = (ATAPRDT *) prdt_vaddr;

	  count++;
	}
    }

  for (i = 0; i < 4; i++)
    {
      if (ata_devices[i].id_reserved)
        printk ("Found %s %s %s drive (size %lu sectors)\n",
		ide_channel_names[ata_devices[i].id_channel],
		ide_drive_names[ata_devices[i].id_drive],
		ide_type_names[ata_devices[i].id_type], ata_devices[i].id_size);
    }
}

unsigned char
ata_read (unsigned char channel, unsigned char reg)
{
  unsigned char result = 0;
  if (reg > 0x07 && reg < 0x0c)
    ata_write (channel, ATA_REG_CONTROL,
	       0x80 | ata_channels[channel].icr_noint);
  if (reg < 0x08)
    result = inb (ata_channels[channel].icr_base + reg);
  else if (reg < 0x0c)
    result = inb (ata_channels[channel].icr_base + reg - 0x06);
  else if (reg < 0x0e)
    result = inb (ata_channels[channel].icr_ctrl + reg - 0x0a);
  else if (reg < 0x16)
    result = inb (ata_channels[channel].icr_bmide + reg - 0x0e);
  if (reg > 0x07 && reg < 0x0c)
    ata_write (channel, ATA_REG_CONTROL, ata_channels[channel].icr_noint);
  return result;
}

void
ata_write (unsigned char channel, unsigned char reg, unsigned char data)
{
  if (reg > 0x07 && reg < 0x0c)
    ata_write (channel, ATA_REG_CONTROL,
	       0x80 | ata_channels[channel].icr_noint);
  if (reg < 0x08)
    outb (data, ata_channels[channel].icr_base + reg);
  else if (reg < 0x0c)
    outb (data, ata_channels[channel].icr_base + reg - 0x06);
  else if (reg < 0x0e)
    outb (data, ata_channels[channel].icr_ctrl + reg - 0x0a);
  else if (reg < 0x16)
    outb (data, ata_channels[channel].icr_bmide + reg - 0x0e);
  if (reg > 0x07 && reg < 0x0c)
    ata_write (channel, ATA_REG_CONTROL, ata_channels[channel].icr_noint);
}

void
ata_readbuf (unsigned char channel, unsigned char reg, void *buffer,
	     uint32_t quads)
{
  if (reg > 0x07 && reg < 0x0c)
    ata_write (channel, ATA_REG_CONTROL,
	       0x80 | ata_channels[channel].icr_noint);
  if (reg < 0x08)
    insl (ata_channels[channel].icr_base + reg, buffer, quads);
  else if (reg < 0x0c)
    insl (ata_channels[channel].icr_base + reg - 0x06, buffer, quads);
  else if (reg < 0x0e)
    insl (ata_channels[channel].icr_ctrl + reg - 0x0a, buffer, quads);
  else if (reg < 0x16)
    insl (ata_channels[channel].icr_bmide + reg - 0x0e, buffer, quads);
  if (reg > 0x07 && reg < 0x0c)
    ata_write (channel, ATA_REG_CONTROL, ata_channels[channel].icr_noint);
}

int
ata_poll (unsigned char channel, unsigned char chkerr)
{
  int i;
  for (i = 0; i < 4; i++)
    ata_read (channel, ATA_REG_ALTSTAT);
  while (ata_read (channel, ATA_REG_STATUS) & ATA_SR_BSY)
    ;

  if (chkerr)
    {
      unsigned char state = ata_read (channel, ATA_REG_STATUS);
      if (state & ATA_SR_ERR)
	return 1;
      if (state & ATA_SR_DF)
	return 2;
      if (!(state & ATA_SR_DRQ))
	return 3;
    }
  return 0;
}

int
ata_perror (unsigned char drive, int err)
{
  unsigned char st;
  if (err == 0)
    return 0;

  printk ("Error in IDE %s %s [%s]:\n",
	  ide_channel_names[ata_devices[drive].id_channel],
	  ide_drive_names[ata_devices[drive].id_drive],
	  ata_devices[drive].id_model);
  switch (err)
    {
    case 1:
      printk ("* Device Fault\n");
      err = 19;
      break;
    case 2:
      st = ata_read (ata_devices[drive].id_channel, ATA_REG_ERROR);
      if (st & ATA_ER_AMNF)
	{
	  printk ("* No address mark found\n");
	  err = 7;
	}
      if (st & ATA_ER_T0NF)
	{
	  printk ("* No media or media error\n");
	  err = 3;
	}
      if (st & ATA_ER_ABRT)
	{
	  printk ("* Command aborted\n");
	  err = 20;
	}
      if (st & ATA_ER_MCR)
	{
	  printk ("* No media or media error\n");
	  err = 3;
	}
      if (st & ATA_ER_IDNF)
	{
	  printk ("* ID mark not found\n");
	  err = 21;
	}
      if (st & ATA_ER_MC)
	{
	  printk ("* No media or media error\n");
	  err = 3;
	}
      if (st & ATA_ER_UNC)
	{
	  printk ("* Uncorrectable data error\n");
	  err = 22;
	}
      if (st & ATA_ER_BBK)
	{
	  printk ("* Bad sectors\n");
	  err = 13;
	}
      break;
    case 3:
      printk ("* Reads nothing\n");
      err = 23;
      break;
    case 4:
      printk ("* Write-protected\n");
      err = 8;
      break;
    }
  return err;
}

int
ata_access (unsigned char op, unsigned char drive, uint32_t lba,
	    unsigned char nsects, void *buffer)
{
  unsigned char lba_mode;
  unsigned char lba_io[6];
  uint32_t channel = ata_devices[drive].id_channel;
  uint32_t slavebit = ata_devices[drive].id_drive;
  uint32_t bus = ata_channels[channel].icr_base;
  uint32_t words = ATA_SECTSIZE >> 1;
  uint16_t cyl;
  unsigned char head;
  unsigned char sect;
  int err;
  char cmd;
  uint16_t i;
#ifdef ATA_DMA
  unsigned char dma = 1;

 retry:
  if (dma)
    {
      /* Setup PRDT */
      ATAPRDT *prdt = ata_devices[drive].id_prdt;
      char *ptr = buffer;
      char *end = buffer + nsects * ATA_SECTSIZE;
      uint32_t paddr = get_paddr (curr_page_dir, prdt);
      uint32_t prev = 0;
      if ((uintptr_t) buffer & 0x1ff)
	{
	  prdt->pr_addr = get_paddr (curr_page_dir, buffer);
	  prdt->pr_len = ATA_SECTSIZE - ((uintptr_t) buffer & 0x1ff);
	  prdt->pr_end = 0;
	  prev = prdt->pr_addr + prdt->pr_len;
	  prdt++;
	  ptr = (char *) ((uintptr_t) buffer & 0xfffffe00) + ATA_SECTSIZE;
	}
      for (; ptr < end; ptr += ATA_SECTSIZE)
	{
	  size_t len = ptr + ATA_SECTSIZE > end ?
	    (uint16_t) ((uintptr_t) buffer & 0x1ff) : ATA_SECTSIZE;
	  uint32_t addr = get_paddr (curr_page_dir, ptr);
	  if (prev == addr)
	    prdt[-1].pr_len += len;
	  else
	    {
	      if (prdt - ata_devices[drive].id_prdt >= ATA_PRDT_MAX)
		{
		  /* Too many entries in PRDT, we have to use PIO */
		  dma = 0;
		  goto retry;
		}
	      prdt->pr_addr = addr;
	      prdt->pr_len = len;
	      prdt->pr_end = 0;
	      prdt++;
	    }
	  prev = prdt[-1].pr_addr + prdt[-1].pr_len;
	}
      prdt[-1].pr_end = ATA_PRDT_END;
      ata_write (channel, ATA_REG_BM_PRDT0, paddr & 0xff);
      ata_write (channel, ATA_REG_BM_PRDT1, (paddr >> 8) & 0xff);
      ata_write (channel, ATA_REG_BM_PRDT2, (paddr >> 16) & 0xff);
      ata_write (channel, ATA_REG_BM_PRDT3, (paddr >> 24) & 0xff);
    }
  else
#endif
    {
      /* Turn off interrupts */
      ata_irq = 0;
      ata_channels[channel].icr_noint = 0x02;
      ata_write (channel, ATA_REG_CONTROL, ata_channels[channel].icr_noint);
    }

  /* Set parameters depending on addressing mode */
  if (lba >= 0x10000000)
    {
      /* Require LBA48 for >128G addresses */
      lba_mode = IDE_LBA48;
      lba_io[0] = lba & 0x000000ff;
      lba_io[1] = (lba & 0x0000ff00) >> 8;
      lba_io[2] = (lba & 0x00ff0000) >> 16;
      lba_io[3] = (lba & 0xff000000) >> 24;
      lba_io[4] = 0;
      lba_io[5] = 0;
      head = 0;
    }
  else if (ata_devices[drive].id_cap & 0x200)
    {
      /* Use LBA28 if supported */
      lba_mode = IDE_LBA28;
      lba_io[0] = lba & 0x00000ff;
      lba_io[1] = (lba & 0x000ff00) >> 8;
      lba_io[2] = (lba & 0x0ff0000) >> 16;
      lba_io[3] = 0;
      lba_io[4] = 0;
      lba_io[5] = 0;
      head = (lba & 0xf000000) >> 24;
    }
  else
    {
      /* Use CHS */
      lba_mode = IDE_CHS;
      sect = lba % 63 + 1;
      cyl = (lba - sect + 1) / 1008;
      lba_io[0] = sect;
      lba_io[1] = cyl & 0xff;
      lba_io[2] = (cyl >> 8) & 0xff;
      lba_io[3] = 0;
      lba_io[4] = 0;
      lba_io[5] = 0;
      head = (lba - sect + 1) % 1008 / 63;
    }

  while (ata_read (channel, ATA_REG_STATUS) & ATA_SR_BSY)
    ;

#ifdef ATA_DMA
  /* Clear bus master error and interrupt for DMA access */
  if (dma)
    ata_write (channel, ATA_REG_BM_STATUS,
	       ata_read (channel, ATA_REG_BM_STATUS) &
	       ~(ATA_BM_SR_ERR | ATA_BM_SR_INT));
#endif

  /* Select drive */
  if (lba_mode == IDE_CHS)
    ata_write (channel, ATA_REG_HDDEVSEL, 0xa0 | slavebit << 4 | head);
  else
    ata_write (channel, ATA_REG_HDDEVSEL, 0xe0 | slavebit << 4 | head);

  /* Write LBA and sector count */
  if (lba_mode == IDE_LBA48)
    {
      ata_write (channel, ATA_REG_SECCNT1, 0);
      ata_write (channel, ATA_REG_LBA3, lba_io[3]);
      ata_write (channel, ATA_REG_LBA4, lba_io[4]);
      ata_write (channel, ATA_REG_LBA5, lba_io[5]);
    }
  ata_write (channel, ATA_REG_SECCNT0, nsects);
  ata_write (channel, ATA_REG_LBA0, lba_io[0]);
  ata_write (channel, ATA_REG_LBA1, lba_io[1]);
  ata_write (channel, ATA_REG_LBA2, lba_io[2]);

  /* Set the command */
  if (lba_mode == IDE_LBA48)
    {
#ifdef ATA_DMA
      if (dma)
	{
	  if (op == ATA_WRITE)
	    cmd = ATA_CMD_WRITE_DMA_EXT;
	  else
	    cmd = ATA_CMD_READ_DMA_EXT;
	}
      else
#endif
	{
	  if (op == ATA_WRITE)
	    cmd = ATA_CMD_WRITE_PIO_EXT;
	  else
	    cmd = ATA_CMD_READ_PIO_EXT;
	}
    }
  else
    {
#ifdef ATA_DMA
      if (dma)
	{
	  if (op == ATA_WRITE)
	    cmd = ATA_CMD_WRITE_DMA;
	  else
	    cmd = ATA_CMD_READ_DMA;
	}
      else
#endif
	{
	  if (op == ATA_WRITE)
	    cmd = ATA_CMD_WRITE_PIO;
	  else
	    cmd = ATA_CMD_READ_PIO;
	}
    }
  ata_write (channel, ATA_REG_COMMAND, cmd);

  /* Run the command */
#ifdef ATA_DMA
  if (dma)
    {
      int flags = ATA_BM_CMD_START;
      if (op == ATA_READ)
	flags |= ATA_BM_CMD_READ;
      while (!(ata_read (channel, ATA_REG_STATUS) & ATA_SR_DRQ))
	;
      ata_write (channel, ATA_REG_BM_COMMAND, flags);
      ata_await ();
      ata_write (channel, ATA_REG_BM_COMMAND, 0);
    }
  else
#endif
    {
      if (op == ATA_WRITE)
	{
	  for (i = 0; i < nsects; i++)
	    {
	      ata_poll (channel, 0);
	      outsw (bus, buffer, words);
	      buffer += words * 2;
	    }

	  ata_write (channel, ATA_REG_COMMAND, ata_flush_cmds[lba_mode]);
	  ata_poll (channel, 0);
	}
      else
	{
	  for (i = 0; i < nsects; i++)
	    {
	      err = ata_poll (channel, 1);
	      if (err != 0)
		return err;
	      insw (bus, buffer, words);
	      buffer += words * 2;
	    }
	}
    }
  return 0;
}

void
ata_await (void)
{
  while (ata_irq == 0)
    ;
  ata_irq = 0;
}

void
ata_interrupt (int channel)
{
  if (ata_read (channel, ATA_REG_BM_STATUS) & ATA_BM_SR_INT)
    {
      ata_irq = 1;
      inb (ATA_REG_BM_STATUS);
    }
}
