/* Bootstrapping GSM - taken from bsc_hack.c */

/* (C) 2008-2009 by Harald Welte <laforge@gnumonks.org>
 * (C) 2009 by Holger Hans Peter Freyther <zecke@selfish.org>
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <getopt.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <openbsc/openbsc.h>
#include <openbsc/gsm_data.h>
#include <openbsc/gsm_04_08.h>
#include <openbsc/db.h>
#include <openbsc/timer.h>
#include <openbsc/select.h>
#include <openbsc/abis_rsl.h>
#include <openbsc/abis_nm.h>
#include <openbsc/debug.h>
#include <openbsc/misdn.h>
#include <openbsc/telnet_interface.h>
#include <openbsc/paging.h>
#include <openbsc/e1_input.h>

/* The following definitions are for OM and NM packets that we cannot yet
 * generate by code but we just pass on */

// BTS Site Manager, SET ATTRIBUTES

/*
  Object Class: BTS Site Manager
  Instance 1: FF
  Instance 2: FF
  Instance 3: FF
SET ATTRIBUTES
  sAbisExternalTime: 2007/09/08   14:36:11
  omLAPDRelTimer: 30sec
  shortLAPDIntTimer: 5sec
  emergencyTimer1: 10 minutes
  emergencyTimer2: 0 minutes
*/

unsigned char msg_1[] = 
{
	0xD0, 0x00, 0xFF, 0xFF, 0xFF, 
		NM_ATT_BS11_ABIS_EXT_TIME, 0x07, 0xD7, 0x09, 0x08, 0x0E, 0x24, 0x0B, 0xCE, 
		0x02, 0x00, 0x1E, 
		0xE8, 0x01, 0x05,
		0x42, 0x02, 0x00, 0x0A, 
		0x44, 0x02, 0x00, 0x00
};

// BTS, SET BTS ATTRIBUTES

/*
  Object Class: BTS
  BTS relat. Number: 0 
  Instance 2: FF
  Instance 3: FF
SET BTS ATTRIBUTES
  bsIdentityCode / BSIC:
    PLMN_colour_code: 7h
    BS_colour_code:   7h
  BTS Air Timer T3105: 4  ,unit 10 ms
  btsIsHopping: FALSE
  periodCCCHLoadIndication: 1sec
  thresholdCCCHLoadIndication: 0%
  cellAllocationNumber: 00h = GSM 900
  enableInterferenceClass: 00h =  Disabled
  fACCHQual: 6 (FACCH stealing flags minus 1)
  intaveParameter: 31 SACCH multiframes
  interferenceLevelBoundaries:
    Interference Boundary 1: 0Ah 
    Interference Boundary 2: 0Fh
    Interference Boundary 3: 14h
    Interference Boundary 4: 19h
    Interference Boundary 5: 1Eh
  mSTxPwrMax: 11
      GSM range:     2=39dBm, 15=13dBm, stepsize 2 dBm 
      DCS1800 range: 0=30dBm, 15=0dBm, stepsize 2 dBm 
      PCS1900 range: 0=30dBm, 15=0dBm, stepsize 2 dBm 
                    30=33dBm, 31=32dBm 
  ny1:
    Maximum number of repetitions for PHYSICAL INFORMATION message (GSM 04.08): 20
  powerOutputThresholds:
    Out Power Fault Threshold:     -10 dB
    Red Out Power Threshold:       - 6 dB
    Excessive Out Power Threshold:   5 dB
  rACHBusyThreshold: -127 dBm 
  rACHLoadAveragingSlots: 250 ,number of RACH burst periods
  rfResourceIndicationPeriod: 125  SACCH multiframes 
  T200:
    SDCCH:                044 in  5 ms
    FACCH/Full rate:      031 in  5 ms
    FACCH/Half rate:      041 in  5 ms
    SACCH with TCH SAPI0: 090 in 10 ms
    SACCH with SDCCH:     090 in 10 ms
    SDCCH with SAPI3:     090 in  5 ms
    SACCH with TCH SAPI3: 135 in 10 ms
  tSync: 9000 units of 10 msec
  tTrau: 9000 units of 10 msec
  enableUmLoopTest: 00h =  disabled
  enableExcessiveDistance: 00h =  Disabled
  excessiveDistance: 64km
  hoppingMode: 00h = baseband hopping
  cellType: 00h =  Standard Cell
  BCCH ARFCN / bCCHFrequency: 1
*/

