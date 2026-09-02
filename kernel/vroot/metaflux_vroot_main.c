#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#include <metaflux/vroot/generated_profile.h>

#define MF_VROOT_DEFAULT_DOMAIN 0x7fffU
#define MF_VROOT_DEFAULT_BUS 0x7fU

struct mf_vroot_host {
	spinlock_t config_lock;
	struct pci_host_bridge *bridge;
	u8 config[MF_VROOT_PROFILE_MAX_FUNCTIONS][MF_VROOT_PROFILE_CONFIG_SIZE];
	u8 writable_mask[MF_VROOT_PROFILE_MAX_FUNCTIONS][MF_VROOT_PROFILE_CONFIG_SIZE];
	unsigned int function_count;
};

static struct mf_vroot_host *mf_vroot;
static unsigned int mf_vroot_function_count = 1;
static unsigned int mf_vroot_domain = MF_VROOT_DEFAULT_DOMAIN;
static unsigned int mf_vroot_bus = MF_VROOT_DEFAULT_BUS;

static int mf_vroot_set_function_count(const char *value,
					       const struct kernel_param *parameter)
{
	unsigned int count;
	unsigned long flags;
	unsigned int current_count;
	int result;

	(void)parameter;
	result = kstrtouint(value, 0, &count);
	if (result != 0 || count == 0U || count > MF_VROOT_PROFILE_MAX_FUNCTIONS)
		return -EINVAL;
	if (mf_vroot != NULL) {
		spin_lock_irqsave(&mf_vroot->config_lock, flags);
		current_count = mf_vroot->function_count;
		/* A lower count cannot remove already scanned PCI functions safely. */
		if (count < current_count) {
			spin_unlock_irqrestore(&mf_vroot->config_lock, flags);
			return -EBUSY;
		}
		mf_vroot->function_count = count;
		spin_unlock_irqrestore(&mf_vroot->config_lock, flags);
	}
	mf_vroot_function_count = count;
	return 0;
}

static const struct kernel_param_ops mf_vroot_function_count_ops = {
	.set = mf_vroot_set_function_count,
	.get = param_get_uint,
};

module_param_cb(function_count, &mf_vroot_function_count_ops,
		&mf_vroot_function_count, 0644);
MODULE_PARM_DESC(function_count,
		 "Logical functions exposed on rescan, in the range 1..8");
module_param_named(domain, mf_vroot_domain, uint, 0444);
MODULE_PARM_DESC(domain, "PCI domain used by the software root");
module_param_named(bus, mf_vroot_bus, uint, 0444);
MODULE_PARM_DESC(bus, "PCI bus number used by the software root");

static struct mf_vroot_host *mf_vroot_from_bus(struct pci_bus *bus)
{
	if (bus == NULL || bus->parent != NULL || bus->sysdata == NULL)
		return NULL;
	return bus->sysdata;
}

static bool mf_vroot_config_access_valid(int where, int size)
{
	return (size == 1 || size == 2 || size == 4) && where >= 0 &&
	       where <= (int)MF_VROOT_PROFILE_CONFIG_SIZE - size &&
	       (where & (size - 1)) == 0;
}

static bool mf_vroot_decode_function(unsigned int devfn, unsigned int *function)
{
	if (PCI_FUNC(devfn) != 0U || PCI_SLOT(devfn) >= MF_VROOT_PROFILE_MAX_FUNCTIONS)
		return false;
	*function = PCI_SLOT(devfn);
	return true;
}

