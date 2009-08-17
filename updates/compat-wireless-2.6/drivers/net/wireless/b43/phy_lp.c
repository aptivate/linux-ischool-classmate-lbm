/*

  Broadcom B43 wireless driver
  IEEE 802.11g LP-PHY driver

  Copyright (c) 2008-2009 Michael Buesch <mb@bu3sch.de>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
  Boston, MA 02110-1301, USA.

*/

#include "b43.h"
#include "main.h"
#include "phy_lp.h"
#include "phy_common.h"
#include "tables_lpphy.h"


static int b43_lpphy_op_allocate(struct b43_wldev *dev)
{
	struct b43_phy_lp *lpphy;

	lpphy = kzalloc(sizeof(*lpphy), GFP_KERNEL);
	if (!lpphy)
		return -ENOMEM;
	dev->phy.lp = lpphy;

	return 0;
}

static void b43_lpphy_op_prepare_structs(struct b43_wldev *dev)
{
	struct b43_phy *phy = &dev->phy;
	struct b43_phy_lp *lpphy = phy->lp;

	memset(lpphy, 0, sizeof(*lpphy));

	//TODO
}

static void b43_lpphy_op_free(struct b43_wldev *dev)
{
	struct b43_phy_lp *lpphy = dev->phy.lp;

	kfree(lpphy);
	dev->phy.lp = NULL;
}

static void lpphy_read_band_sprom(struct b43_wldev *dev)
{
	struct b43_phy_lp *lpphy = dev->phy.lp;
	struct ssb_bus *bus = dev->dev->bus;
	u16 cckpo, maxpwr;
	u32 ofdmpo;
	int i;

	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
		lpphy->tx_isolation_med_band = bus->sprom.tri2g;
		lpphy->bx_arch = bus->sprom.bxa2g;
		lpphy->rx_pwr_offset = bus->sprom.rxpo2g;
		lpphy->rssi_vf = bus->sprom.rssismf2g;
		lpphy->rssi_vc = bus->sprom.rssismc2g;
		lpphy->rssi_gs = bus->sprom.rssisav2g;
		lpphy->txpa[0] = bus->sprom.pa0b0;
		lpphy->txpa[1] = bus->sprom.pa0b1;
		lpphy->txpa[2] = bus->sprom.pa0b2;
		maxpwr = bus->sprom.maxpwr_bg;
		lpphy->max_tx_pwr_med_band = maxpwr;
		cckpo = bus->sprom.cck2gpo;
		ofdmpo = bus->sprom.ofdm2gpo;
		if (cckpo) {
			for (i = 0; i < 4; i++) {
				lpphy->tx_max_rate[i] =
					maxpwr - (ofdmpo & 0xF) * 2;
				ofdmpo >>= 4;
			}
			ofdmpo = bus->sprom.ofdm2gpo;
			for (i = 4; i < 15; i++) {
				lpphy->tx_max_rate[i] =
					maxpwr - (ofdmpo & 0xF) * 2;
				ofdmpo >>= 4;
			}
		} else {
			ofdmpo &= 0xFF;
			for (i = 0; i < 4; i++)
				lpphy->tx_max_rate[i] = maxpwr;
			for (i = 4; i < 15; i++)
				lpphy->tx_max_rate[i] = maxpwr - ofdmpo;
		}
	} else { /* 5GHz */
		lpphy->tx_isolation_low_band = bus->sprom.tri5gl;
		lpphy->tx_isolation_med_band = bus->sprom.tri5g;
		lpphy->tx_isolation_hi_band = bus->sprom.tri5gh;
		lpphy->bx_arch = bus->sprom.bxa5g;
		lpphy->rx_pwr_offset = bus->sprom.rxpo5g;
		lpphy->rssi_vf = bus->sprom.rssismf5g;
		lpphy->rssi_vc = bus->sprom.rssismc5g;
		lpphy->rssi_gs = bus->sprom.rssisav5g;
		lpphy->txpa[0] = bus->sprom.pa1b0;
		lpphy->txpa[1] = bus->sprom.pa1b1;
		lpphy->txpa[2] = bus->sprom.pa1b2;
		lpphy->txpal[0] = bus->sprom.pa1lob0;
		lpphy->txpal[1] = bus->sprom.pa1lob1;
		lpphy->txpal[2] = bus->sprom.pa1lob2;
		lpphy->txpah[0] = bus->sprom.pa1hib0;
		lpphy->txpah[1] = bus->sprom.pa1hib1;
		lpphy->txpah[2] = bus->sprom.pa1hib2;
		maxpwr = bus->sprom.maxpwr_al;
		ofdmpo = bus->sprom.ofdm5glpo;
		lpphy->max_tx_pwr_low_band = maxpwr;
		for (i = 4; i < 12; i++) {
			lpphy->tx_max_ratel[i] = maxpwr - (ofdmpo & 0xF) * 2;
			ofdmpo >>= 4;
		}
		maxpwr = bus->sprom.maxpwr_a;
		ofdmpo = bus->sprom.ofdm5gpo;
		lpphy->max_tx_pwr_med_band = maxpwr;
		for (i = 4; i < 12; i++) {
			lpphy->tx_max_rate[i] = maxpwr - (ofdmpo & 0xF) * 2;
			ofdmpo >>= 4;
		}
		maxpwr = bus->sprom.maxpwr_ah;
		ofdmpo = bus->sprom.ofdm5ghpo;
		lpphy->max_tx_pwr_hi_band = maxpwr;
		for (i = 4; i < 12; i++) {
			lpphy->tx_max_rateh[i] = maxpwr - (ofdmpo & 0xF) * 2;
			ofdmpo >>= 4;
		}
	}
}

static void lpphy_adjust_gain_table(struct b43_wldev *dev)
{
	struct b43_phy_lp *lpphy = dev->phy.lp;
	u32 freq = dev->wl->hw->conf.channel->center_freq;
	u16 temp[3];
	u16 isolation;

	B43_WARN_ON(dev->phy.rev >= 2);

	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ)
		isolation = lpphy->tx_isolation_med_band;
	else if (freq <= 5320)
		isolation = lpphy->tx_isolation_low_band;
	else if (freq <= 5700)
		isolation = lpphy->tx_isolation_med_band;
	else
		isolation = lpphy->tx_isolation_hi_band;

	temp[0] = ((isolation - 26) / 12) << 12;
	temp[1] = temp[0] + 0x1000;
	temp[2] = temp[0] + 0x2000;

	b43_lptab_write_bulk(dev, B43_LPTAB16(12, 0), 3, temp);
	b43_lptab_write_bulk(dev, B43_LPTAB16(13, 0), 3, temp);
}

static void lpphy_table_init(struct b43_wldev *dev)
{
	if (dev->phy.rev < 2)
		lpphy_rev0_1_table_init(dev);
	else
		lpphy_rev2plus_table_init(dev);

	lpphy_init_tx_gain_table(dev);

	if (dev->phy.rev < 2)
		lpphy_adjust_gain_table(dev);
}