unsigned char msg_2[] = 
{
	0x41, 0x01, 0x00, 0xFF, 0xFF,
		NM_ATT_BSIC, 0x3F,
		NM_ATT_BTS_AIR_TIMER, 0x04,
		NM_ATT_BS11_BTSLS_HOPPING, 0x00,
		NM_ATT_CCCH_L_I_P, 0x01,
		NM_ATT_CCCH_L_T, 0x00,
		NM_ATT_BS11_CELL_ALLOC_NR, 0x00,
		NM_ATT_BS11_ENA_INTERF_CLASS, 0x00,
		NM_ATT_BS11_FACCH_QUAL, 0x06,
		NM_ATT_INTAVE_PARAM, 0x1F, 
		NM_ATT_INTERF_BOUND, 0x0A, 0x0F, 0x14, 0x19, 0x1E, 0x7B,
		NM_ATT_CCCH_L_T, 0x23,
		NM_ATT_GSM_TIME, 0x28, 0x00,
		NM_ATT_ADM_STATE, 0x03,
		NM_ATT_RACH_B_THRESH, 0x7F,
		NM_ATT_LDAVG_SLOTS, 0x00, 0xFA,
		NM_ATT_BS11_RF_RES_IND_PER, 0x7D,
		NM_ATT_T200, 0x2C, 0x1F, 0x29, 0x5A, 0x5A, 0x5A, 0x87,
		NM_ATT_BS11_TSYNC, 0x23, 0x28,
		NM_ATT_BS11_TTRAU, 0x23, 0x28, 
		NM_ATT_TEST_DUR, 0x01, 0x00,
		NM_ATT_OUTST_ALARM, 0x01, 0x00,
		NM_ATT_BS11_EXCESSIVE_DISTANCE, 0x01, 0x40,
		NM_ATT_BS11_HOPPING_MODE, 0x01, 0x00,
		NM_ATT_BS11_PLL, 0x01, 0x00, 
		NM_ATT_BCCH_ARFCN, 0x00, HARDCODED_ARFCN/*0x01*/, 
};

// Handover Recognition, SET ATTRIBUTES

/*
Illegal Contents GSM Formatted O&M Msg 
  Object Class: Handover Recognition
  BTS relat. Number: 0 
  Instance 2: FF
  Instance 3: FF
SET ATTRIBUTES
  enableDelayPowerBudgetHO: 00h = Disabled
  enableDistanceHO: 00h =  Disabled
  enableInternalInterCellHandover: 00h = Disabled
  enableInternalIntraCellHandover: 00h =  Disabled
  enablePowerBudgetHO: 00h = Disabled
  enableRXLEVHO: 00h =  Disabled
  enableRXQUALHO: 00h =  Disabled
  hoAveragingDistance: 8  SACCH multiframes 
  hoAveragingLev:
    A_LEV_HO: 8  SACCH multiframes 
    W_LEV_HO: 1  SACCH multiframes 
  hoAveragingPowerBudget:  16  SACCH multiframes 
  hoAveragingQual:
    A_QUAL_HO: 8  SACCH multiframes 
    W_QUAL_HO: 2  SACCH multiframes 
  hoLowerThresholdLevDL: (10 - 110) dBm
  hoLowerThresholdLevUL: (5 - 110) dBm
  hoLowerThresholdQualDL: 06h =   6.4% < BER < 12.8%
  hoLowerThresholdQualUL: 06h =   6.4% < BER < 12.8%
  hoThresholdLevDLintra : (20 - 110) dBm
  hoThresholdLevULintra: (20 - 110) dBm
  hoThresholdMsRangeMax: 20 km 
  nCell: 06h
  timerHORequest: 3  ,unit 2 SACCH multiframes 
*/

unsigned char msg_3[] = 
{
	0xD0, 0xA1, 0x00, 0xFF, 0xFF, 
		0xD0, 0x00,
		0x64, 0x00,
		0x67, 0x00,
		0x68, 0x00,
		0x6A, 0x00,
		0x6C, 0x00,
		0x6D, 0x00,
		0x6F, 0x08,
		0x70, 0x08, 0x01,
		0x71, 0x10, 0x10, 0x10,
		0x72, 0x08, 0x02,
		0x73, 0x0A,
		0x74, 0x05,
		0x75, 0x06,
		0x76, 0x06,
		0x78, 0x14,
		0x79, 0x14,
		0x7A, 0x14,
		0x7D, 0x06,
		0x92, 0x03, 0x20, 0x01, 0x00,
		0x45, 0x01, 0x00,
		0x48, 0x01, 0x00,
		0x5A, 0x01, 0x00,
		0x5B, 0x01, 0x05,
		0x5E, 0x01, 0x1A,
		0x5F, 0x01, 0x20,
		0x9D, 0x01, 0x00,
		0x47, 0x01, 0x00,
		0x5C, 0x01, 0x64,
		0x5D, 0x01, 0x1E,
		0x97, 0x01, 0x20,
		0xF7, 0x01, 0x3C,
};

// Power Control, SET ATTRIBUTES

