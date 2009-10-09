/******************************************************************************
 *
 * Copyright(c) 2008-2009 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <net/mac80211.h>
#include <linux/etherdevice.h>
#include <asm/unaligned.h>

#include "iwl-eeprom.h"
#include "iwl-dev.h"
#include "iwl-core.h"
#include "iwl-io.h"
#include "iwl-sta.h"
#include "iwl-helpers.h"
#include "iwl-5000-hw.h"
#include "iwl-6000-hw.h"
#include "iwl-agn-led.h"

/* Highest firmware API version supported */
#define IWL6000_UCODE_API_MAX 4
#define IWL6050_UCODE_API_MAX 4

/* Lowest firmware API version supported */
#define IWL6000_UCODE_API_MIN 1
#define IWL6050_UCODE_API_MIN 1

#define IWL6000_FW_PRE "iwlwifi-6000-"
#define _IWL6000_MODULE_FIRMWARE(api) IWL6000_FW_PRE #api ".ucode"
#define IWL6000_MODULE_FIRMWARE(api) _IWL6000_MODULE_FIRMWARE(api)

#define IWL6050_FW_PRE "iwlwifi-6050-"
#define _IWL6050_MODULE_FIRMWARE(api) IWL6050_FW_PRE #api ".ucode"
#define IWL6050_MODULE_FIRMWARE(api) _IWL6050_MODULE_FIRMWARE(api)

static void iwl6000_set_ct_threshold(struct iwl_priv *priv)
{
	/* want Celsius */
	priv->hw_params.ct_kill_threshold = CT_KILL_THRESHOLD;
	priv->hw_params.ct_kill_exit_threshold = CT_KILL_EXIT_THRESHOLD;
}

/* NIC configuration for 6000 series */
static void iwl6000_nic_config(struct iwl_priv *priv)
{
	u16 radio_cfg;

	radio_cfg = iwl_eeprom_query16(priv, EEPROM_RADIO_CONFIG);

	/* write radio config values to register */
	if (EEPROM_RF_CFG_TYPE_MSK(radio_cfg) <= EEPROM_RF_CONFIG_TYPE_MAX)
		iwl_set_bit(priv, CSR_HW_IF_CONFIG_REG,
			    EEPROM_RF_CFG_TYPE_MSK(radio_cfg) |
			    EEPROM_RF_CFG_STEP_MSK(radio_cfg) |
			    EEPROM_RF_CFG_DASH_MSK(radio_cfg));

	/* set CSR_HW_CONFIG_REG for uCode use */
	iwl_set_bit(priv, CSR_HW_IF_CONFIG_REG,
		    CSR_HW_IF_CONFIG_REG_BIT_RADIO_SI |
		    CSR_HW_IF_CONFIG_REG_BIT_MAC_SI);

	/* no locking required for register write */
	if (priv->cfg->pa_type == IWL_PA_HYBRID) {
		/* 2x2 hybrid phy type */
		iwl_write32(priv, CSR_GP_DRIVER_REG,
			     CSR_GP_DRIVER_REG_BIT_RADIO_SKU_2x2_HYB);
	} else if (priv->cfg->pa_type == IWL_PA_INTERNAL) {
		/* 2x2 IPA phy type */
		iwl_write32(priv, CSR_GP_DRIVER_REG,
			     CSR_GP_DRIVER_REG_BIT_RADIO_SKU_2x2_IPA);
	}
	/* else do nothing, uCode configured */
}

static struct iwl_sensitivity_ranges iwl6000_sensitivity = {
	.min_nrg_cck = 97,
	.max_nrg_cck = 0, /* not used, set to 0 */
	.auto_corr_min_ofdm = 80,
	.auto_corr_min_ofdm_mrc = 128,
	.auto_corr_min_ofdm_x1 = 105,
	.auto_corr_min_ofdm_mrc_x1 = 192,

	.auto_corr_max_ofdm = 145,
	.auto_corr_max_ofdm_mrc = 232,
	.auto_corr_max_ofdm_x1 = 145,
	.auto_corr_max_ofdm_mrc_x1 = 232,

