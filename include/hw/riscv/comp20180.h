/*
 * QEMU RISC-V VirtIO machine interface
 *
 * Copyright (c) 2017 SiFive, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_RISCV_COMP20180_H
#define HW_RISCV_COMP20180_H

#include "hw/boards.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/sysbus.h"
#include "hw/block/flash.h"

#define COMP20180_CPUS_MAX_BITS             9
#define COMP20180_CPUS_MAX                  (1 << COMP20180_CPUS_MAX_BITS)
#define COMP20180_SOCKETS_MAX_BITS          2
#define COMP20180_SOCKETS_MAX               (1 << COMP20180_SOCKETS_MAX_BITS)

#define TYPE_RISCV_COMP20180_MACHINE MACHINE_TYPE_NAME("comp20180")
typedef struct RISCVCOMP20180State RISCVCOMP20180State;
DECLARE_INSTANCE_CHECKER(RISCVCOMP20180State, RISCV_COMP20180_MACHINE,
                         TYPE_RISCV_COMP20180_MACHINE)

typedef enum RISCVCOMP20180AIAType {
    COMP20180_AIA_TYPE_NONE = 0,
    COMP20180_AIA_TYPE_APLIC,
    COMP20180_AIA_TYPE_APLIC_IMSIC,
} RISCVCOMP20180AIAType;

struct RISCVCOMP20180State {
    /*< private >*/
    MachineState parent;

    /*< public >*/
    Notifier machine_done;
    DeviceState *platform_bus_dev;
    RISCVHartArrayState soc[COMP20180_SOCKETS_MAX];
    DeviceState *irqchip[COMP20180_SOCKETS_MAX];
    PFlashCFI01 *flash[2];
    FWCfgState *fw_cfg;

    int fdt_size;
    bool have_aclint;
    RISCVCOMP20180AIAType aia_type;
    int aia_guests;
    char *oem_id;
    char *oem_table_id;
    OnOffAuto acpi;
    const MemMapEntry *memmap;
};

enum {
    COMP20180_DEBUG,
    COMP20180_MROM,
    COMP20180_TEST,
    COMP20180_RTC,
    COMP20180_CLINT,
    COMP20180_ACLINT_SSWI,
    COMP20180_PLIC,
    COMP20180_APLIC_M,
    COMP20180_APLIC_S,
    COMP20180_UART0,
    COMP20180_COMP20180IO,
    COMP20180_FW_CFG,
    COMP20180_IMSIC_M,
    COMP20180_IMSIC_S,
    COMP20180_FLASH,
    COMP20180_DRAM,
    COMP20180_PCIE_MMIO,
    COMP20180_PCIE_PIO,
    COMP20180_PLATFORM_BUS,
    COMP20180_PCIE_ECAM
};

enum {
    UART0_IRQ = 10,
    RTC_IRQ = 11,
    COMP20180IO_IRQ = 1, /* 1 to 8 */
    COMP20180IO_COUNT = 8,
    PCIE_IRQ = 0x20, /* 32 to 35 */
    COMP20180_PLATFORM_BUS_IRQ = 64, /* 64 to 95 */
};

#define COMP20180_PLATFORM_BUS_NUM_IRQS 32

#define COMP20180_IRQCHIP_NUM_MSIS 255
#define COMP20180_IRQCHIP_NUM_SOURCES 96
#define COMP20180_IRQCHIP_NUM_PRIO_BITS 3
#define COMP20180_IRQCHIP_MAX_GUESTS_BITS 3
#define COMP20180_IRQCHIP_MAX_GUESTS ((1U << COMP20180_IRQCHIP_MAX_GUESTS_BITS) - 1U)

#define COMP20180_PLIC_PRIORITY_BASE 0x00
#define COMP20180_PLIC_PENDING_BASE 0x1000
#define COMP20180_PLIC_ENABLE_BASE 0x2000
#define COMP20180_PLIC_ENABLE_STRIDE 0x80
#define COMP20180_PLIC_CONTEXT_BASE 0x200000
#define COMP20180_PLIC_CONTEXT_STRIDE 0x1000
#define COMP20180_PLIC_SIZE(__num_context) \
    (COMP20180_PLIC_CONTEXT_BASE + (__num_context) * COMP20180_PLIC_CONTEXT_STRIDE)

#define FDT_PCI_ADDR_CELLS    3
#define FDT_PCI_INT_CELLS     1
#define FDT_PLIC_ADDR_CELLS   0
#define FDT_PLIC_INT_CELLS    1
#define FDT_APLIC_INT_CELLS   2
#define FDT_IMSIC_INT_CELLS   0
#define FDT_MAX_INT_CELLS     2
#define FDT_MAX_INT_MAP_WIDTH (FDT_PCI_ADDR_CELLS + FDT_PCI_INT_CELLS + \
                                 1 + FDT_MAX_INT_CELLS)
#define FDT_PLIC_INT_MAP_WIDTH  (FDT_PCI_ADDR_CELLS + FDT_PCI_INT_CELLS + \
                                 1 + FDT_PLIC_INT_CELLS)
#define FDT_APLIC_INT_MAP_WIDTH (FDT_PCI_ADDR_CELLS + FDT_PCI_INT_CELLS + \
                                 1 + FDT_APLIC_INT_CELLS)

//bool virt_is_acpi_enabled(RISCVCOMP20180State *s);
//void virt_acpi_setup(RISCVCOMP20180State *vms);
#endif