/*
  Object Class: Power Control
  BTS relat. Number: 0 
  Instance 2: FF
  Instance 3: FF
SET ATTRIBUTES
  enableMsPowerControl: 00h =  Disabled
  enablePowerControlRLFW: 00h =  Disabled
  pcAveragingLev:
    A_LEV_PC: 4  SACCH multiframes 
    W_LEV_PC: 1  SACCH multiframes 
  pcAveragingQual:
    A_QUAL_PC: 4  SACCH multiframes 
    W_QUAL_PC: 2  SACCH multiframes 
  pcLowerThresholdLevDL: 0Fh
  pcLowerThresholdLevUL: 0Ah
  pcLowerThresholdQualDL: 05h =   3.2% < BER <  6.4%
  pcLowerThresholdQualUL: 05h =   3.2% < BER <  6.4%
  pcRLFThreshold: 0Ch
  pcUpperThresholdLevDL: 14h
  pcUpperThresholdLevUL: 0Fh
  pcUpperThresholdQualDL: 04h =   1.6% < BER <  3.2%
  pcUpperThresholdQualUL: 04h =   1.6% < BER <  3.2%
  powerConfirm: 2  ,unit 2 SACCH multiframes 
  powerControlInterval: 2  ,unit 2 SACCH multiframes 
  powerIncrStepSize: 02h = 4 dB
  powerRedStepSize: 01h = 2 dB
  radioLinkTimeoutBs: 64  SACCH multiframes 
  enableBSPowerControl: 00h =  disabled
*/

unsigned char msg_4[] = 
{
	0xD0, 0xA2, 0x00, 0xFF, 0xFF, 
		NM_ATT_BS11_ENA_MS_PWR_CTRL, 0x00,
		NM_ATT_BS11_ENA_PWR_CTRL_RLFW, 0x00,
		0x7E, 0x04, 0x01,
		0x7F, 0x04, 0x02,
		0x80, 0x0F,
		0x81, 0x0A,
		0x82, 0x05,
		0x83, 0x05,
		0x84, 0x0C, 
		0x85, 0x14, 
		0x86, 0x0F, 
		0x87, 0x04,
		0x88, 0x04,
		0x89, 0x02,
		0x8A, 0x02,
		0x8B, 0x02,
		0x8C, 0x01,
		0x8D, 0x40,
		0x65, 0x01, 0x00 // set to 0x01 to enable BSPowerControl
};


// Transceiver, SET TRX ATTRIBUTES (TRX 0)

/*
  Object Class: Transceiver
  BTS relat. Number: 0 
  Tranceiver number: 0 
  Instance 3: FF
SET TRX ATTRIBUTES
  aRFCNList (HEX):  0001
  txPwrMaxReduction: 00h =   30dB
  radioMeasGran: 254  SACCH multiframes 
  radioMeasRep: 01h =  enabled
  memberOfEmergencyConfig: 01h =  TRUE
  trxArea: 00h = TRX doesn't belong to a concentric cell
*/

unsigned char msg_6[] = 
{
	0x44, 0x02, 0x00, 0x00, 0xFF, 
		NM_ATT_ARFCN_LIST, 0x01, 0x00, HARDCODED_ARFCN /*0x01*/,
		NM_ATT_RF_MAXPOWR_R, 0x00,
		NM_ATT_BS11_RADIO_MEAS_GRAN, 0x01, 0xFE, 
		NM_ATT_BS11_RADIO_MEAS_REP, 0x01, 0x01,
		NM_ATT_BS11_EMRG_CFG_MEMBER, 0x01, 0x01,
		NM_ATT_BS11_TRX_AREA, 0x01, 0x00, 
};

static unsigned char nanobts_attr_bts[] = {
	NM_ATT_INTERF_BOUND, 0x55, 0x5b, 0x61, 0x67, 0x6d, 0x73,
	NM_ATT_INTAVE_PARAM, 0x06,
	NM_ATT_CONN_FAIL_CRIT, 0x00, 0x02, 0x01, 0x10, 
	NM_ATT_T200, 0x1e, 0x24, 0x24, 0xa8, 0x34, 0x21, 0xa8,
	NM_ATT_MAX_TA, 0x3f,
	NM_ATT_OVERL_PERIOD, 0x00, 0x01, 10, /* seconds */
	NM_ATT_CCCH_L_T, 10, /* percent */
	NM_ATT_CCCH_L_I_P, 1, /* seconds */
	NM_ATT_RACH_B_THRESH, 0x0a,
	NM_ATT_LDAVG_SLOTS, 0x03, 0xe8,
	NM_ATT_BTS_AIR_TIMER, 0x80,
	NM_ATT_NY1, 0x0a,
	NM_ATT_BCCH_ARFCN, HARDCODED_ARFCN >> 8, HARDCODED_ARFCN & 0xff,
	NM_ATT_BSIC, 0x20,
};

