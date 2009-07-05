#ifndef __RFKILL_H
#define __RFKILL_H

/*
 * Copyright (C) 2006 - 2007 Ivo van Doorn
 * Copyright (C) 2007 Dmitry Torokhov
 * Copyright 2009 Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/types.h>

/* define userspace visible states */
#define RFKILL_STATE_SOFT_BLOCKED	0
#define RFKILL_STATE_UNBLOCKED		1
#define RFKILL_STATE_HARD_BLOCKED	2

/**
 * enum rfkill_type - type of rfkill switch.
 *
 * @RFKILL_TYPE_ALL: toggles all switches (userspace only)
 * @RFKILL_TYPE_WLAN: switch is on a 802.11 wireless network device.
 * @RFKILL_TYPE_BLUETOOTH: switch is on a bluetooth device.
 * @RFKILL_TYPE_UWB: switch is on a ultra wideband device.
 * @RFKILL_TYPE_WIMAX: switch is on a WiMAX device.
 * @RFKILL_TYPE_WWAN: switch is on a wireless WAN device.
 * @NUM_RFKILL_TYPES: number of defined rfkill types
 */
enum rfkill_type {
	RFKILL_TYPE_ALL = 0,
	RFKILL_TYPE_WLAN,
	RFKILL_TYPE_BLUETOOTH,
	RFKILL_TYPE_UWB,
	RFKILL_TYPE_WIMAX,
	RFKILL_TYPE_WWAN,
	NUM_RFKILL_TYPES,
};

/**
 * enum rfkill_operation - operation types
 * @RFKILL_OP_ADD: a device was added
 * @RFKILL_OP_DEL: a device was removed
 * @RFKILL_OP_CHANGE: a device's state changed -- userspace changes one device
 * @RFKILL_OP_CHANGE_ALL: userspace changes all devices (of a type, or all)
 */
enum rfkill_operation {
	RFKILL_OP_ADD = 0,
	RFKILL_OP_DEL,
	RFKILL_OP_CHANGE,
	RFKILL_OP_CHANGE_ALL,
};

/**
 * struct rfkill_event - events for userspace on /dev/rfkill
 * @idx: index of dev rfkill
 * @type: type of the rfkill struct
 * @op: operation code
 * @hard: hard state (0/1)
 * @soft: soft state (0/1)
 *
 * Structure used for userspace communication on /dev/rfkill,
 * used for events from the kernel and control to the kernel.
 */
struct rfkill_event {
	__u32 idx;
	__u8  type;
	__u8  op;
	__u8  soft, hard;
} __packed;

/* ioctl for turning off rfkill-input (if present) */
#define RFKILL_IOC_MAGIC	'R'
#define RFKILL_IOC_NOINPUT	1
#define RFKILL_IOCTL_NOINPUT	_IO(RFKILL_IOC_MAGIC, RFKILL_IOC_NOINPUT)

/* and that's all userspace gets */
#ifdef __KERNEL__
/* don't allow anyone to use these in the kernel */
enum rfkill_user_states {
	RFKILL_USER_STATE_SOFT_BLOCKED	= RFKILL_STATE_SOFT_BLOCKED,
	RFKILL_USER_STATE_UNBLOCKED	= RFKILL_STATE_UNBLOCKED,
	RFKILL_USER_STATE_HARD_BLOCKED	= RFKILL_STATE_HARD_BLOCKED,
};
#undef RFKILL_STATE_SOFT_BLOCKED
#undef RFKILL_STATE_UNBLOCKED
#undef RFKILL_STATE_HARD_BLOCKED

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/err.h>

/* this is opaque */
struct rfkill;

/**
 * struct rfkill_ops - rfkill driver methods
 *
 * @poll: poll the rfkill block state(s) -- only assign this method
 *	when you need polling. When called, simply call one of the
 *	rfkill_set{,_hw,_sw}_state family of functions. If the hw
 *	is getting unblocked you need to take into account the return
 *	value of those functions to make sure the software block is
 *	properly used.
 * @query: query the rfkill block state(s) and call exactly one of the
 *	rfkill_set{,_hw,_sw}_state family of functions. Assign this
 *	method if input events can cause hardware state changes to make
 *	the rfkill core query your driver before setting a requested
 *	block.
 * @set_block: turn the transmitter on (blocked == false) or off
 *	(blocked == true) -- ignore and return 0 when hard blocked.
 *	This callback must be assigned.
 */
