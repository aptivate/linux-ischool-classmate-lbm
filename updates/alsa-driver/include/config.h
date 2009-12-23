/* include/config.h.  Generated from config.h.in by configure.  */
/*
 *  Configuration header file for compilation of the ALSA driver
 */

#include "config1.h"

/* ALSA section */
#define CONFIG_SND_ISA 1
#define CONFIG_SND_SEQUENCER_MODULE 1
#define CONFIG_SND_OSSEMUL 1
#define CONFIG_SND_MIXER_OSS_MODULE 1
#define CONFIG_SND_PCM_OSS_MODULE 1
#define CONFIG_SND_PCM_OSS_PLUGINS 1
#define CONFIG_SND_SEQUENCER_OSS 1
#define CONFIG_SND_SUPPORT_OLD_API 1
#define CONFIG_SND_VERBOSE_PROCFS 1
#define CONFIG_SND_VERBOSE_PRINTK 1
/* #undef CONFIG_SND_DYNAMIC_MINORS */
/* #undef CONFIG_SND_DEBUG */
/* #undef CONFIG_SND_DEBUG_MEMORY */
/* #undef CONFIG_SND_DEBUG_VERBOSE */
/* #undef CONFIG_SND_BIT32_EMUL_MODULE */
#define CONFIG_SND_RTCTIMER_MODULE 1
/* #undef CONFIG_SND_HPET_MODULE */
#define CONFIG_SND_SEQ_DUMMY_MODULE 1

/* card-dependent section */
/* #undef CONFIG_SND_GENERIC_PM */

/* build section */
#define CONFIG_SND_KERNELDIR "/home/alsa/kernel/linux-2.6.17.8"
#define CONFIG_ISAPNP_KERNEL 1
#define CONFIG_PNP_KERNEL 1
#define CONFIG_SND_ISAPNP 1
#define CONFIG_SND_PNP 1

/* 2.4 kernels */
/* #undef CONFIG_HAVE_OLD_REQUEST_MODULE */
/* #undef CONFIG_HAVE_STRLCPY */
/* #undef CONFIG_HAVE_SNPRINTF */
/* #undef CONFIG_HAVE_VSNPRINTF */
#define CONFIG_HAVE_VMALLOC_TO_PAGE 1
#define CONFIG_HAVE_PDE 1
/* #undef CONFIG_HAVE_TTY_COUNT_ATOMIC */
#define CONFIG_HAVE_VIDEO_GET_DRVDATA 1
#define CONFIG_HAVE_DUMP_STACK 1
#define CONFIG_HAVE_FFS 1

/* 2.2 kernels */
/* #undef CONFIG_OLD_KILL_FASYNC */
/* #undef CONFIG_HAVE_DMA_ADDR_T */
/* #undef CONFIG_HAVE_MUTEX_MACROS */
/* #undef CONFIG_HAVE_SCNPRINTF */
/* #undef CONFIG_HAVE_SSCANF */
#define CONFIG_SND_HAS_BUILTIN_BOOL 1

/* 2.4/2.6 kernels */
#define CONFIG_HAVE_PCI_CONSISTENT_DMA_MASK 1
#define CONFIG_HAVE_KCALLOC 1
#define CONFIG_HAVE_KSTRDUP 1
/* #undef CONFIG_HAVE_KSTRNDUP */
#define CONFIG_HAVE_KZALLOC 1
#define CONFIG_HAVE_IO_REMAP_PFN_RANGE 1
/* #undef CONFIG_OLD_IO_REMAP_PAGE_RANGE */
/* #undef CONFIG_HAVE_PCI_IOREMAP_BAR */
#define CONFIG_HAVE_PCI_SAVED_CONFIG 1
#define CONFIG_HAVE_NEW_PCI_SAVE_STATE 1
#define CONFIG_HAVE_MSLEEP 1
#define CONFIG_HAVE_MSLEEP_INTERRUPTIBLE 1
#define CONFIG_HAVE_MSECS_TO_JIFFIES 1
#define CONFIG_HAVE_PCI_DEV_PRESENT 1
#define CONFIG_SND_HAVE_NEW_IOCTL 1
/* #undef CONFIG_SND_HAVE_CLASS_SIMPLE */
/* #undef CONFIG_CREATE_WORKQUEUE_FLAGS */
#define CONFIG_HAVE_REGISTER_SOUND_SPECIAL_DEVICE 1

/* 2.6 kernel */
#define CONFIG_SND_NESTED_CLASS_DEVICE 1
/* #undef CONFIG_SND_OLD_DRIVER_SUSPEND */
#define CONFIG_SND_REMOVE_PAGE_RESERVE 1
/* #undef CONFIG_SND_NEW_IRQ_HANDLER */
#define CONFIG_HAVE_PNP_SUSPEND 1
#define CONFIG_HAVE_GFP_T 1
/* #undef CONFIG_HAVE_INIT_UTSNAME */
/* #undef CONFIG_SND_HAS_DEVICE_CREATE_DRVDATA */
/* #undef CONFIG_HAVE_IS_POWER_OF_2 */
/* #undef CONFIG_HAVE_DEPRECATED_CONFIG_H */
#define CONFIG_HAVE_GFP_DMA32 1
#define CONFIG_HAVE_PAGE_TO_PFN 1
/* #undef CONFIG_HAVE_VIDEO_DRVDATA */
