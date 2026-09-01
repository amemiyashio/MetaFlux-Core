#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/pci.h>

#define MF_PCI_VENDOR_ID 0x4d46
#define MF_PCI_DEVICE_ID 0x0001
#define MF_PCI_CLASS 0x120000

#define MF_PCI_BAR0 0
#define MF_PCI_BAR2 2
#define MF_PCI_BAR4 4
#define MF_PCI_BAR0_SIZE (64U * 1024U)
#define MF_PCI_BAR2_SIZE 4096U
#define MF_PCI_BAR4_SIZE 4096U
#define MF_PCI_MSIX_VECTORS 2

struct mf_pci_irq_slot {
	atomic64_t notifications;
};

struct mf_pci_device {
	void __iomem *bar0;
	void __iomem *bar2;
	int irq_vectors;
	unsigned int irq_requested;
	struct mf_pci_irq_slot irq_slots[MF_PCI_MSIX_VECTORS];
};

static irqreturn_t mf_pci_irq_handler(int irq, void *context)
{
	struct mf_pci_irq_slot *slot = context;

	if (slot == NULL || irq < 0)
		return IRQ_NONE;
	atomic64_inc(&slot->notifications);
	return IRQ_HANDLED;
}

static bool mf_pci_mem_bar_matches(const struct pci_dev *pdev, int bar,
					  resource_size_t expected_size)
{
	return (pci_resource_flags(pdev, bar) & IORESOURCE_MEM) != 0 &&
	       pci_resource_len(pdev, bar) == expected_size;
}

static int mf_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct mf_pci_device *device;
	int result;

	if (pdev->class != MF_PCI_CLASS ||
	    !mf_pci_mem_bar_matches(pdev, MF_PCI_BAR0, MF_PCI_BAR0_SIZE) ||
	    !mf_pci_mem_bar_matches(pdev, MF_PCI_BAR2, MF_PCI_BAR2_SIZE) ||
	    !mf_pci_mem_bar_matches(pdev, MF_PCI_BAR4, MF_PCI_BAR4_SIZE))
		return -ENODEV;

	result = pci_enable_device_mem(pdev);
	if (result != 0)
		return result;
	result = pci_request_regions(pdev, "metaflux_pci");
	if (result != 0)
		goto disable_device;

	device = devm_kzalloc(&pdev->dev, sizeof(*device), GFP_KERNEL);
	if (device == NULL) {
		result = -ENOMEM;
		goto release_regions;
	}
	device->bar0 = pci_iomap(pdev, MF_PCI_BAR0, MF_PCI_BAR0_SIZE);
	if (device->bar0 == NULL) {
		result = -ENOMEM;
		goto release_regions;
	}
	device->bar2 = pci_iomap(pdev, MF_PCI_BAR2, MF_PCI_BAR2_SIZE);
	if (device->bar2 == NULL) {
		result = -ENOMEM;
		goto unmap_bar0;
	}

	/* BAR4 is owned by the PCI MSI-X capability; do not map its table here. */
	device->irq_vectors = pci_alloc_irq_vectors(pdev, MF_PCI_MSIX_VECTORS,
						    MF_PCI_MSIX_VECTORS, PCI_IRQ_MSIX);
	if (device->irq_vectors < 0) {
		result = device->irq_vectors;
		goto unmap_bar2;
	}
	for (device->irq_requested = 0; device->irq_requested < MF_PCI_MSIX_VECTORS;
	     ++device->irq_requested) {
		result = request_irq(pci_irq_vector(pdev, device->irq_requested), mf_pci_irq_handler, 0,
				     "metaflux_pci", &device->irq_slots[device->irq_requested]);
		if (result != 0)
			goto free_irqs;
	}
	pci_set_master(pdev);
	pci_set_drvdata(pdev, device);
	dev_info(&pdev->dev, "static MetaFlux guest function ready (BAR0/BAR2/BAR4, %d MSI-X vectors)\n",
		 device->irq_vectors);
	return 0;

free_irqs:
	while (device->irq_requested > 0) {
		--device->irq_requested;
		free_irq(pci_irq_vector(pdev, device->irq_requested),
			 &device->irq_slots[device->irq_requested]);
	}
	pci_free_irq_vectors(pdev);
unmap_bar2:
	pci_iounmap(pdev, device->bar2);
unmap_bar0:
	pci_iounmap(pdev, device->bar0);
release_regions:
	pci_release_regions(pdev);
disable_device:
	pci_disable_device(pdev);
	return result;
}

static void mf_pci_remove(struct pci_dev *pdev)
{
	struct mf_pci_device *device = pci_get_drvdata(pdev);

	pci_set_drvdata(pdev, NULL);
	if (device == NULL)
		goto disable_device;
	while (device->irq_requested > 0) {
		--device->irq_requested;
		free_irq(pci_irq_vector(pdev, device->irq_requested),
			 &device->irq_slots[device->irq_requested]);
	}
	if (device->irq_vectors > 0)
		pci_free_irq_vectors(pdev);
	if (device->bar2 != NULL)
		pci_iounmap(pdev, device->bar2);
	if (device->bar0 != NULL)
		pci_iounmap(pdev, device->bar0);
disable_device:
	pci_clear_master(pdev);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

static const struct pci_device_id mf_pci_ids[] = {
	{ PCI_DEVICE(MF_PCI_VENDOR_ID, MF_PCI_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, mf_pci_ids);

static struct pci_driver mf_pci_driver = {
	.name = "metaflux_pci",
	.id_table = mf_pci_ids,
	.probe = mf_pci_probe,
	.remove = mf_pci_remove,
};

module_pci_driver(mf_pci_driver);

MODULE_DESCRIPTION("MetaFlux static guest PCI function driver");
MODULE_LICENSE("GPL");