	.auto_corr_min_cck = 125,
	.auto_corr_max_cck = 175,
	.auto_corr_min_cck_mrc = 160,
	.auto_corr_max_cck_mrc = 310,
	.nrg_th_cck = 97,
	.nrg_th_ofdm = 100,
};

static int iwl6000_hw_set_hw_params(struct iwl_priv *priv)
{
	if ((priv->cfg->mod_params->num_of_queues > IWL50_NUM_QUEUES) ||
	    (priv->cfg->mod_params->num_of_queues < IWL_MIN_NUM_QUEUES)) {
		IWL_ERR(priv,
			"invalid queues_num, should be between %d and %d\n",
			IWL_MIN_NUM_QUEUES, IWL50_NUM_QUEUES);
		return -EINVAL;
	}

	priv->hw_params.max_txq_num = priv->cfg->mod_params->num_of_queues;
	priv->hw_params.dma_chnl_num = FH50_TCSR_CHNL_NUM;
	priv->hw_params.scd_bc_tbls_size =
			IWL50_NUM_QUEUES * sizeof(struct iwl5000_scd_bc_tbl);
	priv->hw_params.tfd_size = sizeof(struct iwl_tfd);
	priv->hw_params.max_stations = IWL5000_STATION_COUNT;
	priv->hw_params.bcast_sta_id = IWL5000_BROADCAST_ID;

	priv->hw_params.max_data_size = IWL60_RTC_DATA_SIZE;
	priv->hw_params.max_inst_size = IWL60_RTC_INST_SIZE;

	priv->hw_params.max_bsm_size = 0;
	priv->hw_params.ht40_channel =  BIT(IEEE80211_BAND_2GHZ) |
					BIT(IEEE80211_BAND_5GHZ);
	priv->hw_params.rx_wrt_ptr_reg = FH_RSCSR_CHNL0_WPTR;

	priv->hw_params.tx_chains_num = num_of_ant(priv->cfg->valid_tx_ant);
	priv->hw_params.rx_chains_num = num_of_ant(priv->cfg->valid_rx_ant);
	priv->hw_params.valid_tx_ant = priv->cfg->valid_tx_ant;
	priv->hw_params.valid_rx_ant = priv->cfg->valid_rx_ant;

	if (priv->cfg->ops->lib->temp_ops.set_ct_kill)
		priv->cfg->ops->lib->temp_ops.set_ct_kill(priv);

	/* Set initial sensitivity parameters */
	/* Set initial calibration set */
	priv->hw_params.sens = &iwl6000_sensitivity;
	priv->hw_params.calib_init_cfg =
			BIT(IWL_CALIB_XTAL)		|
			BIT(IWL_CALIB_LO)		|
			BIT(IWL_CALIB_TX_IQ) 		|
			BIT(IWL_CALIB_TX_IQ_PERD)	|
			BIT(IWL_CALIB_BASE_BAND);

	return 0;
}