static void lpphy_baseband_rev0_1_init(struct b43_wldev *dev)
{
	struct ssb_bus *bus = dev->dev->bus;
	u16 tmp, tmp2;

	if (dev->phy.rev == 1 &&
	   (bus->sprom.boardflags_hi & B43_BFH_FEM_BT)) {
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_1, 0xFFC0, 0x000A);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_1, 0x3F00, 0x0900);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_2, 0xFFC0, 0x000A);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_2, 0xC0FF, 0x0B00);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_3, 0xFFC0, 0x000A);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_3, 0xC0FF, 0x0400);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_4, 0xFFC0, 0x000A);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_4, 0xC0FF, 0x0B00);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_5, 0xFFC0, 0x000A);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_5, 0xC0FF, 0x0900);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_6, 0xFFC0, 0x000A);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_6, 0xC0FF, 0x0B00);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_7, 0xFFC0, 0x000A);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_7, 0xC0FF, 0x0900);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_8, 0xFFC0, 0x000A);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_8, 0xC0FF, 0x0B00);
	} else if (b43_current_band(dev->wl) == IEEE80211_BAND_5GHZ ||
		  (bus->boardinfo.type == 0x048A) || ((dev->phy.rev == 0) &&
		  (bus->sprom.boardflags_lo & B43_BFL_FEM))) {
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_1, 0xFFC0, 0x0001);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_1, 0xC0FF, 0x0400);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_2, 0xFFC0, 0x0001);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_2, 0xC0FF, 0x0500);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_3, 0xFFC0, 0x0002);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_3, 0xC0FF, 0x0800);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_4, 0xFFC0, 0x0002);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_4, 0xC0FF, 0x0A00);
	} else if (dev->phy.rev == 1 ||
		  (bus->sprom.boardflags_lo & B43_BFL_FEM)) {
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_1, 0xFFC0, 0x0004);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_1, 0xC0FF, 0x0800);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_2, 0xFFC0, 0x0004);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_2, 0xC0FF, 0x0C00);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_3, 0xFFC0, 0x0002);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_3, 0xC0FF, 0x0100);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_4, 0xFFC0, 0x0002);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_4, 0xC0FF, 0x0300);
	} else {
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_1, 0xFFC0, 0x000A);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_1, 0xC0FF, 0x0900);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_2, 0xFFC0, 0x000A);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_2, 0xC0FF, 0x0B00);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_3, 0xFFC0, 0x0006);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_3, 0xC0FF, 0x0500);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_4, 0xFFC0, 0x0006);
		b43_phy_maskset(dev, B43_LPPHY_TR_LOOKUP_4, 0xC0FF, 0x0700);
	}
	if (dev->phy.rev == 1) {
		b43_phy_copy(dev, B43_LPPHY_TR_LOOKUP_5, B43_LPPHY_TR_LOOKUP_1);
		b43_phy_copy(dev, B43_LPPHY_TR_LOOKUP_6, B43_LPPHY_TR_LOOKUP_2);
		b43_phy_copy(dev, B43_LPPHY_TR_LOOKUP_7, B43_LPPHY_TR_LOOKUP_3);
		b43_phy_copy(dev, B43_LPPHY_TR_LOOKUP_8, B43_LPPHY_TR_LOOKUP_4);
	}
	if ((bus->sprom.boardflags_hi & B43_BFH_FEM_BT) &&
	    (bus->chip_id == 0x5354) &&
	    (bus->chip_package == SSB_CHIPPACK_BCM4712S)) {
		b43_phy_set(dev, B43_LPPHY_CRSGAIN_CTL, 0x0006);
		b43_phy_write(dev, B43_LPPHY_GPIO_SELECT, 0x0005);
		b43_phy_write(dev, B43_LPPHY_GPIO_OUTEN, 0xFFFF);
		b43_hf_write(dev, b43_hf_read(dev) | B43_HF_PR45960W);
	}
	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
		b43_phy_set(dev, B43_LPPHY_LP_PHY_CTL, 0x8000);
		b43_phy_set(dev, B43_LPPHY_CRSGAIN_CTL, 0x0040);
		b43_phy_maskset(dev, B43_LPPHY_MINPWR_LEVEL, 0x00FF, 0xA400);
		b43_phy_maskset(dev, B43_LPPHY_CRSGAIN_CTL, 0xF0FF, 0x0B00);
		b43_phy_maskset(dev, B43_LPPHY_SYNCPEAKCNT, 0xFFF8, 0x0007);
		b43_phy_maskset(dev, B43_LPPHY_DSSS_CONFIRM_CNT, 0xFFF8, 0x0003);
		b43_phy_maskset(dev, B43_LPPHY_DSSS_CONFIRM_CNT, 0xFFC7, 0x0020);
		b43_phy_mask(dev, B43_LPPHY_IDLEAFTERPKTRXTO, 0x00FF);
	} else { /* 5GHz */
		b43_phy_mask(dev, B43_LPPHY_LP_PHY_CTL, 0x7FFF);
		b43_phy_mask(dev, B43_LPPHY_CRSGAIN_CTL, 0xFFBF);
	}
	if (dev->phy.rev == 1) {
		tmp = b43_phy_read(dev, B43_LPPHY_CLIPCTRTHRESH);
		tmp2 = (tmp & 0x03E0) >> 5;
		tmp2 |= tmp << 5;
		b43_phy_write(dev, B43_LPPHY_4C3, tmp2);
		tmp = b43_phy_read(dev, B43_LPPHY_OFDMSYNCTHRESH0);
		tmp2 = (tmp & 0x1F00) >> 8;
		tmp2 |= tmp << 5;
		b43_phy_write(dev, B43_LPPHY_4C4, tmp2);
		tmp = b43_phy_read(dev, B43_LPPHY_VERYLOWGAINDB);
		tmp2 = tmp & 0x00FF;
		tmp2 |= tmp << 8;
		b43_phy_write(dev, B43_LPPHY_4C5, tmp2);
	}
}

static void lpphy_save_dig_flt_state(struct b43_wldev *dev)
{
	static const u16 addr[] = {
		B43_PHY_OFDM(0xC1),
		B43_PHY_OFDM(0xC2),
		B43_PHY_OFDM(0xC3),
		B43_PHY_OFDM(0xC4),
		B43_PHY_OFDM(0xC5),
		B43_PHY_OFDM(0xC6),
		B43_PHY_OFDM(0xC7),
		B43_PHY_OFDM(0xC8),
		B43_PHY_OFDM(0xCF),
	};

	static const u16 coefs[] = {
		0xDE5E, 0xE832, 0xE331, 0x4D26,
		0x0026, 0x1420, 0x0020, 0xFE08,
		0x0008,
	};

	struct b43_phy_lp *lpphy = dev->phy.lp;
	int i;

	for (i = 0; i < ARRAY_SIZE(addr); i++) {
		lpphy->dig_flt_state[i] = b43_phy_read(dev, addr[i]);
		b43_phy_write(dev, addr[i], coefs[i]);
	}
}

static void lpphy_restore_dig_flt_state(struct b43_wldev *dev)
{
	static const u16 addr[] = {
		B43_PHY_OFDM(0xC1),
		B43_PHY_OFDM(0xC2),
		B43_PHY_OFDM(0xC3),
		B43_PHY_OFDM(0xC4),
		B43_PHY_OFDM(0xC5),
		B43_PHY_OFDM(0xC6),
		B43_PHY_OFDM(0xC7),
		B43_PHY_OFDM(0xC8),
		B43_PHY_OFDM(0xCF),
	};

	struct b43_phy_lp *lpphy = dev->phy.lp;
	int i;

	for (i = 0; i < ARRAY_SIZE(addr); i++)
		b43_phy_write(dev, addr[i], lpphy->dig_flt_state[i]);
}

