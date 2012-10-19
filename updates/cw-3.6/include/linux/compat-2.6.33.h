#ifndef LINUX_26_33_COMPAT_H
#define LINUX_26_33_COMPAT_H

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33))

#include <linux/skbuff.h>
#include <linux/pci.h>
#if defined(CONFIG_PCCARD) || defined(CONFIG_PCCARD_MODULE)
#include <pcmcia/cs_types.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>
#endif
#include <linux/firmware.h>
#include <linux/input.h>

#if defined(CONFIG_COMPAT_FIRMWARE_CLASS)
#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
#define release_firmware compat_release_firmware
#define request_firmware compat_request_firmware
#define request_firmware_nowait compat_request_firmware_nowait
#endif

#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
int compat_request_firmware(const struct firmware **fw, const char *name,
		     struct device *device);
int compat_request_firmware_nowait(
	struct module *module, int uevent,
	const char *name, struct device *device, gfp_t gfp, void *context,
	void (*cont)(const struct firmware *fw, void *context));

void compat_release_firmware(const struct firmware *fw);
#else
static inline int compat_request_firmware(const struct firmware **fw,
				   const char *name,
				   struct device *device)
{
	return -EINVAL;
}
static inline int compat_request_firmware_nowait(
	struct module *module, int uevent,
	const char *name, struct device *device, gfp_t gfp, void *context,
	void (*cont)(const struct firmware *fw, void *context))
{
	return -EINVAL;
}

static inline void compat_release_firmware(const struct firmware *fw)
{
}
#endif
#endif

/* mask KEY_RFKILL as RHEL6 backports this */
#if !defined(KEY_RFKILL)
#define KEY_RFKILL		247	/* Key that controls all radios */
#endif

/* mask IFF_DONT_BRIDGE as RHEL6 backports this */
#if !defined(IFF_DONT_BRIDGE)
#define IFF_DONT_BRIDGE 0x800		/* disallow bridging this ether dev */
/* source: include/linux/if.h */
#endif

/* mask NETDEV_POST_INIT as RHEL6 backports this */
/* this will never happen on older kernels */
#if !defined(NETDEV_POST_INIT)
#define NETDEV_POST_INIT 0xffff
#endif

/* mask netdev_alloc_skb_ip_align as debian squeeze also backports this */
#define netdev_alloc_skb_ip_align(a, b) compat_netdev_alloc_skb_ip_align(a, b)

static inline struct sk_buff *netdev_alloc_skb_ip_align(struct net_device *dev,
                unsigned int length)
{
	struct sk_buff *skb = netdev_alloc_skb(dev, length + NET_IP_ALIGN);

	if (NET_IP_ALIGN && skb)
		skb_reserve(skb, NET_IP_ALIGN);
	return skb;
}

#if defined(CONFIG_PCCARD) || defined(CONFIG_PCCARD_MODULE)

#if defined(CONFIG_PCMCIA) || defined(CONFIG_PCMCIA_MODULE)

#define pcmcia_request_window(a, b, c) pcmcia_request_window(&a, b, c)

#define pcmcia_map_mem_page(a, b, c) pcmcia_map_mem_page(b, c)

/* loop over CIS entries */
int pcmcia_loop_tuple(struct pcmcia_device *p_dev, cisdata_t code,
		      int (*loop_tuple) (struct pcmcia_device *p_dev,
					 tuple_t *tuple,
					 void *priv_data),
		      void *priv_data);

#endif /* CONFIG_PCMCIA */

/* loop over CIS entries */
int pccard_loop_tuple(struct pcmcia_socket *s, unsigned int function,
		      cisdata_t code, cisparse_t *parse, void *priv_data,
		      int (*loop_tuple) (tuple_t *tuple,
					 cisparse_t *parse,
					 void *priv_data));

#endif /* CONFIG_PCCARD */

/**
 * list_for_each_entry_continue_rcu - continue iteration over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 *
 * Continue to iterate over list of given type, continuing after
 * the current position.
 */
#define list_for_each_entry_continue_rcu(pos, head, member) 		\
	for (pos = list_entry_rcu(pos->member.next, typeof(*pos), member); \
	     prefetch(pos->member.next), &pos->member != (head);	\
	     pos = list_entry_rcu(pos->member.next, typeof(*pos), member))

#define sock_recv_ts_and_drops(msg, sk, skb) sock_recv_timestamp(msg, sk, skb)

/* mask pci_pcie_cap as debian squeeze also backports this */
#define pci_pcie_cap(a) compat_pci_pcie_cap(a)

/**
 * pci_pcie_cap - get the saved PCIe capability offset
 * @dev: PCI device
 *
 * PCIe capability offset is calculated at PCI device initialization
 * time and saved in the data structure. This function returns saved
 * PCIe capability offset. Using this instead of pci_find_capability()
 * reduces unnecessary search in the PCI configuration space. If you
 * need to calculate PCIe capability offset from raw device for some
 * reasons, please use pci_find_capability() instead.
 */
static inline int pci_pcie_cap(struct pci_dev *dev)
{
	return pci_find_capability(dev, PCI_CAP_ID_EXP);
}

/* mask pci_is_pcie as RHEL6 backports this */
#define pci_is_pcie(a) compat_pci_is_pcie(a)

/**
 * pci_is_pcie - check if the PCI device is PCI Express capable
 * @dev: PCI device
 *
 * Retrun true if the PCI device is PCI Express capable, false otherwise.
 */
static inline bool pci_is_pcie(struct pci_dev *dev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	return dev->is_pcie;
#else
	return !!pci_pcie_cap(dev);
#endif
}

#ifdef __GNUC__
#define __always_unused			__attribute__((unused))
#else
#define __always_unused			/* unimplemented */
#endif

/* mask IS_ERR_OR_NULL as debian squeeze also backports this */
#define IS_ERR_OR_NULL(a) compat_IS_ERR_OR_NULL(a)

static inline long __must_check IS_ERR_OR_NULL(const void *ptr)
{
	return !ptr || IS_ERR_VALUE((unsigned long)ptr);
}

#if (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,32))
#undef SIMPLE_DEV_PM_OPS
#define SIMPLE_DEV_PM_OPS(name, suspend_fn, resume_fn) \
const struct dev_pm_ops name = { \
	.suspend = suspend_fn, \
	.resume = resume_fn, \
	.freeze = suspend_fn, \
	.thaw = resume_fn, \
	.poweroff = suspend_fn, \
	.restore = resume_fn, \
}
#endif /* (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,32)) */

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)) */

#endif /* LINUX_26_33_COMPAT_H */