static struct iwl_lib_ops iwl6000_lib = {
	.set_hw_params = iwl6000_hw_set_hw_params,
	.txq_update_byte_cnt_tbl = iwl5000_txq_update_byte_cnt_tbl,
	.txq_inval_byte_cnt_tbl = iwl5000_txq_inval_byte_cnt_tbl,
	.txq_set_sched = iwl5000_txq_set_sched,
	.txq_agg_enable = iwl5000_txq_agg_enable,
	.txq_agg_disable = iwl5000_txq_agg_disable,
	.txq_attach_buf_to_tfd = iwl_hw_txq_attach_buf_to_tfd,
	.txq_free_tfd = iwl_hw_txq_free_tfd,
	.txq_init = iwl_hw_tx_queue_init,
	.rx_handler_setup = iwl5000_rx_handler_setup,
	.setup_deferred_work = iwl5000_setup_deferred_work,
	.is_valid_rtc_data_addr = iwl5000_hw_valid_rtc_data_addr,
	.load_ucode = iwl5000_load_ucode,
	.dump_nic_event_log = iwl_dump_nic_event_log,
	.dump_nic_error_log = iwl_dump_nic_error_log,
	.init_alive_start = iwl5000_init_alive_start,
	.alive_notify = iwl5000_alive_notify,
	.send_tx_power = iwl5000_send_tx_power,
	.update_chain_flags = iwl_update_chain_flags,
	.apm_ops = {
		.init =	iwl5000_apm_init,
		.reset = iwl5000_apm_reset,
		.stop = iwl_apm_stop,
		.config = iwl6000_nic_config,
		.set_pwr_src = iwl_set_pwr_src,
	},
	.eeprom_ops = {
		.regulatory_bands = {
			EEPROM_5000_REG_BAND_1_CHANNELS,
			EEPROM_5000_REG_BAND_2_CHANNELS,
			EEPROM_5000_REG_BAND_3_CHANNELS,
			EEPROM_5000_REG_BAND_4_CHANNELS,
			EEPROM_5000_REG_BAND_5_CHANNELS,
			EEPROM_5000_REG_BAND_24_HT40_CHANNELS,
			EEPROM_5000_REG_BAND_52_HT40_CHANNELS
		},
		.verify_signature  = iwlcore_eeprom_verify_signature,
		.acquire_semaphore = iwlcore_eeprom_acquire_semaphore,
		.release_semaphore = iwlcore_eeprom_release_semaphore,
		.calib_version	= iwl5000_eeprom_calib_version,
		.query_addr = iwl5000_eeprom_query_addr,
		.update_enhanced_txpower = iwlcore_eeprom_enhanced_txpower,
	},
	.post_associate = iwl_post_associate,
	.isr = iwl_isr_ict,
	.config_ap = iwl_config_ap,
	.temp_ops = {
		.temperature = iwl5000_temperature,
		.set_ct_kill = iwl6000_set_ct_threshold,
	 },
};

static struct iwl_ops iwl6000_ops = {
	.ucode = &iwl5000_ucode,
	.lib = &iwl6000_lib,
	.hcmd = &iwl5000_hcmd,
	.utils = &iwl5000_hcmd_utils,
	.led = &iwlagn_led_ops,
};


/*
 * "h": Hybrid configuration, use both internal and external Power Amplifier
 */
struct iwl_cfg iwl6000h_2agn_cfg = {
	.name = "6000 Series 2x2 AGN",
	.fw_name_pre = IWL6000_FW_PRE,
	.ucode_api_max = IWL6000_UCODE_API_MAX,
	.ucode_api_min = IWL6000_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G|IWL_SKU_N,
	.ops = &iwl6000_ops,
	.eeprom_size = OTP_LOW_IMAGE_SIZE,
	.eeprom_ver = EEPROM_6000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.mod_params = &iwl50_mod_params,
	.valid_tx_ant = ANT_AB,
	.valid_rx_ant = ANT_AB,
	.need_pll_cfg = false,
	.pa_type = IWL_PA_HYBRID,
	.max_ll_items = OTP_MAX_LL_ITEMS_6x00,
	.shadow_ram_support = true,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.use_rts_for_ht = true, /* use rts/cts protection */
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.supports_idle = true,
};

struct iwl_cfg iwl6000h_2abg_cfg = {
	.name = "6000 Series 2x2 ABG",
	.fw_name_pre = IWL6000_FW_PRE,
	.ucode_api_max = IWL6000_UCODE_API_MAX,
	.ucode_api_min = IWL6000_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G,
	.ops = &iwl6000_ops,
	.eeprom_size = OTP_LOW_IMAGE_SIZE,
	.eeprom_ver = EEPROM_6000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.mod_params = &iwl50_mod_params,
	.valid_tx_ant = ANT_AB,
	.valid_rx_ant = ANT_AB,
	.need_pll_cfg = false,
	.pa_type = IWL_PA_HYBRID,
	.max_ll_items = OTP_MAX_LL_ITEMS_6x00,
	.shadow_ram_support = true,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.supports_idle = true,
};