static void lpphy_baseband_rev2plus_init(struct b43_wldev *dev)
{
	struct ssb_bus *bus = dev->dev->bus;
	struct b43_phy_lp *lpphy = dev->phy.lp;

	b43_phy_write(dev, B43_LPPHY_AFE_DAC_CTL, 0x50);
	b43_phy_write(dev, B43_LPPHY_AFE_CTL, 0x8800);
	b43_phy_write(dev, B43_LPPHY_AFE_CTL_OVR, 0);
	b43_phy_write(dev, B43_LPPHY_AFE_CTL_OVRVAL, 0);
	b43_phy_write(dev, B43_LPPHY_RF_OVERRIDE_0, 0);
	b43_phy_write(dev, B43_LPPHY_RF_OVERRIDE_2, 0);
	b43_phy_write(dev, B43_PHY_OFDM(0xF9), 0);
	b43_phy_write(dev, B43_LPPHY_TR_LOOKUP_1, 0);
	b43_phy_set(dev, B43_LPPHY_ADC_COMPENSATION_CTL, 0x10);
	b43_phy_maskset(dev, B43_LPPHY_OFDMSYNCTHRESH0, 0xFF00, 0xB4);
	b43_phy_maskset(dev, B43_LPPHY_DCOFFSETTRANSIENT, 0xF8FF, 0x200);
	b43_phy_maskset(dev, B43_LPPHY_DCOFFSETTRANSIENT, 0xFF00, 0x7F);
	b43_phy_maskset(dev, B43_LPPHY_GAINDIRECTMISMATCH, 0xFF0F, 0x40);
	b43_phy_maskset(dev, B43_LPPHY_PREAMBLECONFIRMTO, 0xFF00, 0x2);
	b43_phy_mask(dev, B43_LPPHY_CRSGAIN_CTL, ~0x4000);
	b43_phy_mask(dev, B43_LPPHY_CRSGAIN_CTL, ~0x2000);
	b43_phy_set(dev, B43_PHY_OFDM(0x10A), 0x1);
	if (bus->boardinfo.rev >= 0x18) {
		b43_lptab_write(dev, B43_LPTAB32(17, 65), 0xEC);
		b43_phy_maskset(dev, B43_PHY_OFDM(0x10A), 0xFF01, 0x14);
	} else {
		b43_phy_maskset(dev, B43_PHY_OFDM(0x10A), 0xFF01, 0x10);
	}
	b43_phy_maskset(dev, B43_PHY_OFDM(0xDF), 0xFF00, 0xF4);
	b43_phy_maskset(dev, B43_PHY_OFDM(0xDF), 0x00FF, 0xF100);
	b43_phy_write(dev, B43_LPPHY_CLIPTHRESH, 0x48);
	b43_phy_maskset(dev, B43_LPPHY_HIGAINDB, 0xFF00, 0x46);
	b43_phy_maskset(dev, B43_PHY_OFDM(0xE4), 0xFF00, 0x10);
	b43_phy_maskset(dev, B43_LPPHY_PWR_THRESH1, 0xFFF0, 0x9);
	b43_phy_mask(dev, B43_LPPHY_GAINDIRECTMISMATCH, ~0xF);
	b43_phy_maskset(dev, B43_LPPHY_VERYLOWGAINDB, 0x00FF, 0x5500);
	b43_phy_maskset(dev, B43_LPPHY_CLIPCTRTHRESH, 0xF81F, 0xA0);
	b43_phy_maskset(dev, B43_LPPHY_GAINDIRECTMISMATCH, 0xE0FF, 0x300);
	b43_phy_maskset(dev, B43_LPPHY_HIGAINDB, 0x00FF, 0x2A00);
	if ((bus->chip_id == 0x4325) && (bus->chip_rev == 0)) {
		b43_phy_maskset(dev, B43_LPPHY_LOWGAINDB, 0x00FF, 0x2100);
		b43_phy_maskset(dev, B43_LPPHY_VERYLOWGAINDB, 0xFF00, 0xA);
	} else {
		b43_phy_maskset(dev, B43_LPPHY_LOWGAINDB, 0x00FF, 0x1E00);
		b43_phy_maskset(dev, B43_LPPHY_VERYLOWGAINDB, 0xFF00, 0xD);
	}
	b43_phy_maskset(dev, B43_PHY_OFDM(0xFE), 0xFFE0, 0x1F);
	b43_phy_maskset(dev, B43_PHY_OFDM(0xFF), 0xFFE0, 0xC);
	b43_phy_maskset(dev, B43_PHY_OFDM(0x100), 0xFF00, 0x19);
	b43_phy_maskset(dev, B43_PHY_OFDM(0xFF), 0x03FF, 0x3C00);
	b43_phy_maskset(dev, B43_PHY_OFDM(0xFE), 0xFC1F, 0x3E0);
	b43_phy_maskset(dev, B43_PHY_OFDM(0xFF), 0xFFE0, 0xC);
	b43_phy_maskset(dev, B43_PHY_OFDM(0x100), 0x00FF, 0x1900);
	b43_phy_maskset(dev, B43_LPPHY_CLIPCTRTHRESH, 0x83FF, 0x5800);
	b43_phy_maskset(dev, B43_LPPHY_CLIPCTRTHRESH, 0xFFE0, 0x12);
	b43_phy_maskset(dev, B43_LPPHY_GAINMISMATCH, 0x0FFF, 0x9000);

	if ((bus->chip_id == 0x4325) && (bus->chip_rev == 1)) {
		b43_lptab_write(dev, B43_LPTAB16(0x08, 0x14), 0);
		b43_lptab_write(dev, B43_LPTAB16(0x08, 0x12), 0x40);
	}

	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
		b43_phy_set(dev, B43_LPPHY_CRSGAIN_CTL, 0x40);
		b43_phy_maskset(dev, B43_LPPHY_CRSGAIN_CTL, 0xF0FF, 0xB00);
		b43_phy_maskset(dev, B43_LPPHY_SYNCPEAKCNT, 0xFFF8, 0x6);
		b43_phy_maskset(dev, B43_LPPHY_MINPWR_LEVEL, 0x00FF, 0x9D00);
		b43_phy_maskset(dev, B43_LPPHY_MINPWR_LEVEL, 0xFF00, 0xA1);
	} else /* 5GHz */
		b43_phy_mask(dev, B43_LPPHY_CRSGAIN_CTL, ~0x40);

	b43_phy_maskset(dev, B43_LPPHY_CRS_ED_THRESH, 0xFF00, 0xB3);
	b43_phy_maskset(dev, B43_LPPHY_CRS_ED_THRESH, 0x00FF, 0xAD00);
	b43_phy_maskset(dev, B43_LPPHY_INPUT_PWRDB, 0xFF00, lpphy->rx_pwr_offset);
	b43_phy_set(dev, B43_LPPHY_RESET_CTL, 0x44);
	b43_phy_write(dev, B43_LPPHY_RESET_CTL, 0x80);
	b43_phy_write(dev, B43_LPPHY_AFE_RSSI_CTL_0, 0xA954);
	b43_phy_write(dev, B43_LPPHY_AFE_RSSI_CTL_1,
		      0x2000 | ((u16)lpphy->rssi_gs << 10) |
		      ((u16)lpphy->rssi_vc << 4) | lpphy->rssi_vf);

	if ((bus->chip_id == 0x4325) && (bus->chip_rev == 0)) {
		b43_phy_set(dev, B43_LPPHY_AFE_ADC_CTL_0, 0x1C);
		b43_phy_maskset(dev, B43_LPPHY_AFE_CTL, 0x00FF, 0x8800);
		b43_phy_maskset(dev, B43_LPPHY_AFE_ADC_CTL_1, 0xFC3C, 0x0400);
	}

	lpphy_save_dig_flt_state(dev);
}

static void lpphy_baseband_init(struct b43_wldev *dev)
{
	lpphy_table_init(dev);
	if (dev->phy.rev >= 2)
		lpphy_baseband_rev2plus_init(dev);
	else
		lpphy_baseband_rev0_1_init(dev);
}

struct b2062_freqdata {
	u16 freq;
	u8 data[6];
};

/* Initialize the 2062 radio. */
static void lpphy_2062_init(struct b43_wldev *dev)
{
	struct ssb_bus *bus = dev->dev->bus;
	u32 crystalfreq, pdiv, tmp, ref;
	unsigned int i;
	const struct b2062_freqdata *fd = NULL;

	static const struct b2062_freqdata freqdata_tab[] = {
		{ .freq = 12000, .data[0] =  6, .data[1] =  6, .data[2] =  6,
				 .data[3] =  6, .data[4] = 10, .data[5] =  6, },
		{ .freq = 13000, .data[0] =  4, .data[1] =  4, .data[2] =  4,
				 .data[3] =  4, .data[4] = 11, .data[5] =  7, },
		{ .freq = 14400, .data[0] =  3, .data[1] =  3, .data[2] =  3,
				 .data[3] =  3, .data[4] = 12, .data[5] =  7, },
		{ .freq = 16200, .data[0] =  3, .data[1] =  3, .data[2] =  3,
				 .data[3] =  3, .data[4] = 13, .data[5] =  8, },
		{ .freq = 18000, .data[0] =  2, .data[1] =  2, .data[2] =  2,
				 .data[3] =  2, .data[4] = 14, .data[5] =  8, },
		{ .freq = 19200, .data[0] =  1, .data[1] =  1, .data[2] =  1,
				 .data[3] =  1, .data[4] = 14, .data[5] =  9, },
	};

	b2062_upload_init_table(dev);

	b43_radio_write(dev, B2062_N_TX_CTL3, 0);
	b43_radio_write(dev, B2062_N_TX_CTL4, 0);
	b43_radio_write(dev, B2062_N_TX_CTL5, 0);
	b43_radio_write(dev, B2062_N_PDN_CTL0, 0x40);
	b43_radio_write(dev, B2062_N_PDN_CTL0, 0);
	b43_radio_write(dev, B2062_N_CALIB_TS, 0x10);
	b43_radio_write(dev, B2062_N_CALIB_TS, 0);
	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ)
		b43_radio_set(dev, B2062_N_TSSI_CTL0, 0x1);
	else
		b43_radio_mask(dev, B2062_N_TSSI_CTL0, ~0x1);

	/* Get the crystal freq, in Hz. */
	crystalfreq = bus->chipco.pmu.crystalfreq * 1000;

	B43_WARN_ON(!(bus->chipco.capabilities & SSB_CHIPCO_CAP_PMU));
	B43_WARN_ON(crystalfreq == 0);

	if (crystalfreq >= 30000000) {
		pdiv = 1;
		b43_radio_mask(dev, B2062_S_RFPLL_CTL1, 0xFFFB);
	} else {
		pdiv = 2;
		b43_radio_set(dev, B2062_S_RFPLL_CTL1, 0x4);
	}

	tmp = (800000000 * pdiv + crystalfreq) / (32000000 * pdiv);
	tmp = (tmp - 1) & 0xFF;
	b43_radio_write(dev, B2062_S_RFPLL_CTL18, tmp);

	tmp = (2 * crystalfreq + 1000000 * pdiv) / (2000000 * pdiv);
	tmp = ((tmp & 0xFF) - 1) & 0xFFFF;
	b43_radio_write(dev, B2062_S_RFPLL_CTL19, tmp);

	ref = (1000 * pdiv + 2 * crystalfreq) / (2000 * pdiv);
	ref &= 0xFFFF;
	for (i = 0; i < ARRAY_SIZE(freqdata_tab); i++) {
		if (ref < freqdata_tab[i].freq) {
			fd = &freqdata_tab[i];
			break;
		}
	}
	if (!fd)
		fd = &freqdata_tab[ARRAY_SIZE(freqdata_tab) - 1];
	b43dbg(dev->wl, "b2062: Using crystal tab entry %u kHz.\n",
	       fd->freq); /* FIXME: Keep this printk until the code is fully debugged. */

	b43_radio_write(dev, B2062_S_RFPLL_CTL8,
			((u16)(fd->data[1]) << 4) | fd->data[0]);
	b43_radio_write(dev, B2062_S_RFPLL_CTL9,
			((u16)(fd->data[3]) << 4) | fd->data[2]);
	b43_radio_write(dev, B2062_S_RFPLL_CTL10, fd->data[4]);
	b43_radio_write(dev, B2062_S_RFPLL_CTL11, fd->data[5]);
}