static unsigned char nanobts_attr_radio[] = {
	NM_ATT_RF_MAXPOWR_R, 0x0c,
	NM_ATT_ARFCN_LIST, 0x00, 0x02, HARDCODED_ARFCN >> 8, HARDCODED_ARFCN & 0xff,
};

static unsigned char nanobts_attr_e0[] = {
	0x85, 0x00,
	0x81, 0x0b, 0xbb,	/* TCP PORT for RSL */
};

int nm_state_event(enum nm_evt evt, u_int8_t obj_class, void *obj,
		   struct gsm_nm_state *old_state, struct gsm_nm_state *new_state)
{
	struct gsm_bts *bts;
	struct gsm_bts_trx *trx;
	struct gsm_bts_trx_ts *ts;

	/* This is currently only required on nanoBTS */

	switch (evt) {
	case EVT_STATECHG_OPER:
		switch (obj_class) {
		case NM_OC_SITE_MANAGER:
			bts = container_of(obj, struct gsm_bts, site_mgr);
			if (old_state->operational != 2 && new_state->operational == 2) {
				abis_nm_opstart(bts, NM_OC_SITE_MANAGER, 0xff, 0xff, 0xff);
			}
			break;
		case NM_OC_BTS:
			bts = (struct gsm_bts *)obj;
			if (new_state->availability == 5) {
				abis_nm_set_bts_attr(bts, nanobts_attr_bts,
							sizeof(nanobts_attr_bts));
				abis_nm_opstart(bts, NM_OC_BTS,
						bts->nr, 0xff, 0xff);
				abis_nm_chg_adm_state(bts, NM_OC_BTS,
						      bts->nr, 0xff, 0xff,
						      NM_STATE_UNLOCKED);
			}
			break;
		case NM_OC_RADIO_CARRIER:
			trx = (struct gsm_bts_trx *)obj;
			if (new_state->availability == 3) {
				abis_nm_set_radio_attr(trx, nanobts_attr_radio,
							sizeof(nanobts_attr_radio));
				abis_nm_opstart(trx->bts, NM_OC_RADIO_CARRIER,
						trx->bts->nr, trx->nr, 0xff);
				abis_nm_chg_adm_state(trx->bts, NM_OC_RADIO_CARRIER,
						      trx->bts->nr, trx->nr, 0xff,
						      NM_STATE_UNLOCKED);
			}
			break;
		case NM_OC_CHANNEL:
			ts = (struct gsm_bts_trx_ts *)obj;
			trx = (struct gsm_bts_trx *)ts->trx;
			if (new_state->availability == 5) {
				if (ts->nr == 0 && trx == trx->bts->c0)
					abis_nm_set_channel_attr(ts, NM_CHANC_BCCH_CBCH);
				else
					abis_nm_set_channel_attr(ts, NM_CHANC_TCHFull);
				abis_nm_opstart(trx->bts, NM_OC_CHANNEL,
						trx->bts->nr, trx->nr, ts->nr);
				abis_nm_chg_adm_state(trx->bts, NM_OC_CHANNEL,
						      trx->bts->nr, trx->nr, ts->nr,
						      NM_STATE_UNLOCKED);
			}
			break;
		case NM_OC_BASEB_TRANSC:
			trx = container_of(obj, struct gsm_bts_trx, bb_transc);
			if (new_state->availability == 5) {
				abis_nm_ipaccess_msg(trx->bts, 0xe0, NM_OC_BASEB_TRANSC,
						     trx->bts->nr, trx->nr, 0xff,
						     nanobts_attr_e0, sizeof(nanobts_attr_e0));
				abis_nm_opstart(trx->bts, NM_OC_BASEB_TRANSC, 
						trx->bts->nr, trx->nr, 0xff);
				abis_nm_chg_adm_state(trx->bts, NM_OC_BASEB_TRANSC, 
							trx->bts->nr, trx->nr, 0xff,
							NM_STATE_UNLOCKED);
			}
			break;
		}
		break;
	case EVT_STATECHG_ADM:
		DEBUGP(DMM, "Unhandled state change in %s:%d\n", __func__, __LINE__);
		break;
	}
	return 0;
}

static void bootstrap_om_nanobts(struct gsm_bts *bts)
{
	/* We don't do callback based bootstrapping, but event driven (see above) */
}