struct iwl_cfg iwl6000h_2bg_cfg = {
	.name = "6000 Series 2x2 BG",
	.fw_name_pre = IWL6000_FW_PRE,
	.ucode_api_max = IWL6000_UCODE_API_MAX,
	.ucode_api_min = IWL6000_UCODE_API_MIN,
	.sku = IWL_SKU_G,
	.ops = &iwl6000_ops,
	.eeprom_size = OTP_LOW_IMAGE_SIZE,
	.eeprom_ver = EEPROM_6000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.mod_params = &iwl50_mod_params,
	.valid_tx_ant = ANT_AB,
	.valid_rx_ant = ANT_AB,
	.need_pll_cfg = false,
	.pa_type = IWL_PA_HYBRID,
	.max_ll_items = OTP_MAX_LL_ITEMS_6x00,
	.shadow_ram_support = true,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.supports_idle = true,
};

/*
 * "i": Internal configuration, use internal Power Amplifier
 */
struct iwl_cfg iwl6000i_2agn_cfg = {
	.name = "6000 Series 2x2 AGN",
	.fw_name_pre = IWL6000_FW_PRE,
	.ucode_api_max = IWL6000_UCODE_API_MAX,
	.ucode_api_min = IWL6000_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G|IWL_SKU_N,
	.ops = &iwl6000_ops,
	.eeprom_size = OTP_LOW_IMAGE_SIZE,
	.eeprom_ver = EEPROM_6000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.mod_params = &iwl50_mod_params,
	.valid_tx_ant = ANT_BC,
	.valid_rx_ant = ANT_BC,
	.need_pll_cfg = false,
	.pa_type = IWL_PA_INTERNAL,
	.max_ll_items = OTP_MAX_LL_ITEMS_6x00,
	.shadow_ram_support = true,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.use_rts_for_ht = true, /* use rts/cts protection */
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.supports_idle = true,
};

struct iwl_cfg iwl6000i_2abg_cfg = {
	.name = "6000 Series 2x2 ABG",
	.fw_name_pre = IWL6000_FW_PRE,
	.ucode_api_max = IWL6000_UCODE_API_MAX,
	.ucode_api_min = IWL6000_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G,
	.ops = &iwl6000_ops,
	.eeprom_size = OTP_LOW_IMAGE_SIZE,
	.eeprom_ver = EEPROM_6000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.mod_params = &iwl50_mod_params,
	.valid_tx_ant = ANT_BC,
	.valid_rx_ant = ANT_BC,
	.need_pll_cfg = false,
	.pa_type = IWL_PA_INTERNAL,
	.max_ll_items = OTP_MAX_LL_ITEMS_6x00,
	.shadow_ram_support = true,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.supports_idle = true,
};

struct iwl_cfg iwl6000i_2bg_cfg = {
	.name = "6000 Series 2x2 BG",
	.fw_name_pre = IWL6000_FW_PRE,
	.ucode_api_max = IWL6000_UCODE_API_MAX,
	.ucode_api_min = IWL6000_UCODE_API_MIN,
	.sku = IWL_SKU_G,
	.ops = &iwl6000_ops,
	.eeprom_size = OTP_LOW_IMAGE_SIZE,
	.eeprom_ver = EEPROM_6000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.mod_params = &iwl50_mod_params,
	.valid_tx_ant = ANT_BC,
	.valid_rx_ant = ANT_BC,
	.need_pll_cfg = false,
	.pa_type = IWL_PA_INTERNAL,
	.max_ll_items = OTP_MAX_LL_ITEMS_6x00,
	.shadow_ram_support = true,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.supports_idle = true,
};