/* Initialize the 2063 radio. */
static void lpphy_2063_init(struct b43_wldev *dev)
{
	b2063_upload_init_table(dev);
	b43_radio_write(dev, B2063_LOGEN_SP5, 0);
	b43_radio_set(dev, B2063_COMM8, 0x38);
	b43_radio_write(dev, B2063_REG_SP1, 0x56);
	b43_radio_mask(dev, B2063_RX_BB_CTL2, ~0x2);
	b43_radio_write(dev, B2063_PA_SP7, 0);
	b43_radio_write(dev, B2063_TX_RF_SP6, 0x20);
	b43_radio_write(dev, B2063_TX_RF_SP9, 0x40);
	b43_radio_write(dev, B2063_PA_SP3, 0xa0);
	b43_radio_write(dev, B2063_PA_SP4, 0xa0);
	b43_radio_write(dev, B2063_PA_SP2, 0x18);
}

struct lpphy_stx_table_entry {
	u16 phy_offset;
	u16 phy_shift;
	u16 rf_addr;
	u16 rf_shift;
	u16 mask;
};

static const struct lpphy_stx_table_entry lpphy_stx_table[] = {
	{ .phy_offset = 2, .phy_shift = 6, .rf_addr = 0x3d, .rf_shift = 3, .mask = 0x01, },
	{ .phy_offset = 1, .phy_shift = 12, .rf_addr = 0x4c, .rf_shift = 1, .mask = 0x01, },
	{ .phy_offset = 1, .phy_shift = 8, .rf_addr = 0x50, .rf_shift = 0, .mask = 0x7f, },
	{ .phy_offset = 0, .phy_shift = 8, .rf_addr = 0x44, .rf_shift = 0, .mask = 0xff, },
	{ .phy_offset = 1, .phy_shift = 0, .rf_addr = 0x4a, .rf_shift = 0, .mask = 0xff, },
	{ .phy_offset = 0, .phy_shift = 4, .rf_addr = 0x4d, .rf_shift = 0, .mask = 0xff, },
	{ .phy_offset = 1, .phy_shift = 4, .rf_addr = 0x4e, .rf_shift = 0, .mask = 0xff, },
	{ .phy_offset = 0, .phy_shift = 12, .rf_addr = 0x4f, .rf_shift = 0, .mask = 0x0f, },
	{ .phy_offset = 1, .phy_shift = 0, .rf_addr = 0x4f, .rf_shift = 4, .mask = 0x0f, },
	{ .phy_offset = 3, .phy_shift = 0, .rf_addr = 0x49, .rf_shift = 0, .mask = 0x0f, },
	{ .phy_offset = 4, .phy_shift = 3, .rf_addr = 0x46, .rf_shift = 4, .mask = 0x07, },
	{ .phy_offset = 3, .phy_shift = 15, .rf_addr = 0x46, .rf_shift = 0, .mask = 0x01, },
	{ .phy_offset = 4, .phy_shift = 0, .rf_addr = 0x46, .rf_shift = 1, .mask = 0x07, },
	{ .phy_offset = 3, .phy_shift = 8, .rf_addr = 0x48, .rf_shift = 4, .mask = 0x07, },
	{ .phy_offset = 3, .phy_shift = 11, .rf_addr = 0x48, .rf_shift = 0, .mask = 0x0f, },
	{ .phy_offset = 3, .phy_shift = 4, .rf_addr = 0x49, .rf_shift = 4, .mask = 0x0f, },
	{ .phy_offset = 2, .phy_shift = 15, .rf_addr = 0x45, .rf_shift = 0, .mask = 0x01, },
	{ .phy_offset = 5, .phy_shift = 13, .rf_addr = 0x52, .rf_shift = 4, .mask = 0x07, },
	{ .phy_offset = 6, .phy_shift = 0, .rf_addr = 0x52, .rf_shift = 7, .mask = 0x01, },
	{ .phy_offset = 5, .phy_shift = 3, .rf_addr = 0x41, .rf_shift = 5, .mask = 0x07, },
	{ .phy_offset = 5, .phy_shift = 6, .rf_addr = 0x41, .rf_shift = 0, .mask = 0x0f, },
	{ .phy_offset = 5, .phy_shift = 10, .rf_addr = 0x42, .rf_shift = 5, .mask = 0x07, },
	{ .phy_offset = 4, .phy_shift = 15, .rf_addr = 0x42, .rf_shift = 0, .mask = 0x01, },
	{ .phy_offset = 5, .phy_shift = 0, .rf_addr = 0x42, .rf_shift = 1, .mask = 0x07, },
	{ .phy_offset = 4, .phy_shift = 11, .rf_addr = 0x43, .rf_shift = 4, .mask = 0x0f, },
	{ .phy_offset = 4, .phy_shift = 7, .rf_addr = 0x43, .rf_shift = 0, .mask = 0x0f, },
	{ .phy_offset = 4, .phy_shift = 6, .rf_addr = 0x45, .rf_shift = 1, .mask = 0x01, },
	{ .phy_offset = 2, .phy_shift = 7, .rf_addr = 0x40, .rf_shift = 4, .mask = 0x0f, },
	{ .phy_offset = 2, .phy_shift = 11, .rf_addr = 0x40, .rf_shift = 0, .mask = 0x0f, },
};

static void lpphy_sync_stx(struct b43_wldev *dev)
{
	const struct lpphy_stx_table_entry *e;
	unsigned int i;
	u16 tmp;

	for (i = 0; i < ARRAY_SIZE(lpphy_stx_table); i++) {
		e = &lpphy_stx_table[i];
		tmp = b43_radio_read(dev, e->rf_addr);
		tmp >>= e->rf_shift;
		tmp <<= e->phy_shift;
		b43_phy_maskset(dev, B43_PHY_OFDM(0xF2 + e->phy_offset),
				~(e->mask << e->phy_shift), tmp);
	}
}

static void lpphy_radio_init(struct b43_wldev *dev)
{
	/* The radio is attached through the 4wire bus. */
	b43_phy_set(dev, B43_LPPHY_FOURWIRE_CTL, 0x2);
	udelay(1);
	b43_phy_mask(dev, B43_LPPHY_FOURWIRE_CTL, 0xFFFD);
	udelay(1);

	if (dev->phy.rev < 2) {
		lpphy_2062_init(dev);
	} else {
		lpphy_2063_init(dev);
		lpphy_sync_stx(dev);
		b43_phy_write(dev, B43_PHY_OFDM(0xF0), 0x5F80);
		b43_phy_write(dev, B43_PHY_OFDM(0xF1), 0);
		if (dev->dev->bus->chip_id == 0x4325) {
			// TODO SSB PMU recalibration
		}
	}
}

struct lpphy_iq_est { u32 iq_prod, i_pwr, q_pwr; };

static void lpphy_set_rc_cap(struct b43_wldev *dev)
{
	u8 rc_cap = dev->phy.lp->rc_cap;

	b43_radio_write(dev, B2062_N_RXBB_CALIB2, max_t(u8, rc_cap-4, 0x80));
	b43_radio_write(dev, B2062_N_TX_CTL_A, ((rc_cap & 0x1F) >> 1) | 0x80);
	b43_radio_write(dev, B2062_S_RXG_CNT16, ((rc_cap & 0x1F) >> 2) | 0x80);
}