static void bootstrap_om_bs11(struct gsm_bts *bts)
{
	struct gsm_bts_trx *trx = &bts->trx[0];

	/* stop sending event reports */
	abis_nm_event_reports(bts, 0);

	/* begin DB transmission */
	abis_nm_bs11_db_transmission(bts, 1);

	/* end DB transmission */
	abis_nm_bs11_db_transmission(bts, 0);

	/* Reset BTS Site manager resource */
	abis_nm_bs11_reset_resource(bts);

	/* begin DB transmission */
	abis_nm_bs11_db_transmission(bts, 1);

	abis_nm_raw_msg(bts, sizeof(msg_1), msg_1); /* set BTS SiteMgr attr*/
	abis_nm_raw_msg(bts, sizeof(msg_2), msg_2); /* set BTS attr */
	abis_nm_raw_msg(bts, sizeof(msg_3), msg_3); /* set BTS handover attr */
	abis_nm_raw_msg(bts, sizeof(msg_4), msg_4); /* set BTS power control attr */

	/* Connect signalling of bts0/trx0 to e1_0/ts1/64kbps */
	abis_nm_conn_terr_sign(trx, 0, 1, 0xff);
	set_ts_e1link(&trx->ts[0], 0, 1, 0xff);
	abis_nm_raw_msg(bts, sizeof(msg_6), msg_6); /* SET TRX ATTRIBUTES */

	/* Use TEI 1 for signalling */
	abis_nm_establish_tei(bts, 0, 0, 1, 0xff, 0x01);
	abis_nm_set_channel_attr(&trx->ts[0], NM_CHANC_SDCCH_CBCH);

#ifdef HAVE_TRX1
	/* TRX 1 */
	abis_nm_conn_terr_sign(&bts->trx[1], 0, 1, 0xff);
	/* FIXME: TRX ATTRIBUTE */
	abis_nm_establish_tei(bts, 0, 0, 1, 0xff, 0x02);
#endif

	/* SET CHANNEL ATTRIBUTE TS1 */
	abis_nm_set_channel_attr(&trx->ts[1], NM_CHANC_TCHFull);
	/* Connect traffic of bts0/trx0/ts1 to e1_0/ts2/b */
	set_ts_e1link(&trx->ts[1], 0, 2, 1);
	abis_nm_conn_terr_traf(&trx->ts[1], 0, 2, 1);
	
	/* SET CHANNEL ATTRIBUTE TS2 */
	abis_nm_set_channel_attr(&trx->ts[2], NM_CHANC_TCHFull);
	/* Connect traffic of bts0/trx0/ts2 to e1_0/ts2/c */
	set_ts_e1link(&trx->ts[2], 0, 2, 2);
	abis_nm_conn_terr_traf(&trx->ts[2], 0, 2, 2);

	/* SET CHANNEL ATTRIBUTE TS3 */
	abis_nm_set_channel_attr(&trx->ts[3], NM_CHANC_TCHFull);
	/* Connect traffic of bts0/trx0/ts3 to e1_0/ts2/d */
	set_ts_e1link(&trx->ts[3], 0, 2, 3);
	abis_nm_conn_terr_traf(&trx->ts[3], 0, 2, 3);

	/* SET CHANNEL ATTRIBUTE TS4 */
	abis_nm_set_channel_attr(&trx->ts[4], NM_CHANC_TCHFull);
	/* Connect traffic of bts0/trx0/ts4 to e1_0/ts3/a */
	set_ts_e1link(&trx->ts[4], 0, 3, 0);
	abis_nm_conn_terr_traf(&trx->ts[4], 0, 3, 0);

	/* SET CHANNEL ATTRIBUTE TS5 */
	abis_nm_set_channel_attr(&trx->ts[5], NM_CHANC_TCHFull);
	/* Connect traffic of bts0/trx0/ts5 to e1_0/ts3/b */
	set_ts_e1link(&trx->ts[5], 0, 3, 1);
	abis_nm_conn_terr_traf(&trx->ts[5], 0, 3, 1);

	/* SET CHANNEL ATTRIBUTE TS6 */
	abis_nm_set_channel_attr(&trx->ts[6], NM_CHANC_TCHFull);
	/* Connect traffic of bts0/trx0/ts6 to e1_0/ts3/c */
	set_ts_e1link(&trx->ts[6], 0, 3, 2);
	abis_nm_conn_terr_traf(&trx->ts[6], 0, 3, 2);

	/* SET CHANNEL ATTRIBUTE TS7 */
	abis_nm_set_channel_attr(&trx->ts[7], NM_CHANC_TCHFull);
	/* Connect traffic of bts0/trx0/ts7 to e1_0/ts3/d */
	set_ts_e1link(&trx->ts[7], 0, 3, 3);
	abis_nm_conn_terr_traf(&trx->ts[7], 0, 3, 3);

	/* end DB transmission */
	abis_nm_bs11_db_transmission(bts, 0);

	/* Reset BTS Site manager resource */
	abis_nm_bs11_reset_resource(bts);

	/* restart sending event reports */
	abis_nm_event_reports(bts, 1);
}