struct iwl_cfg iwl6050_2agn_cfg = {
	.name = "6050 Series 2x2 AGN",
	.fw_name_pre = IWL6050_FW_PRE,
	.ucode_api_max = IWL6050_UCODE_API_MAX,
	.ucode_api_min = IWL6050_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G|IWL_SKU_N,
	.ops = &iwl6000_ops,
	.eeprom_size = OTP_LOW_IMAGE_SIZE,
	.eeprom_ver = EEPROM_6000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.mod_params = &iwl50_mod_params,
	.valid_tx_ant = ANT_AB,
	.valid_rx_ant = ANT_AB,
	.need_pll_cfg = false,
	.pa_type = IWL_PA_SYSTEM,
	.max_ll_items = OTP_MAX_LL_ITEMS_6x00,
	.shadow_ram_support = true,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.use_rts_for_ht = true, /* use rts/cts protection */
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.supports_idle = true,
};

struct iwl_cfg iwl6050_2abg_cfg = {
	.name = "6050 Series 2x2 ABG",
	.fw_name_pre = IWL6050_FW_PRE,
	.ucode_api_max = IWL6050_UCODE_API_MAX,
	.ucode_api_min = IWL6050_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G,
	.ops = &iwl6000_ops,
	.eeprom_size = OTP_LOW_IMAGE_SIZE,
	.eeprom_ver = EEPROM_6000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.mod_params = &iwl50_mod_params,
	.valid_tx_ant = ANT_AB,
	.valid_rx_ant = ANT_AB,
	.need_pll_cfg = false,
	.pa_type = IWL_PA_SYSTEM,
	.max_ll_items = OTP_MAX_LL_ITEMS_6x00,
	.shadow_ram_support = true,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.supports_idle = true,
};

struct iwl_cfg iwl6000_3agn_cfg = {
	.name = "6000 Series 3x3 AGN",
	.fw_name_pre = IWL6000_FW_PRE,
	.ucode_api_max = IWL6000_UCODE_API_MAX,
	.ucode_api_min = IWL6000_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G|IWL_SKU_N,
	.ops = &iwl6000_ops,
	.eeprom_size = OTP_LOW_IMAGE_SIZE,
	.eeprom_ver = EEPROM_6000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.mod_params = &iwl50_mod_params,
	.valid_tx_ant = ANT_ABC,
	.valid_rx_ant = ANT_ABC,
	.need_pll_cfg = false,
	.pa_type = IWL_PA_SYSTEM,
	.max_ll_items = OTP_MAX_LL_ITEMS_6x00,
	.shadow_ram_support = true,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.use_rts_for_ht = true, /* use rts/cts protection */
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.supports_idle = true,
};

struct iwl_cfg iwl6050_3agn_cfg = {
	.name = "6050 Series 3x3 AGN",
	.fw_name_pre = IWL6050_FW_PRE,
	.ucode_api_max = IWL6050_UCODE_API_MAX,
	.ucode_api_min = IWL6050_UCODE_API_MIN,
	.sku = IWL_SKU_A|IWL_SKU_G|IWL_SKU_N,
	.ops = &iwl6000_ops,
	.eeprom_size = OTP_LOW_IMAGE_SIZE,
	.eeprom_ver = EEPROM_6000_EEPROM_VERSION,
	.eeprom_calib_ver = EEPROM_5000_TX_POWER_VERSION,
	.mod_params = &iwl50_mod_params,
	.valid_tx_ant = ANT_ABC,
	.valid_rx_ant = ANT_ABC,
	.need_pll_cfg = false,
	.pa_type = IWL_PA_SYSTEM,
	.max_ll_items = OTP_MAX_LL_ITEMS_6x00,
	.shadow_ram_support = true,
	.ht_greenfield_support = true,
	.led_compensation = 51,
	.use_rts_for_ht = true, /* use rts/cts protection */
	.chain_noise_num_beacons = IWL_CAL_NUM_BEACONS,
	.supports_idle = true,
};

MODULE_FIRMWARE(IWL6000_MODULE_FIRMWARE(IWL6000_UCODE_API_MAX));
MODULE_FIRMWARE(IWL6050_MODULE_FIRMWARE(IWL6050_UCODE_API_MAX));