static u8 lpphy_get_bb_mult(struct b43_wldev *dev)
{
	return (b43_lptab_read(dev, B43_LPTAB16(0, 87)) & 0xFF00) >> 8;
}

static void lpphy_set_bb_mult(struct b43_wldev *dev, u8 bb_mult)
{
	b43_lptab_write(dev, B43_LPTAB16(0, 87), (u16)bb_mult << 8);
}

static void lpphy_disable_crs(struct b43_wldev *dev)
{
	b43_phy_maskset(dev, B43_LPPHY_CRSGAIN_CTL, 0xFF1F, 0x80);
	b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_VAL_0, 0xFFFC, 0x1);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_0, 0x3);
	b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_VAL_0, 0xFFFB);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_0, 0x4);
	b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_0, 0xFFF7);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_0, 0x8);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_VAL_0, 0x10);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_0, 0x10);
	b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_VAL_0, 0xFFDF);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_0, 0x20);
	b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_VAL_0, 0xFFBF);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_0, 0x40);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_2_VAL, 0x7);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_2_VAL, 0x38);
	b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_2_VAL, 0xFF3F);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_2_VAL, 0x100);
	b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_2_VAL, 0xFDFF);
	b43_phy_write(dev, B43_LPPHY_PS_CTL_OVERRIDE_VAL0, 0);
	b43_phy_write(dev, B43_LPPHY_PS_CTL_OVERRIDE_VAL1, 1);
	b43_phy_write(dev, B43_LPPHY_PS_CTL_OVERRIDE_VAL2, 0x20);
	b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_2_VAL, 0xFBFF);
	b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_2_VAL, 0xF7FF);
	b43_phy_write(dev, B43_LPPHY_TX_GAIN_CTL_OVERRIDE_VAL, 0);
	b43_phy_write(dev, B43_LPPHY_RX_GAIN_CTL_OVERRIDE_VAL, 0x45AF);
	b43_phy_write(dev, B43_LPPHY_RF_OVERRIDE_2, 0x3FF);
}

static void lpphy_restore_crs(struct b43_wldev *dev)
{
	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ)
		b43_phy_maskset(dev, B43_LPPHY_CRSGAIN_CTL, 0xFF1F, 0x60);
	else
		b43_phy_maskset(dev, B43_LPPHY_CRSGAIN_CTL, 0xFF1F, 0x20);
	b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_0, 0xFF80);
	b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_2, 0xFC00);
}

struct lpphy_tx_gains { u16 gm, pga, pad, dac; };

static struct lpphy_tx_gains lpphy_get_tx_gains(struct b43_wldev *dev)
{
	struct lpphy_tx_gains gains;
	u16 tmp;

	gains.dac = (b43_phy_read(dev, B43_LPPHY_AFE_DAC_CTL) & 0x380) >> 7;
	if (dev->phy.rev < 2) {
		tmp = b43_phy_read(dev,
				   B43_LPPHY_TX_GAIN_CTL_OVERRIDE_VAL) & 0x7FF;
		gains.gm = tmp & 0x0007;
		gains.pga = (tmp & 0x0078) >> 3;
		gains.pad = (tmp & 0x780) >> 7;
	} else {
		tmp = b43_phy_read(dev, B43_LPPHY_TX_GAIN_CTL_OVERRIDE_VAL);
		gains.pad = b43_phy_read(dev, B43_PHY_OFDM(0xFB)) & 0xFF;
		gains.gm = tmp & 0xFF;
		gains.pga = (tmp >> 8) & 0xFF;
	}

	return gains;
}

static void lpphy_set_dac_gain(struct b43_wldev *dev, u16 dac)
{
	u16 ctl = b43_phy_read(dev, B43_LPPHY_AFE_DAC_CTL) & 0xC7F;
	ctl |= dac << 7;
	b43_phy_maskset(dev, B43_LPPHY_AFE_DAC_CTL, 0xF000, ctl);
}

static void lpphy_set_tx_gains(struct b43_wldev *dev,
			       struct lpphy_tx_gains gains)
{
	u16 rf_gain, pa_gain;

	if (dev->phy.rev < 2) {
		rf_gain = (gains.pad << 7) | (gains.pga << 3) | gains.gm;
		b43_phy_maskset(dev, B43_LPPHY_TX_GAIN_CTL_OVERRIDE_VAL,
				0xF800, rf_gain);
	} else {
		pa_gain = b43_phy_read(dev, B43_PHY_OFDM(0xFB)) & 0x7F00;
		b43_phy_write(dev, B43_LPPHY_TX_GAIN_CTL_OVERRIDE_VAL,
			      (gains.pga << 8) | gains.gm);
		b43_phy_maskset(dev, B43_LPPHY_TX_GAIN_CTL_OVERRIDE_VAL,
				0x8000, gains.pad | pa_gain);
		b43_phy_write(dev, B43_PHY_OFDM(0xFC),
			      (gains.pga << 8) | gains.gm);
		b43_phy_maskset(dev, B43_PHY_OFDM(0xFD),
				0x8000, gains.pad | pa_gain);
	}
	lpphy_set_dac_gain(dev, gains.dac);
	if (dev->phy.rev < 2) {
		b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_2, 0xFEFF, 1 << 8);
	} else {
		b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_2, 0xFF7F, 1 << 7);
		b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_2, 0xBFFF, 1 << 14);
	}
	b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_2, 0xFFBF, 1 << 4);
}

static void lpphy_rev0_1_set_rx_gain(struct b43_wldev *dev, u32 gain)
{
	u16 trsw = gain & 0x1;
	u16 lna = (gain & 0xFFFC) | ((gain & 0xC) >> 2);
	u16 ext_lna = (gain & 2) >> 1;

	b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_VAL_0, 0xFFFE, trsw);
	b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_2_VAL,
			0xFBFF, ext_lna << 10);
	b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_2_VAL,
			0xF7FF, ext_lna << 11);
	b43_phy_write(dev, B43_LPPHY_RX_GAIN_CTL_OVERRIDE_VAL, lna);
}

static void lpphy_rev2plus_set_rx_gain(struct b43_wldev *dev, u32 gain)
{
	u16 low_gain = gain & 0xFFFF;
	u16 high_gain = (gain >> 16) & 0xF;
	u16 ext_lna = (gain >> 21) & 0x1;
	u16 trsw = ~(gain >> 20) & 0x1;
	u16 tmp;

	b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_VAL_0, 0xFFFE, trsw);
	b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_2_VAL,
			0xFDFF, ext_lna << 9);
	b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_2_VAL,
			0xFBFF, ext_lna << 10);
	b43_phy_write(dev, B43_LPPHY_RX_GAIN_CTL_OVERRIDE_VAL, low_gain);
	b43_phy_maskset(dev, B43_LPPHY_AFE_DDFS, 0xFFF0, high_gain);
	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
		tmp = (gain >> 2) & 0x3;
		b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_2_VAL,
				0xE7FF, tmp<<11);
		b43_phy_maskset(dev, B43_PHY_OFDM(0xE6), 0xFFE7, tmp << 3);
	}
}

static void lpphy_enable_rx_gain_override(struct b43_wldev *dev)
{
	b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_0, 0xFFFE);
	b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_0, 0xFFEF);
	b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_0, 0xFFBF);
	if (dev->phy.rev >= 2) {
		b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_2, 0xFEFF);
		if (b43_current_band(dev->wl) != IEEE80211_BAND_2GHZ)
			return;
		b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_2, 0xFBFF);
		b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_2, 0xFFF7);
	} else {
		b43_phy_mask(dev, B43_LPPHY_RF_OVERRIDE_2, 0xFDFF);
	}
}

static void lpphy_disable_rx_gain_override(struct b43_wldev *dev)
{
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_0, 0x1);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_0, 0x10);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_0, 0x40);
	if (dev->phy.rev >= 2) {
		b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_2, 0x100);
		if (b43_current_band(dev->wl) != IEEE80211_BAND_2GHZ)
			return;
		b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_2, 0x400);
		b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_2, 0x8);
	} else {
		b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_2, 0x200);
	}
}

static void lpphy_set_rx_gain(struct b43_wldev *dev, u32 gain)
{
	if (dev->phy.rev < 2)
		lpphy_rev0_1_set_rx_gain(dev, gain);
	else
		lpphy_rev2plus_set_rx_gain(dev, gain);
	lpphy_enable_rx_gain_override(dev);
}

static void lpphy_set_rx_gain_by_index(struct b43_wldev *dev, u16 idx)
{
	u32 gain = b43_lptab_read(dev, B43_LPTAB16(12, idx));
	lpphy_set_rx_gain(dev, gain);
}