struct rfkill_ops {
	void	(*poll)(struct rfkill *rfkill, void *data);
	void	(*query)(struct rfkill *rfkill, void *data);
	int	(*set_block)(void *data, bool blocked);
};

#if defined(CONFIG_RFKILL_BACKPORT) || defined(CONFIG_RFKILL_MODULE_BACKPORT)
/**
 * rfkill_alloc - allocate rfkill structure
 * @name: name of the struct -- the string is not copied internally
 * @parent: device that has rf switch on it
 * @type: type of the switch (RFKILL_TYPE_*)
 * @ops: rfkill methods
 * @ops_data: data passed to each method
 *
 * This function should be called by the transmitter driver to allocate an
 * rfkill structure. Returns %NULL on failure.
 */
struct rfkill * __must_check backport_rfkill_alloc(const char *name,
					  struct device *parent,
					  const enum rfkill_type type,
					  const struct rfkill_ops *ops,
					  void *ops_data);

/**
 * rfkill_register - Register a rfkill structure.
 * @rfkill: rfkill structure to be registered
 *
 * This function should be called by the transmitter driver to register
 * the rfkill structure. Before calling this function the driver needs
 * to be ready to service method calls from rfkill.
 *
 * If rfkill_init_sw_state() is not called before registration,
 * set_block() will be called to initialize the software blocked state
 * to a default value.
 *
 * If the hardware blocked state is not set before registration,
 * it is assumed to be unblocked.
 */
int __must_check backport_rfkill_register(struct rfkill *rfkill);

/**
 * rfkill_pause_polling(struct rfkill *rfkill)
 *
 * Pause polling -- say transmitter is off for other reasons.
 * NOTE: not necessary for suspend/resume -- in that case the
 * core stops polling anyway
 */
void backport_rfkill_pause_polling(struct rfkill *rfkill);

/**
 * rfkill_resume_polling(struct rfkill *rfkill)
 *
 * Pause polling -- say transmitter is off for other reasons.
 * NOTE: not necessary for suspend/resume -- in that case the
 * core stops polling anyway
 */
void backport_rfkill_resume_polling(struct rfkill *rfkill);


/**
 * rfkill_unregister - Unregister a rfkill structure.
 * @rfkill: rfkill structure to be unregistered
 *
 * This function should be called by the network driver during device
 * teardown to destroy rfkill structure. Until it returns, the driver
 * needs to be able to service method calls.
 */
void backport_rfkill_unregister(struct rfkill *rfkill);

/**
 * rfkill_destroy - free rfkill structure
 * @rfkill: rfkill structure to be destroyed
 *
 * Destroys the rfkill structure.
 */
void backport_rfkill_destroy(struct rfkill *rfkill);

/**
 * rfkill_set_hw_state - Set the internal rfkill hardware block state
 * @rfkill: pointer to the rfkill class to modify.
 * @state: the current hardware block state to set
 *
 * rfkill drivers that get events when the hard-blocked state changes
 * use this function to notify the rfkill core (and through that also
 * userspace) of the current state.  They should also use this after
 * resume if the state could have changed.
 *
 * You need not (but may) call this function if poll_state is assigned.
 *
 * This function can be called in any context, even from within rfkill
 * callbacks.
 *
 * The function returns the combined block state (true if transmitter
 * should be blocked) so that drivers need not keep track of the soft
 * block state -- which they might not be able to.
 */
bool __must_check backport_rfkill_set_hw_state(struct rfkill *rfkill, bool blocked);

/**
 * rfkill_set_sw_state - Set the internal rfkill software block state
 * @rfkill: pointer to the rfkill class to modify.
 * @state: the current software block state to set
 *
 * rfkill drivers that get events when the soft-blocked state changes
 * (yes, some platforms directly act on input but allow changing again)
 * use this function to notify the rfkill core (and through that also
 * userspace) of the current state.
 *
 * Drivers should also call this function after resume if the state has
 * been changed by the user.  This only makes sense for "persistent"
 * devices (see rfkill_init_sw_state()).
 *
 * This function can be called in any context, even from within rfkill
 * callbacks.
 *
 * The function returns the combined block state (true if transmitter
 * should be blocked).
 */
