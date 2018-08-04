#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>

#ifdef HAVE_SHM
#include <sys/shm.h>
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include "lib/intel_gpu_tools.h"
#include "lib/instdone.h"
#include "shm.h"

#define  FORCEWAKE	    0xA18C
#define  FORCEWAKE_ACK	    0x130090

#define SAMPLES_PER_SEC             100
#define SAMPLES_TO_PERCENT_RATIO    (SAMPLES_PER_SEC / 100)

#define MAX_NUM_TOP_BITS            100

#define HAS_STATS_REGS(devid)		IS_965(devid)

#define SPACE 1

#ifdef HAVE_SHM
int init_shm(key_t key);
int insert_shm(int value);
#endif

struct top_bit {
	struct instdone_bit *bit;
	int count;
} top_bits[MAX_NUM_TOP_BITS];
struct top_bit *top_bits_sorted[MAX_NUM_TOP_BITS];

enum stats_counts {
	IA_VERTICES,
	IA_PRIMITIVES,
	VS_INVOCATION,
	GS_INVOCATION,
	GS_PRIMITIVES,
	CL_INVOCATION,
	CL_PRIMITIVES,
	PS_INVOCATION,
	PS_DEPTH,
	STATS_COUNT
};

const uint32_t stats_regs[STATS_COUNT] = {
	IA_VERTICES_COUNT_QW,
	IA_PRIMITIVES_COUNT_QW,
	VS_INVOCATION_COUNT_QW,
	GS_INVOCATION_COUNT_QW,
	GS_PRIMITIVES_COUNT_QW,
	CL_INVOCATION_COUNT_QW,
	CL_PRIMITIVES_COUNT_QW,
	PS_INVOCATION_COUNT_QW,
	PS_DEPTH_COUNT_QW,
};

const char *stats_reg_names[STATS_COUNT] = {
	"vert fetch",
	"prim fetch",
	"VS invocations",
	"GS invocations",
	"GS prims",
	"CL invocations",
	"CL prims",
	"PS invocations",
	"PS depth pass",
};

uint64_t stats[STATS_COUNT];
uint64_t last_stats[STATS_COUNT];

#ifdef HAVE_SHM
int *shm;
int init_shm_b = 0;

int init_shm(key_t key){

	int shmid;
	int size;

	size = 3;

    if ((shmid = shmget(key, size, IPC_CREAT | 0666)) < 0) {
		printf("SHMGET error\n");
		return 1;
    }

    if ((shm = shmat(shmid, NULL, 0)) == (int *) -1) {
		printf("SHMMAT error\n");
		return 1;
    }

	init_shm_b = 1;
	return 0;
}

int insert_shm(int value){

	if(!init_shm_b) if(init_shm(SHM_REFERENCE_NUMBER)) return 1;
	*shm = value;
	return 0;
}
#endif

static unsigned long
gettime(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return (t.tv_usec + (t.tv_sec * 1000000));
}

struct ring {
	const char *name;
	uint32_t mmio;
	int head, tail, size;
	uint64_t full;
	int idle;
};

static uint32_t ring_read(struct ring *ring, uint32_t reg)
{
	return INREG(ring->mmio + reg);
}

static void ring_init(struct ring *ring)
{
	ring->size = (((ring_read(ring, RING_LEN) & RING_NR_PAGES) >> 12) + 1) * 4096;
}

static void ring_reset(struct ring *ring)
{
	ring->idle = ring->full = 0;
}

static void ring_sample(struct ring *ring)
{
	int full;

	if (!ring->size)
		return;

	ring->head = ring_read(ring, RING_HEAD) & HEAD_ADDR;
	ring->tail = ring_read(ring, RING_TAIL) & TAIL_ADDR;

	if (ring->tail == ring->head)
		ring->idle++;

	full = ring->tail - ring->head;
	if (full < 0)
		full += ring->size;
	ring->full += full;
}

static void ring_print(struct ring *ring, unsigned long samples_per_sec)
{
	int percent_busy;

	if (!ring->size)
		return;

	percent_busy = 100 - 100 * ring->idle / samples_per_sec;

#ifdef HAVE_SHM
	insert_shm(percent_busy);
#endif

	printf("Percent: %d %%\n", percent_busy);

	// printf("Space: %d/%d\n", (int)(ring->full / samples_per_sec), ring->size);
}

int main()
{
	int i;

	int samples_per_sec = SAMPLES_PER_SEC;

	uint32_t devid;

	struct pci_device *pci_dev;
	struct ring render_ring = {
		.name = "render",
		.mmio = 0x2030,
	}, bsd_ring = {
		.name = "bitstream",
		.mmio = 0x4030,
	}, bsd6_ring = {
		.name = "bitstream",
		.mmio = 0x12030,
	}, blt_ring = {
		.name = "blitter",
		.mmio = 0x22030,
	};

	pci_dev = intel_get_pci_device();
	devid = pci_dev->device_id;
	intel_get_mmio(pci_dev);
	init_instdone_definitions(devid);

	for (i = 0; i < num_instdone_bits; i++) {
		top_bits[i].bit = &instdone_bits[i];
		top_bits[i].count = 0;
		top_bits_sorted[i] = &top_bits[i];
	}

	/* Grab access to the registers */
	intel_register_access_init(pci_dev, 0);

	ring_init(&render_ring);
	if (IS_GEN4(devid) || IS_GEN5(devid))
		ring_init(&bsd_ring);
	if (IS_GEN6(devid) || IS_GEN7(devid)) {
		ring_init(&bsd6_ring);
		ring_init(&blt_ring);
	}

	/* Initialize GPU stats */
	if (HAS_STATS_REGS(devid)) {
		for (i = 0; i < STATS_COUNT; i++) {
			uint32_t stats_high, stats_low, stats_high_2;

			do {
				stats_high = INREG(stats_regs[i] + 4);
				stats_low = INREG(stats_regs[i]);
				stats_high_2 = INREG(stats_regs[i] + 4);
			} while (stats_high != stats_high_2);

			last_stats[i] = (uint64_t)stats_high << 32 |
				stats_low;
		}
	}

	while(1){

		unsigned long long t1, ti, tf;
		unsigned long long def_sleep = 1000000 / samples_per_sec;
		unsigned long long last_samples_per_sec = samples_per_sec;

		t1 = gettime();

		ring_reset(&render_ring);

		for (i = 0; i < samples_per_sec; i++) {
			long long interval;
			ti = gettime();

			ring_sample(&render_ring);

			tf = gettime();
			if (tf - t1 >= 1000000) {
				last_samples_per_sec = i+1;
				break;
			}
			interval = def_sleep - (tf - ti);
			if (interval > 0)
				usleep(interval);
		}

		ring_print(&render_ring, last_samples_per_sec); //important

	}

	intel_register_access_fini();
	return 0;
}