static void lpphy_stop_ddfs(struct b43_wldev *dev)
{
	b43_phy_mask(dev, B43_LPPHY_AFE_DDFS, 0xFFFD);
	b43_phy_mask(dev, B43_LPPHY_LP_PHY_CTL, 0xFFDF);
}

static void lpphy_run_ddfs(struct b43_wldev *dev, int i_on, int q_on,
			   int incr1, int incr2, int scale_idx)
{
	lpphy_stop_ddfs(dev);
	b43_phy_mask(dev, B43_LPPHY_AFE_DDFS_POINTER_INIT, 0xFF80);
	b43_phy_mask(dev, B43_LPPHY_AFE_DDFS_POINTER_INIT, 0x80FF);
	b43_phy_maskset(dev, B43_LPPHY_AFE_DDFS_INCR_INIT, 0xFF80, incr1);
	b43_phy_maskset(dev, B43_LPPHY_AFE_DDFS_INCR_INIT, 0x80FF, incr2 << 8);
	b43_phy_maskset(dev, B43_LPPHY_AFE_DDFS, 0xFFF7, i_on << 3);
	b43_phy_maskset(dev, B43_LPPHY_AFE_DDFS, 0xFFEF, q_on << 4);
	b43_phy_maskset(dev, B43_LPPHY_AFE_DDFS, 0xFF9F, scale_idx << 5);
	b43_phy_mask(dev, B43_LPPHY_AFE_DDFS, 0xFFFB);
	b43_phy_set(dev, B43_LPPHY_AFE_DDFS, 0x2);
	b43_phy_set(dev, B43_LPPHY_AFE_DDFS, 0x20);
}

static bool lpphy_rx_iq_est(struct b43_wldev *dev, u16 samples, u8 time,
			   struct lpphy_iq_est *iq_est)
{
	int i;

	b43_phy_mask(dev, B43_LPPHY_CRSGAIN_CTL, 0xFFF7);
	b43_phy_write(dev, B43_LPPHY_IQ_NUM_SMPLS_ADDR, samples);
	b43_phy_maskset(dev, B43_LPPHY_IQ_ENABLE_WAIT_TIME_ADDR, 0xFF00, time);
	b43_phy_mask(dev, B43_LPPHY_IQ_ENABLE_WAIT_TIME_ADDR, 0xFEFF);
	b43_phy_set(dev, B43_LPPHY_IQ_ENABLE_WAIT_TIME_ADDR, 0xFDFF);

	for (i = 0; i < 500; i++) {
		if (!(b43_phy_read(dev,
				B43_LPPHY_IQ_ENABLE_WAIT_TIME_ADDR) & 0x200))
			break;
		msleep(1);
	}

	if ((b43_phy_read(dev, B43_LPPHY_IQ_ENABLE_WAIT_TIME_ADDR) & 0x200)) {
		b43_phy_set(dev, B43_LPPHY_CRSGAIN_CTL, 0x8);
		return false;
	}

	iq_est->iq_prod = b43_phy_read(dev, B43_LPPHY_IQ_ACC_HI_ADDR);
	iq_est->iq_prod <<= 16;
	iq_est->iq_prod |= b43_phy_read(dev, B43_LPPHY_IQ_ACC_LO_ADDR);

	iq_est->i_pwr = b43_phy_read(dev, B43_LPPHY_IQ_I_PWR_ACC_HI_ADDR);
	iq_est->i_pwr <<= 16;
	iq_est->i_pwr |= b43_phy_read(dev, B43_LPPHY_IQ_I_PWR_ACC_LO_ADDR);

	iq_est->q_pwr = b43_phy_read(dev, B43_LPPHY_IQ_Q_PWR_ACC_HI_ADDR);
	iq_est->q_pwr <<= 16;
	iq_est->q_pwr |= b43_phy_read(dev, B43_LPPHY_IQ_Q_PWR_ACC_LO_ADDR);

	b43_phy_set(dev, B43_LPPHY_CRSGAIN_CTL, 0x8);
	return true;
}

static int lpphy_loopback(struct b43_wldev *dev)
{
	struct lpphy_iq_est iq_est;
	int i, index = -1;
	u32 tmp;

	memset(&iq_est, 0, sizeof(iq_est));

	b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_VAL_0, 0xFFFC, 0x3);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_0, 0x3);
	b43_phy_mask(dev, B43_LPPHY_AFE_CTL_OVRVAL, 0xFFFE);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_0, 0x800);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_VAL_0, 0x800);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_0, 0x8);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_VAL_0, 0x8);
	b43_radio_write(dev, B2062_N_TX_CTL_A, 0x80);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_0, 0x80);
	b43_phy_set(dev, B43_LPPHY_RF_OVERRIDE_VAL_0, 0x80);
	for (i = 0; i < 32; i++) {
		lpphy_set_rx_gain_by_index(dev, i);
		lpphy_run_ddfs(dev, 1, 1, 5, 5, 0);
		if (!(lpphy_rx_iq_est(dev, 1000, 32, &iq_est)))
			continue;
		tmp = (iq_est.i_pwr + iq_est.q_pwr) / 1000;
		if ((tmp > 4000) && (tmp < 10000)) {
			index = i;
			break;
		}
	}
	lpphy_stop_ddfs(dev);
	return index;
}

static u32 lpphy_qdiv_roundup(u32 dividend, u32 divisor, u8 precision)
{
	u32 quotient, remainder, rbit, roundup, tmp;

	if (divisor == 0) {
		quotient = 0;
		remainder = 0;
	} else {
		quotient = dividend / divisor;
		remainder = dividend % divisor;
	}

	rbit = divisor & 0x1;
	roundup = (divisor >> 1) + rbit;
	precision--;

	while (precision != 0xFF) {
		tmp = remainder - roundup;
		quotient <<= 1;
		remainder <<= 1;
		if (remainder >= roundup) {
			remainder = (tmp << 1) + rbit;
			quotient--;
		}
		precision--;
	}

	if (remainder >= roundup)
		quotient++;

	return quotient;
}

/* Read the TX power control mode from hardware. */
static void lpphy_read_tx_pctl_mode_from_hardware(struct b43_wldev *dev)
{
	struct b43_phy_lp *lpphy = dev->phy.lp;
	u16 ctl;

	ctl = b43_phy_read(dev, B43_LPPHY_TX_PWR_CTL_CMD);
	switch (ctl & B43_LPPHY_TX_PWR_CTL_CMD_MODE) {
	case B43_LPPHY_TX_PWR_CTL_CMD_MODE_OFF:
		lpphy->txpctl_mode = B43_LPPHY_TXPCTL_OFF;
		break;
	case B43_LPPHY_TX_PWR_CTL_CMD_MODE_SW:
		lpphy->txpctl_mode = B43_LPPHY_TXPCTL_SW;
		break;
	case B43_LPPHY_TX_PWR_CTL_CMD_MODE_HW:
		lpphy->txpctl_mode = B43_LPPHY_TXPCTL_HW;
		break;
	default:
		lpphy->txpctl_mode = B43_LPPHY_TXPCTL_UNKNOWN;
		B43_WARN_ON(1);
		break;
	}
}

/* Set the TX power control mode in hardware. */
static void lpphy_write_tx_pctl_mode_to_hardware(struct b43_wldev *dev)
{
	struct b43_phy_lp *lpphy = dev->phy.lp;
	u16 ctl;

	switch (lpphy->txpctl_mode) {
	case B43_LPPHY_TXPCTL_OFF:
		ctl = B43_LPPHY_TX_PWR_CTL_CMD_MODE_OFF;
		break;
	case B43_LPPHY_TXPCTL_HW:
		ctl = B43_LPPHY_TX_PWR_CTL_CMD_MODE_HW;
		break;
	case B43_LPPHY_TXPCTL_SW:
		ctl = B43_LPPHY_TX_PWR_CTL_CMD_MODE_SW;
		break;
	default:
		ctl = 0;
		B43_WARN_ON(1);
	}
	b43_phy_maskset(dev, B43_LPPHY_TX_PWR_CTL_CMD,
			(u16)~B43_LPPHY_TX_PWR_CTL_CMD_MODE, ctl);
}

static void lpphy_set_tx_power_control(struct b43_wldev *dev,
				       enum b43_lpphy_txpctl_mode mode)
{
	struct b43_phy_lp *lpphy = dev->phy.lp;
	enum b43_lpphy_txpctl_mode oldmode;

	oldmode = lpphy->txpctl_mode;
	lpphy_read_tx_pctl_mode_from_hardware(dev);
	if (lpphy->txpctl_mode == mode)
		return;
	lpphy->txpctl_mode = mode;

