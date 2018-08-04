#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "intel_gpu_tools.h"

enum pch_type pch;

struct pci_device *
intel_get_pci_device(void)
{
	struct pci_device *pci_dev;
	int error;

	error = pci_system_init();
	if (error != 0) {
		fprintf(stderr, "Couldn't initialize PCI system: %s\n",
			strerror(error));
		exit(1);
	}

	/* Grab the graphics card. Try the canonical slot first, then
	 * walk the entire PCI bus for a matching device. */
	pci_dev = pci_device_find_by_slot(0, 0, 2, 0);
	if (pci_dev == NULL || pci_dev->vendor_id != 0x8086) {
		struct pci_device_iterator *iter;
		struct pci_id_match match;

		match.vendor_id = 0x8086; /* Intel */
		match.device_id = PCI_MATCH_ANY;
		match.subvendor_id = PCI_MATCH_ANY;
		match.subdevice_id = PCI_MATCH_ANY;

		match.device_class = 0x3 << 16;
		match.device_class_mask = 0xff << 16;

		match.match_data = 0;

		iter = pci_id_match_iterator_create(&match);
		pci_dev = pci_device_next(iter);
		pci_iterator_destroy(iter);
	}
	if (pci_dev == NULL)
		errx(1, "Couldn't find graphics card");

	error = pci_device_probe(pci_dev);
	if (error != 0) {
		fprintf(stderr, "Couldn't probe graphics card: %s\n",
			strerror(error));
		exit(1);
	}

	if (pci_dev->vendor_id != 0x8086)
		errx(1, "Graphics card is non-intel");

	return pci_dev;
}

void
intel_check_pch(void)
{
	struct pci_device *pch_dev;

	pch_dev = pci_device_find_by_slot(0, 0, 31, 0);
	if (pch_dev == NULL)
		return;

	if (pch_dev->vendor_id != 0x8086)
		return;

	switch (pch_dev->device_id & 0xff00) {
	case 0x3b00:
		pch = PCH_IBX;
		break;
	case 0x1c00:
	case 0x1e00:
		pch = PCH_CPT;
		break;
	case 0x8c00:
	case 0x9c00:
		pch = PCH_LPT;
		break;
	default:
		pch = PCH_NONE;
		return;
	}
}