static void bootstrap_om(struct gsm_bts *bts)
{
	fprintf(stdout, "bootstrapping OML\n");

	switch (bts->type) {
	case GSM_BTS_TYPE_BS11:
		bootstrap_om_bs11(bts);
		break;
	case GSM_BTS_TYPE_NANOBTS_900:
	case GSM_BTS_TYPE_NANOBTS_1800:
		bootstrap_om_nanobts(bts);
		break;
	default:
		fprintf(stderr, "Unable to bootstrap OML: Unknown BTS type %d\n", bts->type);
	}
}

static int shutdown_om(struct gsm_bts *bts)
{
	/* stop sending event reports */
	abis_nm_event_reports(bts, 0);

	/* begin DB transmission */
	abis_nm_bs11_db_transmission(bts, 1);

	/* end DB transmission */
	abis_nm_bs11_db_transmission(bts, 0);

	/* Reset BTS Site manager resource */
	abis_nm_bs11_reset_resource(bts);

	return 0;
}

struct bcch_info {
	u_int8_t type;
	u_int8_t len;
	const u_int8_t *data;
};

/*
SYSTEM INFORMATION TYPE 1
  Cell channel description
    Format-ID bit map 0
    CA-ARFCN Bit 124...001 (Hex): 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01
  RACH Control Parameters
    maximum 7 retransmissions
    8 slots used to spread transmission
    cell not barred for access
    call reestablishment not allowed
    Access Control Class = 0000
*/
static u_int8_t si1[] = {
	/* header */0x55, 0x06, 0x19,
	/* ccdesc */0x04 /*0x00*/, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 /*0x01*/,
	/* rach */0xD5, 0x00, 0x00,
	/* s1 reset*/0x2B
};

/*
 SYSTEM INFORMATION TYPE 2
  Neighbour Cells Description
    EXT-IND: Carries the complete BA
    BA-IND = 0
    Format-ID bit map 0
    CA-ARFCN Bit 124...001 (Hex): 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  NCC permitted (NCC) = FF
  RACH Control Parameters
    maximum 7 retransmissions
    8 slots used to spread transmission
    cell not barred for access
    call reestablishment not allowed
    Access Control Class = 0000
*/
static u_int8_t si2[] = {
	/* header */0x59, 0x06, 0x1A,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* ncc */0xFF,
	/* rach*/0xD5, 0x00, 0x00
};

/*
SYSTEM INFORMATION TYPE 3
  Cell identity = 00001 (1h)
  Location area identification
    Mobile Country Code (MCC): 001
    Mobile Network Code (MNC): 01
    Location Area Code  (LAC): 00001 (1h)
  Control Channel Description
    Attach-detach: MSs in the cell are not allowed to apply IMSI attach /detach
    0 blocks reserved for access grant
    1 channel used for CCCH, with SDCCH
    5 multiframes period for PAGING REQUEST
    Time-out T3212 = 0
  Cell Options BCCH
    Power control indicator: not set
    MSs shall not use uplink DTX
    Radio link timeout = 36
  Cell Selection Parameters
    Cell reselect hysteresis = 6 dB RXLEV hysteresis for LA re-selection
    max.TX power level MS may use for CCH = 2 <- according to GSM05.05 39dBm (max)
    Additional Reselect Parameter Indication (ACS) = only SYSTEM INFO 4: The SI rest octets, if present, shall be used to derive the value of PI and possibly C2 parameters
    Half rate support (NECI): New establishment causes are not supported
    min.RX signal level for MS = 0
  RACH Control Parameters
    maximum 7 retransmissions
    8 slots used to spread transmission
    cell not barred for access
    call reestablishment not allowed
    Access Control Class = 0000
  SI 3 Rest Octets
    Cell Bar Qualify (CBQ): 0
    Cell Reselect Offset = 0 dB
    Temporary Offset = 0 dB
    Penalty Time = 20 s
    System Information 2ter Indicator (2TI): 0 = not available
    Early Classmark Sending Control (ECSC):  0 = forbidden
    Scheduling Information is not sent in SYSTEM INFORMATION TYPE 9 on the BCCH
*/
static u_int8_t si3[] = {
	/* header */0x49, 0x06, 0x1B,
	/* cell */0x00, 0x01,
	/* lai  */0x00, 0xF1, 0x10, 0x00, 0x01,
	/* desc */0x01, 0x03, 0x00,
	/* option*/0x28,
	/* selection*/0x62, 0x00,
	/* rach */0xD5, 0x00, 0x00,
	/* reset*/0x80, 0x00, 0x00, 0x2B
};