	if (oldmode == B43_LPPHY_TXPCTL_HW) {
		//TODO Update TX Power NPT
		//TODO Clear all TX Power offsets
	} else {
		if (mode == B43_LPPHY_TXPCTL_HW) {
			//TODO Recalculate target TX power
			b43_phy_maskset(dev, B43_LPPHY_TX_PWR_CTL_CMD,
					0xFF80, lpphy->tssi_idx);
			b43_phy_maskset(dev, B43_LPPHY_TX_PWR_CTL_NNUM,
					0x8FFF, ((u16)lpphy->tssi_npt << 16));
			//TODO Set "TSSI Transmit Count" variable to total transmitted frame count
			//TODO Disable TX gain override
			lpphy->tx_pwr_idx_over = -1;
		}
	}
	if (dev->phy.rev >= 2) {
		if (mode == B43_LPPHY_TXPCTL_HW)
			b43_phy_maskset(dev, B43_PHY_OFDM(0xD0), 0xFD, 0x2);
		else
			b43_phy_maskset(dev, B43_PHY_OFDM(0xD0), 0xFD, 0);
	}
	lpphy_write_tx_pctl_mode_to_hardware(dev);
}

static void lpphy_rev0_1_rc_calib(struct b43_wldev *dev)
{
	struct b43_phy_lp *lpphy = dev->phy.lp;
	struct lpphy_iq_est iq_est;
	struct lpphy_tx_gains tx_gains;
	static const u32 ideal_pwr_table[22] = {
		0x10000, 0x10557, 0x10e2d, 0x113e0, 0x10f22, 0x0ff64,
		0x0eda2, 0x0e5d4, 0x0efd1, 0x0fbe8, 0x0b7b8, 0x04b35,
		0x01a5e, 0x00a0b, 0x00444, 0x001fd, 0x000ff, 0x00088,
		0x0004c, 0x0002c, 0x0001a, 0xc0006,
	};
	bool old_txg_ovr;
	u8 old_bbmult;
	u16 old_rf_ovr, old_rf_ovrval, old_afe_ovr, old_afe_ovrval,
	    old_rf2_ovr, old_rf2_ovrval, old_phy_ctl, old_txpctl;
	u32 normal_pwr, ideal_pwr, mean_sq_pwr, tmp = 0, mean_sq_pwr_min = 0;
	int loopback, i, j, inner_sum;

	memset(&iq_est, 0, sizeof(iq_est));

	b43_switch_channel(dev, 7);
	old_txg_ovr = (b43_phy_read(dev, B43_LPPHY_AFE_CTL_OVR) >> 6) & 1;
	old_bbmult = lpphy_get_bb_mult(dev);
	if (old_txg_ovr)
		tx_gains = lpphy_get_tx_gains(dev);
	old_rf_ovr = b43_phy_read(dev, B43_LPPHY_RF_OVERRIDE_0);
	old_rf_ovrval = b43_phy_read(dev, B43_LPPHY_RF_OVERRIDE_VAL_0);
	old_afe_ovr = b43_phy_read(dev, B43_LPPHY_AFE_CTL_OVR);
	old_afe_ovrval = b43_phy_read(dev, B43_LPPHY_AFE_CTL_OVRVAL);
	old_rf2_ovr = b43_phy_read(dev, B43_LPPHY_RF_OVERRIDE_2);
	old_rf2_ovrval = b43_phy_read(dev, B43_LPPHY_RF_OVERRIDE_2_VAL);
	old_phy_ctl = b43_phy_read(dev, B43_LPPHY_LP_PHY_CTL);
	old_txpctl = b43_phy_read(dev, B43_LPPHY_TX_PWR_CTL_CMD) &
					B43_LPPHY_TX_PWR_CTL_CMD_MODE;

	lpphy_set_tx_power_control(dev, B43_LPPHY_TX_PWR_CTL_CMD_MODE_OFF);
	lpphy_disable_crs(dev);
	loopback = lpphy_loopback(dev);
	if (loopback == -1)
		goto finish;
	lpphy_set_rx_gain_by_index(dev, loopback);
	b43_phy_maskset(dev, B43_LPPHY_LP_PHY_CTL, 0xFFBF, 0x40);
	b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_2_VAL, 0xFFF8, 0x1);
	b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_2_VAL, 0xFFC7, 0x8);
	b43_phy_maskset(dev, B43_LPPHY_RF_OVERRIDE_2_VAL, 0xFF3F, 0xC0);
	for (i = 128; i <= 159; i++) {
		b43_radio_write(dev, B2062_N_RXBB_CALIB2, i);
		inner_sum = 0;
		for (j = 5; j <= 25; j++) {
			lpphy_run_ddfs(dev, 1, 1, j, j, 0);
			if (!(lpphy_rx_iq_est(dev, 1000, 32, &iq_est)))
				goto finish;
			mean_sq_pwr = iq_est.i_pwr + iq_est.q_pwr;
			if (j == 5)
				tmp = mean_sq_pwr;
			ideal_pwr = ((ideal_pwr_table[j-5] >> 3) + 1) >> 1;
			normal_pwr = lpphy_qdiv_roundup(mean_sq_pwr, tmp, 12);
			mean_sq_pwr = ideal_pwr - normal_pwr;
			mean_sq_pwr *= mean_sq_pwr;
			inner_sum += mean_sq_pwr;
			if ((i = 128) || (inner_sum < mean_sq_pwr_min)) {
				lpphy->rc_cap = i;
				mean_sq_pwr_min = inner_sum;
			}
		}
	}
	lpphy_stop_ddfs(dev);

finish:
	lpphy_restore_crs(dev);
	b43_phy_write(dev, B43_LPPHY_RF_OVERRIDE_VAL_0, old_rf_ovrval);
	b43_phy_write(dev, B43_LPPHY_RF_OVERRIDE_0, old_rf_ovr);
	b43_phy_write(dev, B43_LPPHY_AFE_CTL_OVRVAL, old_afe_ovrval);
	b43_phy_write(dev, B43_LPPHY_AFE_CTL_OVR, old_afe_ovr);
	b43_phy_write(dev, B43_LPPHY_RF_OVERRIDE_2_VAL, old_rf2_ovrval);
	b43_phy_write(dev, B43_LPPHY_RF_OVERRIDE_2, old_rf2_ovr);
	b43_phy_write(dev, B43_LPPHY_LP_PHY_CTL, old_phy_ctl);

	lpphy_set_bb_mult(dev, old_bbmult);
	if (old_txg_ovr) {
		/*
		 * SPEC FIXME: The specs say "get_tx_gains" here, which is
		 * illogical. According to lwfinger, vendor driver v4.150.10.5
		 * has a Set here, while v4.174.64.19 has a Get - regression in
		 * the vendor driver? This should be tested this once the code
		 * is testable.
		 */
		lpphy_set_tx_gains(dev, tx_gains);
	}
	lpphy_set_tx_power_control(dev, old_txpctl);
	if (lpphy->rc_cap)
		lpphy_set_rc_cap(dev);
}

static void lpphy_rev2plus_rc_calib(struct b43_wldev *dev)
{
	struct ssb_bus *bus = dev->dev->bus;
	u32 crystal_freq = bus->chipco.pmu.crystalfreq * 1000;
	u8 tmp = b43_radio_read(dev, B2063_RX_BB_SP8) & 0xFF;
	int i;

	b43_radio_write(dev, B2063_RX_BB_SP8, 0x0);
	b43_radio_write(dev, B2063_RC_CALIB_CTL1, 0x7E);
	b43_radio_mask(dev, B2063_PLL_SP1, 0xF7);
	b43_radio_write(dev, B2063_RC_CALIB_CTL1, 0x7C);
	b43_radio_write(dev, B2063_RC_CALIB_CTL2, 0x15);
	b43_radio_write(dev, B2063_RC_CALIB_CTL3, 0x70);
	b43_radio_write(dev, B2063_RC_CALIB_CTL4, 0x52);
	b43_radio_write(dev, B2063_RC_CALIB_CTL5, 0x1);
	b43_radio_write(dev, B2063_RC_CALIB_CTL1, 0x7D);

	for (i = 0; i < 10000; i++) {
		if (b43_radio_read(dev, B2063_RC_CALIB_CTL6) & 0x2)
			break;
		msleep(1);
	}

	if (!(b43_radio_read(dev, B2063_RC_CALIB_CTL6) & 0x2))
		b43_radio_write(dev, B2063_RX_BB_SP8, tmp);

	tmp = b43_radio_read(dev, B2063_TX_BB_SP3) & 0xFF;

	b43_radio_write(dev, B2063_TX_BB_SP3, 0x0);
	b43_radio_write(dev, B2063_RC_CALIB_CTL1, 0x7E);
	b43_radio_write(dev, B2063_RC_CALIB_CTL1, 0x7C);
	b43_radio_write(dev, B2063_RC_CALIB_CTL2, 0x55);
	b43_radio_write(dev, B2063_RC_CALIB_CTL3, 0x76);

	if (crystal_freq == 24000000) {
		b43_radio_write(dev, B2063_RC_CALIB_CTL4, 0xFC);
		b43_radio_write(dev, B2063_RC_CALIB_CTL5, 0x0);
	} else {
		b43_radio_write(dev, B2063_RC_CALIB_CTL4, 0x13);
		b43_radio_write(dev, B2063_RC_CALIB_CTL5, 0x1);
	}

	b43_radio_write(dev, B2063_PA_SP7, 0x7D);

	for (i = 0; i < 10000; i++) {
		if (b43_radio_read(dev, B2063_RC_CALIB_CTL6) & 0x2)
			break;
		msleep(1);
	}

	if (!(b43_radio_read(dev, B2063_RC_CALIB_CTL6) & 0x2))
		b43_radio_write(dev, B2063_TX_BB_SP3, tmp);

	b43_radio_write(dev, B2063_RC_CALIB_CTL1, 0x7E);
}

