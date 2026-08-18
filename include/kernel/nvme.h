#pragma once
#include <stdint.h>

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_NVM 0x08
#define PCI_PROG_IF_NVME 0x02

void nvme_init(void);