/*
SYSTEM INFORMATION TYPE 4
  Location area identification
    Mobile Country Code (MCC): 001
    Mobile Network Code (MNC): 01
    Location Area Code  (LAC): 00001 (1h)
  Cell Selection Parameters
    Cell reselect hysteresis = 6 dB RXLEV hysteresis for LA re-selection
    max.TX power level MS may use for CCH = 2
    Additional Reselect Parameter Indication (ACS) = only SYSTEM INFO 4: The SI rest octets, if present, shall be used to derive the value of PI and possibly C2 parameters
    Half rate support (NECI): New establishment causes are not supported
    min.RX signal level for MS = 0
  RACH Control Parameters
    maximum 7 retransmissions
    8 slots used to spread transmission
    cell not barred for access
    call reestablishment not allowed
    Access Control Class = 0000
  Channel Description
    Type = SDCCH/4[2]
    Timeslot Number: 0
    Training Sequence Code: 7h
    ARFCN: 1
  SI Rest Octets
    Cell Bar Qualify (CBQ): 0
    Cell Reselect Offset = 0 dB
    Temporary Offset = 0 dB
    Penalty Time = 20 s
*/
static u_int8_t si4[] = {
	/* header */0x41, 0x06, 0x1C,
	/* lai */0x00, 0xF1, 0x10, 0x00, 0x01,
	/* sel */0x62, 0x00,
	/* rach*/0xD5, 0x00, 0x00,
	/* var */0x64, 0x30, 0xE0, HARDCODED_ARFCN/*0x01*/, 0x80, 0x00, 0x00,
	0x2B, 0x2B, 0x2B
};

/*
 SYSTEM INFORMATION TYPE 5
  Neighbour Cells Description
    EXT-IND: Carries the complete BA
    BA-IND = 0
    Format-ID bit map 0
    CA-ARFCN Bit 124...001 (Hex): 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
*/

static u_int8_t si5[] = {
	/* header without l2 len*/0x06, 0x1D,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// SYSTEM INFORMATION TYPE 6

/*
SACCH FILLING
  System Info Type: SYSTEM INFORMATION 6
  L3 Information (Hex): 06 1E 00 01 xx xx 10 00 01 28 FF

SYSTEM INFORMATION TYPE 6
  Cell identity = 00001 (1h)
  Location area identification
    Mobile Country Code (MCC): 001
    Mobile Network Code (MNC): 01
    Location Area Code  (LAC): 00001 (1h)
  Cell Options SACCH
    Power control indicator: not set
    MSs shall not use uplink DTX on a TCH-F. MS shall not use uplink DTX on TCH-H.
    Radio link timeout = 36
  NCC permitted (NCC) = FF
*/

static u_int8_t si6[] = {
	/* header */0x06, 0x1E,
	/* cell id*/ 0x00, 0x01,
	/* lai */ 0x00, 0xF1, 0x10, 0x00, 0x01,
	/* options */ 0x28,
	/* ncc */ 0xFF,
};



static const struct bcch_info bcch_infos[] = {
	{
		RSL_SYSTEM_INFO_1,
		sizeof(si1),
		si1,
	}, {
		RSL_SYSTEM_INFO_2,
		sizeof(si2),
		si2,
	}, {
		RSL_SYSTEM_INFO_3,
		sizeof(si3),
		si3,
	}, {
		RSL_SYSTEM_INFO_4,
		sizeof(si4),
		si4,
	},
};

static_assert(sizeof(si1) == sizeof(struct gsm48_system_information_type_1), type1)
static_assert(sizeof(si2) == sizeof(struct gsm48_system_information_type_2), type2)
static_assert(sizeof(si3) == sizeof(struct gsm48_system_information_type_3), type3)
static_assert(sizeof(si4) >= sizeof(struct gsm48_system_information_type_4), type4)
static_assert(sizeof(si5) == sizeof(struct gsm48_system_information_type_5), type5)
static_assert(sizeof(si6) >= sizeof(struct gsm48_system_information_type_6), type6)

/* set all system information types */
static int set_system_infos(struct gsm_bts_trx *trx)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(bcch_infos); i++) {
		rsl_bcch_info(trx, bcch_infos[i].type,
			      bcch_infos[i].data,
			      bcch_infos[i].len);
	}
	rsl_sacch_filling(trx, RSL_SYSTEM_INFO_5, si5, sizeof(si5));
	rsl_sacch_filling(trx, RSL_SYSTEM_INFO_6, si6, sizeof(si6));

	return 0;
}

/*
 * Inform anyone...
 */
static void bsc_hack_channel_allocated(struct gsm_lchan *lchan) {
}

/*
 * Patch the various SYSTEM INFORMATION tables to update
 * the LAI
 */