bool backport_rfkill_set_sw_state(struct rfkill *rfkill, bool blocked);

/**
 * rfkill_init_sw_state - Initialize persistent software block state
 * @rfkill: pointer to the rfkill class to modify.
 * @state: the current software block state to set
 *
 * rfkill drivers that preserve their software block state over power off
 * use this function to notify the rfkill core (and through that also
 * userspace) of their initial state.  It should only be used before
 * registration.
 *
 * In addition, it marks the device as "persistent", an attribute which
 * can be read by userspace.  Persistent devices are expected to preserve
 * their own state when suspended.
 */
void backport_rfkill_init_sw_state(struct rfkill *rfkill, bool blocked);

/**
 * rfkill_set_states - Set the internal rfkill block states
 * @rfkill: pointer to the rfkill class to modify.
 * @sw: the current software block state to set
 * @hw: the current hardware block state to set
 *
 * This function can be called in any context, even from within rfkill
 * callbacks.
 */
void backport_rfkill_set_states(struct rfkill *rfkill, bool sw, bool hw);

/**
 * rfkill_blocked - query rfkill block
 *
 * @rfkill: rfkill struct to query
 */
bool backport_rfkill_blocked(struct rfkill *rfkill);
#else /* !RFKILL_BACKPORT */
static inline struct rfkill * __must_check
backport_rfkill_alloc(const char *name,
	     struct device *parent,
	     const enum rfkill_type type,
	     const struct rfkill_ops *ops,
	     void *ops_data)
{
	return ERR_PTR(-ENODEV);
}

static inline int __must_check backport_rfkill_register(struct rfkill *rfkill)
{
	if (rfkill == ERR_PTR(-ENODEV))
		return 0;
	return -EINVAL;
}

static inline void backport_rfkill_pause_polling(struct rfkill *rfkill)
{
}

static inline void backport_rfkill_resume_polling(struct rfkill *rfkill)
{
}

static inline void backport_rfkill_unregister(struct rfkill *rfkill)
{
}

static inline void backport_rfkill_destroy(struct rfkill *rfkill)
{
}

static inline bool backport_rfkill_set_hw_state(struct rfkill *rfkill, bool blocked)
{
	return blocked;
}

static inline bool backport_rfkill_set_sw_state(struct rfkill *rfkill, bool blocked)
{
	return blocked;
}

static inline void backport_rfkill_init_sw_state(struct rfkill *rfkill, bool blocked)
{
}

static inline void backport_rfkill_set_states(struct rfkill *rfkill, bool sw, bool hw)
{
}

static inline bool backport_rfkill_blocked(struct rfkill *rfkill)
{
	return false;
}
#endif /* RFKILL_BACKPORT || RFKILL_BACKPORT_MODULE */


#ifdef CONFIG_RFKILL_BACKPORT_LEDS
/**
 * rfkill_get_led_trigger_name - Get the LED trigger name for the button's LED.
 * This function might return a NULL pointer if registering of the
 * LED trigger failed. Use this as "default_trigger" for the LED.
 */
const char *backport_rfkill_get_led_trigger_name(struct rfkill *rfkill);

/**
 * rfkill_set_led_trigger_name -- set the LED trigger name
 * @rfkill: rfkill struct
 * @name: LED trigger name
 *
 * This function sets the LED trigger name of the radio LED
 * trigger that rfkill creates. It is optional, but if called
 * must be called before rfkill_register() to be effective.
 */
void backport_rfkill_set_led_trigger_name(struct rfkill *rfkill, const char *name);
#else
static inline const char *backport_rfkill_get_led_trigger_name(struct rfkill *rfkill)
{
	return NULL;
}

static inline void
backport_rfkill_set_led_trigger_name(struct rfkill *rfkill, const char *name)
{
}
#endif

#endif /* __KERNEL__ */

#endif /* RFKILL_H */
