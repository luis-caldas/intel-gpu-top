#ifndef INTEL_GPU_TOOLS_H
#define INTEL_GPU_TOOLS_H

#include <stdint.h>
#include <sys/types.h>
#include <pciaccess.h>

#include "intel_chipset.h"
#include "intel_reg.h"

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

extern void *mmio;
void intel_get_mmio(struct pci_device *pci_dev);

/* New style register access API */
int intel_register_access_init(struct pci_device *pci_dev, int safe);
void intel_register_access_fini(void);
uint32_t intel_register_read(uint32_t reg);
void intel_register_write(uint32_t reg, uint32_t val);
/* Following functions are relevant only for SoCs like Valleyview */
uint32_t intel_dpio_reg_read(uint32_t reg);
void intel_dpio_reg_write(uint32_t reg, uint32_t val);

#define INTEL_RANGE_RSVD	(0<<0) /*  Shouldn't be read or written */
#define INTEL_RANGE_READ	(1<<0)
#define INTEL_RANGE_WRITE	(1<<1)
#define INTEL_RANGE_RW		(INTEL_RANGE_READ | INTEL_RANGE_WRITE)
#define INTEL_RANGE_END		(1<<31)

struct intel_register_range {
	uint32_t base;
	uint32_t size;
	uint32_t flags;
};

struct intel_register_map {
	struct intel_register_range *map;
	uint32_t top;
	uint32_t alignment_mask;
};
struct intel_register_map intel_get_register_map(uint32_t devid);
struct intel_register_range *intel_get_register_range(struct intel_register_map map, uint32_t offset, int mode);


static inline uint32_t
INREG(uint32_t reg)
{
	return *(volatile uint32_t *)((volatile char *)mmio + reg);
}

static inline void
OUTREG(uint32_t reg, uint32_t val)
{
	*(volatile uint32_t *)((volatile char *)mmio + reg) = val;
}

struct pci_device *intel_get_pci_device(void);

uint32_t intel_get_drm_devid(int fd);
int intel_gen(uint32_t devid);
uint64_t intel_get_total_ram_mb(void);
uint64_t intel_get_total_swap_mb(void);

void intel_map_file(char *);

enum pch_type {
	PCH_NONE,
	PCH_IBX,
	PCH_CPT,
	PCH_LPT,
};

extern enum pch_type pch;
void intel_check_pch(void);

#define HAS_IBX (pch == PCH_IBX)
#define HAS_CPT (pch == PCH_CPT)
#define HAS_LPT (pch == PCH_LPT)

#endif /* INTEL_GPU_TOOLS_H */