static void patch_tables(struct gsm_bts *bts)
{
	u_int8_t arfcn_low = bts->trx[0].arfcn & 0xff;
	u_int8_t arfcn_high = (bts->trx[0].arfcn >> 8) & 0x0f;
	/* covert the raw packet to the struct */
	struct gsm48_system_information_type_3 *type_3 =
		(struct gsm48_system_information_type_3*)&si3;
	struct gsm48_system_information_type_4 *type_4 =
		(struct gsm48_system_information_type_4*)&si4;
	struct gsm48_system_information_type_6 *type_6 =
		(struct gsm48_system_information_type_6*)&si6;
	struct gsm48_loc_area_id lai;

	gsm0408_generate_lai(&lai, bts->network->country_code,
				bts->network->network_code, bts->location_area_code);

	/* assign the MCC and MNC */
	type_3->lai = lai;
	type_4->lai = lai;
	type_6->lai = lai;

	/* patch ARFCN into BTS Attributes */
	msg_2[74] &= 0xf0;
	msg_2[74] |= arfcn_high;
	msg_2[75] = arfcn_low;
	nanobts_attr_bts[42] &= 0xf0;
	nanobts_attr_bts[42] |= arfcn_high;
	nanobts_attr_bts[43] = arfcn_low;

	/* patch ARFCN into TRX Attributes */
	msg_6[7] &= 0xf0;
	msg_6[7] |= arfcn_high;
	msg_6[8] = arfcn_low;
	nanobts_attr_radio[5] &= 0xf0;
	nanobts_attr_radio[5] |= arfcn_high;
	nanobts_attr_radio[6] = arfcn_low;

	type_4->data[2] &= 0xf0;
	type_4->data[2] |= arfcn_high;
	type_4->data[3] = arfcn_low;

	/* patch Control Channel Description 10.5.2.11 */
	type_3->control_channel_desc = bts->chan_desc;
}


static void bootstrap_rsl(struct gsm_bts_trx *trx)
{
	fprintf(stdout, "bootstrapping RSL MCC=%u MNC=%u\n", trx->bts->network->country_code, trx->bts->network->network_code);
	set_system_infos(trx);
}

void input_event(int event, enum e1inp_sign_type type, struct gsm_bts_trx *trx)
{
	switch (event) {
	case EVT_E1_TEI_UP:
		switch (type) {
		case E1INP_SIGN_OML:
			bootstrap_om(trx->bts);
			break;
		case E1INP_SIGN_RSL:
			bootstrap_rsl(trx);
			break;
		default:
			break;
		}
		break;
	case EVT_E1_TEI_DN:
		fprintf(stderr, "Lost some E1 TEI link\n");
		/* FIXME: deal with TEI or L1 link loss */
		break;
	default:
		break;
	}
}

void *bootstrap_network(int (*mncc_recv)(void *, int, void *),int bts_type, int mcc, int mnc, int lac, int arfcn, int cardnr, int release_l2, char *name_short, char *name_long, char *hlr, int allow_all)
{
	struct gsm_bts *bts;
	struct gsm_network *gsmnet;

	/* open database */
	if (db_init(hlr)) {
		fprintf(stderr, "DB: Failed to init HLR database '%s'. Please check the option settings.\n", hlr);
		return NULL;
	}	 
	if (db_prepare()) {
		fprintf(stderr, "DB: Failed to prepare database.\n");
		return NULL;
	}

	/* seed the PRNG for TMSI */
	srand(time(NULL));

	/* initialize our data structures */
	gsmnet = gsm_network_init(1, (gsm_bts_type)bts_type, mcc, mnc, mncc_recv);
	if (!gsmnet)
		return 0;

	gsmnet->name_long = name_long;
	gsmnet->name_short = name_short;
	bts = &gsmnet->bts[0];
	bts->location_area_code = lac;
	bts->trx[0].arfcn = arfcn;

	/* Control Channel Description */
	memset(&bts->chan_desc, 0, sizeof(struct gsm48_control_channel_descr));
	bts->chan_desc.att = 1;
	bts->chan_desc.ccch_conf = RSL_BCCH_CCCH_CONF_1_C;
	bts->chan_desc.bs_pa_mfrms = RSL_BS_PA_MFRMS_5;
	bts->chan_desc.t3212 = 0;

	patch_tables(bts);

	paging_init(bts);
	bts->paging.channel_allocated = bsc_hack_channel_allocated;

	telnet_init(gsmnet, 4242);

	/* E1 mISDN input setup */
	if (bts_type == GSM_BTS_TYPE_BS11) {
		if (e1_config(bts, cardnr, release_l2))
			return NULL;
	} else {
		if (ia_config(bts))
			return NULL;
	}

	if (allow_all)
		gsm0408_allow_everyone(1);

	return gsmnet;
}

int shutdown_net(void *network)
{
	struct gsm_network *net = (struct gsm_network *)network;
	unsigned int i;
	for (i = 0; i < net->num_bts; i++) {
		int rc;
		rc = shutdown_om(&net->bts[i]);
		if (rc < 0)
			return rc;
	}

	return 0;
}

#ifdef __cplusplus
}
#endif