static int mf_vroot_read_config(struct pci_bus *bus, unsigned int devfn, int where,
				int size, u32 *value)
{
	struct mf_vroot_host *host = mf_vroot_from_bus(bus);
	unsigned int function;
	unsigned long flags;
	int index;

	if (value == NULL || !mf_vroot_config_access_valid(where, size) ||
	    host == NULL || !mf_vroot_decode_function(devfn, &function)) {
		if (value != NULL)
			*value = ~0U;
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}
	*value = ~0U;
	spin_lock_irqsave(&host->config_lock, flags);
	if (function >= host->function_count) {
		spin_unlock_irqrestore(&host->config_lock, flags);
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	*value = 0U;
	for (index = 0; index < size; ++index)
		*value |= (u32)host->config[function][where + index] << (index * 8);
	spin_unlock_irqrestore(&host->config_lock, flags);
	return PCIBIOS_SUCCESSFUL;
}

static int mf_vroot_write_config(struct pci_bus *bus, unsigned int devfn, int where,
				 int size, u32 value)
{
	struct mf_vroot_host *host = mf_vroot_from_bus(bus);
	unsigned int function;
	unsigned long flags;
	int index;

	if (!mf_vroot_config_access_valid(where, size) || host == NULL ||
	    !mf_vroot_decode_function(devfn, &function))
		return PCIBIOS_BAD_REGISTER_NUMBER;
	spin_lock_irqsave(&host->config_lock, flags);
	if (function >= host->function_count) {
		spin_unlock_irqrestore(&host->config_lock, flags);
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	for (index = 0; index < size; ++index) {
		u8 mask = host->writable_mask[function][where + index];
		u8 byte = (u8)(value >> (index * 8));

		host->config[function][where + index] =
			(host->config[function][where + index] & (u8)~mask) | (byte & mask);
	}
	spin_unlock_irqrestore(&host->config_lock, flags);
	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops mf_vroot_pci_ops = {
	.read = mf_vroot_read_config,
	.write = mf_vroot_write_config,
};

static int mf_vroot_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	unsigned int bar;

	(void)id;
	if (pdev->class != MF_VROOT_PROFILE_CLASS_CODE)
		return -ENODEV;
	for (bar = 0; bar < PCI_STD_NUM_BARS; ++bar) {
		if (pci_resource_len(pdev, bar) != 0U ||
		    (pci_resource_flags(pdev, bar) & (IORESOURCE_MEM | IORESOURCE_IO)) != 0U)
			return -ENODEV;
	}
	pci_set_drvdata(pdev, mf_vroot);
	dev_info(&pdev->dev, "MetaFlux software root presentation function ready\n");
	return 0;
}

static void mf_vroot_remove(struct pci_dev *pdev)
{
	pci_set_drvdata(pdev, NULL);
}

static const struct pci_device_id mf_vroot_ids[] = {
	{ PCI_DEVICE(MF_VROOT_PROFILE_VENDOR_ID, MF_VROOT_PROFILE_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, mf_vroot_ids);

static struct pci_driver mf_vroot_driver = {
	.name = "metaflux_vroot",
	.id_table = mf_vroot_ids,
	.probe = mf_vroot_probe,
	.remove = mf_vroot_remove,
};

static int __init mf_vroot_init(void)
{
	struct pci_host_bridge *bridge;
	struct mf_vroot_host *host;
	unsigned int function;
	int result;

	if (mf_vroot_domain > 0xffffU || mf_vroot_bus > 0xffU ||
	    mf_vroot_function_count == 0U ||
	    mf_vroot_function_count > MF_VROOT_PROFILE_MAX_FUNCTIONS)
		return -EINVAL;
	bridge = pci_alloc_host_bridge(sizeof(*host));
	if (bridge == NULL)
		return -ENOMEM;
	host = pci_host_bridge_priv(bridge);
	memset(host, 0, sizeof(*host));
	spin_lock_init(&host->config_lock);
	host->bridge = bridge;
	host->function_count = mf_vroot_function_count;
	for (function = 0; function < MF_VROOT_PROFILE_MAX_FUNCTIONS; ++function) {
		memcpy(host->config[function], mf_vroot_profile_config_template,
		       MF_VROOT_PROFILE_CONFIG_SIZE);
		memcpy(host->writable_mask[function], mf_vroot_profile_writable_mask,
		       MF_VROOT_PROFILE_CONFIG_SIZE);
	}
	bridge->ops = &mf_vroot_pci_ops;
	bridge->sysdata = host;
	bridge->busnr = (int)mf_vroot_bus;
	bridge->domain_nr = (int)mf_vroot_domain;
	result = pci_register_driver(&mf_vroot_driver);
	if (result != 0) {
		pci_free_host_bridge(bridge);
		return result;
	}
	mf_vroot = host;
	result = pci_host_probe(bridge);
	if (result != 0) {
		mf_vroot = NULL;
		pci_unregister_driver(&mf_vroot_driver);
		pci_free_host_bridge(bridge);
		return result;
	}
	dev_info(&bridge->dev, "MetaFlux software PCI root ready: domain=%04x bus=%02x functions=%u\n",
		 mf_vroot_domain, mf_vroot_bus, host->function_count);
	return 0;
}

static void __exit mf_vroot_exit(void)
{
	struct pci_host_bridge *bridge;

	bridge = mf_vroot == NULL ? NULL : mf_vroot->bridge;
	mf_vroot = NULL;
	pci_unregister_driver(&mf_vroot_driver);
	if (bridge != NULL && bridge->bus != NULL) {
		pci_stop_root_bus(bridge->bus);
		pci_remove_root_bus(bridge->bus);
	}
	if (bridge != NULL)
		pci_free_host_bridge(bridge);
}

module_init(mf_vroot_init);
module_exit(mf_vroot_exit);

MODULE_DESCRIPTION("MetaFlux default-off software PCI root presentation");
MODULE_LICENSE("GPL");
