/*************************************************************************
 * acpi.h -- This file is part of OS/0.                                  *
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

#ifndef _SYS_ACPI_H
#define _SYS_ACPI_H

#include <sys/cdefs.h>
#include <stdint.h>

typedef struct
{
  unsigned char r_signature[8];
  unsigned char r_checksum;
  unsigned char r_oem_id[6];
  unsigned char r_revision;
  uint32_t r_addr;
} ACPIRSDP;

typedef struct
{
  unsigned char h_signature[4];
  uint32_t h_len;
  unsigned char h_revision;
  unsigned char h_checksum;
  unsigned char h_oem_id[6];
  unsigned char h_oem_table_id[8];
  uint32_t h_oem_revision;
  uint32_t h_creator_id;
  uint32_t h_creator_revision;
} ACPISDTHeader;

typedef struct
{
  unsigned char a_space;
  unsigned char a_bit_width;
  unsigned char a_bit_offset;
  unsigned char a_size;
  uint64_t a_addr;
} ACPIAddress;

typedef struct
{
  ACPISDTHeader f_header;
  uint32_t f_firmware_control;
  uint32_t f_dsdt;
  unsigned char f_reserved;
  unsigned char f_pwr_mgmt_prof;
  uint16_t f_sci_int;
  uint32_t f_smi_cmd;
  unsigned char f_acpi_enable;
  unsigned char f_acpi_disable;
  unsigned char f_s4bios_req;
  unsigned char f_pstate_control;
  uint32_t f_pm1a_event_block;
  uint32_t f_pm1b_event_block;
  uint32_t f_pm1a_control_block;
  uint32_t f_pm1b_control_block;
  uint32_t f_pm2_control_block;
  uint32_t f_pm_timer_block;
  uint32_t f_gpe0_block;
  uint32_t f_gpe1_block;
  unsigned char f_pm1_event_len;
  unsigned char f_pm1_control_len;
  unsigned char f_pm2_control_len;
  unsigned char f_pm_timer_len;
  unsigned char f_gpe0_len;
  unsigned char f_gpe1_len;
  unsigned char f_gpe1_base;
  unsigned char f_cstate_control;
  uint16_t f_worst_c2_latency;
  uint16_t f_worst_c3_latency;
  uint16_t f_flush_size;
  uint16_t f_flush_stride;
  unsigned char f_duty_offset;
  unsigned char f_duty_width;
  unsigned char f_day_alarm;
  unsigned char f_month_alarm;
  unsigned char f_century;
  uint16_t f_boot_arch_flags;
  unsigned char f_reserved2;
  uint32_t f_flags;
  ACPIAddress f_reset_reg;
  unsigned char f_reset_value;
  unsigned char f_reserved3[3];
  uint64_t f_x_firmware_control;
  uint64_t f_x_dsdt;
  ACPIAddress f_x_pm1a_event_block;
  ACPIAddress f_x_pm1b_event_block;
  ACPIAddress f_x_pm1a_control_block;
  ACPIAddress f_x_pm1b_control_block;
  ACPIAddress f_x_pm2_control_block;
  ACPIAddress f_x_pm_timer_block;
  ACPIAddress f_x_gpe0_block;
  ACPIAddress f_x_gpe1_block;
} ACPIFADT;

__BEGIN_DECLS

int acpi_enable (void);
int acpi_shutdown (void);

__END_DECLS

#endif