static void lpphy_calibrate_rc(struct b43_wldev *dev)
{
	struct b43_phy_lp *lpphy = dev->phy.lp;

	if (dev->phy.rev >= 2) {
		lpphy_rev2plus_rc_calib(dev);
	} else if (!lpphy->rc_cap) {
		if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ)
			lpphy_rev0_1_rc_calib(dev);
	} else {
		lpphy_set_rc_cap(dev);
	}
}

static void lpphy_set_tx_power_by_index(struct b43_wldev *dev, u8 index)
{
	struct b43_phy_lp *lpphy = dev->phy.lp;

	lpphy->tx_pwr_idx_over = index;
	if (lpphy->txpctl_mode != B43_LPPHY_TXPCTL_OFF)
		lpphy_set_tx_power_control(dev, B43_LPPHY_TXPCTL_SW);

	//TODO
}

static void lpphy_btcoex_override(struct b43_wldev *dev)
{
	b43_write16(dev, B43_MMIO_BTCOEX_CTL, 0x3);
	b43_write16(dev, B43_MMIO_BTCOEX_TXCTL, 0xFF);
}

static void lpphy_pr41573_workaround(struct b43_wldev *dev)
{
	struct b43_phy_lp *lpphy = dev->phy.lp;
	u32 *saved_tab;
	const unsigned int saved_tab_size = 256;
	enum b43_lpphy_txpctl_mode txpctl_mode;
	s8 tx_pwr_idx_over;
	u16 tssi_npt, tssi_idx;

	saved_tab = kcalloc(saved_tab_size, sizeof(saved_tab[0]), GFP_KERNEL);
	if (!saved_tab) {
		b43err(dev->wl, "PR41573 failed. Out of memory!\n");
		return;
	}

	lpphy_read_tx_pctl_mode_from_hardware(dev);
	txpctl_mode = lpphy->txpctl_mode;
	tx_pwr_idx_over = lpphy->tx_pwr_idx_over;
	tssi_npt = lpphy->tssi_npt;
	tssi_idx = lpphy->tssi_idx;

	if (dev->phy.rev < 2) {
		b43_lptab_read_bulk(dev, B43_LPTAB32(10, 0x140),
				    saved_tab_size, saved_tab);
	} else {
		b43_lptab_read_bulk(dev, B43_LPTAB32(7, 0x140),
				    saved_tab_size, saved_tab);
	}
	//TODO

	kfree(saved_tab);
}

static void lpphy_calibration(struct b43_wldev *dev)
{
	struct b43_phy_lp *lpphy = dev->phy.lp;
	enum b43_lpphy_txpctl_mode saved_pctl_mode;

	b43_mac_suspend(dev);

	lpphy_btcoex_override(dev);
	lpphy_read_tx_pctl_mode_from_hardware(dev);
	saved_pctl_mode = lpphy->txpctl_mode;
	lpphy_set_tx_power_control(dev, B43_LPPHY_TXPCTL_OFF);
	//TODO Perform transmit power table I/Q LO calibration
	if ((dev->phy.rev == 0) && (saved_pctl_mode != B43_LPPHY_TXPCTL_OFF))
		lpphy_pr41573_workaround(dev);
	//TODO If a full calibration has not been performed on this channel yet, perform PAPD TX-power calibration
	lpphy_set_tx_power_control(dev, saved_pctl_mode);
	//TODO Perform I/Q calibration with a single control value set

	b43_mac_enable(dev);
}

/* Initialize TX power control */
static void lpphy_tx_pctl_init(struct b43_wldev *dev)
{
	if (0/*FIXME HWPCTL capable */) {
		//TODO
	} else { /* This device is only software TX power control capable. */
		if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
			//TODO
		} else {
			//TODO
		}
		//TODO set BB multiplier to 0x0096
	}
}

static int b43_lpphy_op_init(struct b43_wldev *dev)
{
	lpphy_read_band_sprom(dev); //FIXME should this be in prepare_structs?
	lpphy_baseband_init(dev);
	lpphy_radio_init(dev);
	lpphy_calibrate_rc(dev);
	//TODO set channel
	lpphy_tx_pctl_init(dev);
	lpphy_calibration(dev);
	//TODO ACI init

	return 0;
}

static u16 b43_lpphy_op_read(struct b43_wldev *dev, u16 reg)
{
	b43_write16(dev, B43_MMIO_PHY_CONTROL, reg);
	return b43_read16(dev, B43_MMIO_PHY_DATA);
}

static void b43_lpphy_op_write(struct b43_wldev *dev, u16 reg, u16 value)
{
	b43_write16(dev, B43_MMIO_PHY_CONTROL, reg);
	b43_write16(dev, B43_MMIO_PHY_DATA, value);
}

static u16 b43_lpphy_op_radio_read(struct b43_wldev *dev, u16 reg)
{
	/* Register 1 is a 32-bit register. */
	B43_WARN_ON(reg == 1);
	/* LP-PHY needs a special bit set for read access */
	if (dev->phy.rev < 2) {
		if (reg != 0x4001)
			reg |= 0x100;
	} else
		reg |= 0x200;

	b43_write16(dev, B43_MMIO_RADIO_CONTROL, reg);
	return b43_read16(dev, B43_MMIO_RADIO_DATA_LOW);
}

static void b43_lpphy_op_radio_write(struct b43_wldev *dev, u16 reg, u16 value)
{
	/* Register 1 is a 32-bit register. */
	B43_WARN_ON(reg == 1);

	b43_write16(dev, B43_MMIO_RADIO_CONTROL, reg);
	b43_write16(dev, B43_MMIO_RADIO_DATA_LOW, value);
}

static void b43_lpphy_op_software_rfkill(struct b43_wldev *dev,
					 bool blocked)
{
	//TODO
}

static int b43_lpphy_op_switch_channel(struct b43_wldev *dev,
				       unsigned int new_channel)
{
	//TODO
	return 0;
}

static unsigned int b43_lpphy_op_get_default_chan(struct b43_wldev *dev)
{
	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ)
		return 1;
	return 36;
}

static void b43_lpphy_op_set_rx_antenna(struct b43_wldev *dev, int antenna)
{
	//TODO
}

static void b43_lpphy_op_adjust_txpower(struct b43_wldev *dev)
{
	//TODO
}

static enum b43_txpwr_result b43_lpphy_op_recalc_txpower(struct b43_wldev *dev,
							 bool ignore_tssi)
{
	//TODO
	return B43_TXPWR_RES_DONE;
}

const struct b43_phy_operations b43_phyops_lp = {
	.allocate		= b43_lpphy_op_allocate,
	.free			= b43_lpphy_op_free,
	.prepare_structs	= b43_lpphy_op_prepare_structs,
	.init			= b43_lpphy_op_init,
	.phy_read		= b43_lpphy_op_read,
	.phy_write		= b43_lpphy_op_write,
	.radio_read		= b43_lpphy_op_radio_read,
	.radio_write		= b43_lpphy_op_radio_write,
	.software_rfkill	= b43_lpphy_op_software_rfkill,
	.switch_analog		= b43_phyop_switch_analog_generic,
	.switch_channel		= b43_lpphy_op_switch_channel,
	.get_default_chan	= b43_lpphy_op_get_default_chan,
	.set_rx_antenna		= b43_lpphy_op_set_rx_antenna,
	.recalc_txpower		= b43_lpphy_op_recalc_txpower,
	.adjust_txpower		= b43_lpphy_op_adjust_txpower,
};
