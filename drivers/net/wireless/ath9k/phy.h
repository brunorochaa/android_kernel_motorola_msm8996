/*
 * Copyright (c) 2008 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef PHY_H
#define PHY_H

bool ath9k_hw_ar9280_set_channel(struct ath_hal *ah,
					  struct ath9k_channel
					  *chan);
bool ath9k_hw_set_channel(struct ath_hal *ah,
				   struct ath9k_channel *chan);
void ath9k_hw_write_regs(struct ath_hal *ah, u32 modesIndex,
			 u32 freqIndex, int regWrites);
bool ath9k_hw_set_rf_regs(struct ath_hal *ah,
				   struct ath9k_channel *chan,
				   u16 modesIndex);
void ath9k_hw_decrease_chain_power(struct ath_hal *ah,
				   struct ath9k_channel *chan);
bool ath9k_hw_init_rf(struct ath_hal *ah,
			       int *status);

#define AR_PHY_BASE     0x9800
#define AR_PHY(_n)      (AR_PHY_BASE + ((_n)<<2))

#define AR_PHY_TEST             0x9800
#define PHY_AGC_CLR             0x10000000
#define RFSILENT_BB             0x00002000

#define AR_PHY_TURBO                0x9804
#define AR_PHY_FC_TURBO_MODE        0x00000001
#define AR_PHY_FC_TURBO_SHORT       0x00000002
#define AR_PHY_FC_DYN2040_EN        0x00000004
#define AR_PHY_FC_DYN2040_PRI_ONLY  0x00000008
#define AR_PHY_FC_DYN2040_PRI_CH    0x00000010
#define AR_PHY_FC_DYN2040_EXT_CH    0x00000020
#define AR_PHY_FC_HT_EN             0x00000040
#define AR_PHY_FC_SHORT_GI_40       0x00000080
#define AR_PHY_FC_WALSH             0x00000100
#define AR_PHY_FC_SINGLE_HT_LTF1    0x00000200

#define AR_PHY_TIMING2           0x9810
#define AR_PHY_TIMING3           0x9814
#define AR_PHY_TIMING3_DSC_MAN   0xFFFE0000
#define AR_PHY_TIMING3_DSC_MAN_S 17
#define AR_PHY_TIMING3_DSC_EXP   0x0001E000
#define AR_PHY_TIMING3_DSC_EXP_S 13

#define AR_PHY_CHIP_ID            0x9818
#define AR_PHY_CHIP_ID_REV_0      0x80
#define AR_PHY_CHIP_ID_REV_1      0x81
#define AR_PHY_CHIP_ID_9160_REV_0 0xb0

#define AR_PHY_ACTIVE       0x981C
#define AR_PHY_ACTIVE_EN    0x00000001
#define AR_PHY_ACTIVE_DIS   0x00000000

#define AR_PHY_RF_CTL2             0x9824
#define AR_PHY_TX_END_DATA_START   0x000000FF
#define AR_PHY_TX_END_DATA_START_S 0
#define AR_PHY_TX_END_PA_ON        0x0000FF00
#define AR_PHY_TX_END_PA_ON_S      8

#define AR_PHY_RF_CTL3                  0x9828
#define AR_PHY_TX_END_TO_A2_RX_ON       0x00FF0000
#define AR_PHY_TX_END_TO_A2_RX_ON_S     16

#define AR_PHY_ADC_CTL                  0x982C
#define AR_PHY_ADC_CTL_OFF_INBUFGAIN    0x00000003
#define AR_PHY_ADC_CTL_OFF_INBUFGAIN_S  0
#define AR_PHY_ADC_CTL_OFF_PWDDAC       0x00002000
#define AR_PHY_ADC_CTL_OFF_PWDBANDGAP   0x00004000
#define AR_PHY_ADC_CTL_OFF_PWDADC       0x00008000
#define AR_PHY_ADC_CTL_ON_INBUFGAIN     0x00030000
#define AR_PHY_ADC_CTL_ON_INBUFGAIN_S   16

#define AR_PHY_ADC_SERIAL_CTL       0x9830
#define AR_PHY_SEL_INTERNAL_ADDAC   0x00000000
#define AR_PHY_SEL_EXTERNAL_RADIO   0x00000001

#define AR_PHY_RF_CTL4                    0x9834
#define AR_PHY_RF_CTL4_TX_END_XPAB_OFF    0xFF000000
#define AR_PHY_RF_CTL4_TX_END_XPAB_OFF_S  24
#define AR_PHY_RF_CTL4_TX_END_XPAA_OFF    0x00FF0000
#define AR_PHY_RF_CTL4_TX_END_XPAA_OFF_S  16
#define AR_PHY_RF_CTL4_FRAME_XPAB_ON      0x0000FF00
#define AR_PHY_RF_CTL4_FRAME_XPAB_ON_S    8
#define AR_PHY_RF_CTL4_FRAME_XPAA_ON      0x000000FF
#define AR_PHY_RF_CTL4_FRAME_XPAA_ON_S    0

#define AR_PHY_SETTLING          0x9844
#define AR_PHY_SETTLING_SWITCH   0x00003F80
#define AR_PHY_SETTLING_SWITCH_S 7

#define AR_PHY_RXGAIN                   0x9848
#define AR_PHY_RXGAIN_TXRX_ATTEN        0x0003F000
#define AR_PHY_RXGAIN_TXRX_ATTEN_S      12
#define AR_PHY_RXGAIN_TXRX_RF_MAX       0x007C0000
#define AR_PHY_RXGAIN_TXRX_RF_MAX_S     18
#define AR9280_PHY_RXGAIN_TXRX_ATTEN    0x00003F80
#define AR9280_PHY_RXGAIN_TXRX_ATTEN_S  7
#define AR9280_PHY_RXGAIN_TXRX_MARGIN   0x001FC000
#define AR9280_PHY_RXGAIN_TXRX_MARGIN_S 14

#define AR_PHY_DESIRED_SZ           0x9850
#define AR_PHY_DESIRED_SZ_ADC       0x000000FF
#define AR_PHY_DESIRED_SZ_ADC_S     0
#define AR_PHY_DESIRED_SZ_PGA       0x0000FF00
#define AR_PHY_DESIRED_SZ_PGA_S     8
#define AR_PHY_DESIRED_SZ_TOT_DES   0x0FF00000
#define AR_PHY_DESIRED_SZ_TOT_DES_S 20

#define AR_PHY_FIND_SIG           0x9858
#define AR_PHY_FIND_SIG_FIRSTEP   0x0003F000
#define AR_PHY_FIND_SIG_FIRSTEP_S 12
#define AR_PHY_FIND_SIG_FIRPWR    0x03FC0000
#define AR_PHY_FIND_SIG_FIRPWR_S  18

#define AR_PHY_AGC_CTL1                  0x985C
#define AR_PHY_AGC_CTL1_COARSE_LOW       0x00007F80
#define AR_PHY_AGC_CTL1_COARSE_LOW_S     7
#define AR_PHY_AGC_CTL1_COARSE_HIGH      0x003F8000
#define AR_PHY_AGC_CTL1_COARSE_HIGH_S    15

#define AR_PHY_AGC_CONTROL               0x9860
#define AR_PHY_AGC_CONTROL_CAL           0x00000001
#define AR_PHY_AGC_CONTROL_NF            0x00000002
#define AR_PHY_AGC_CONTROL_ENABLE_NF     0x00008000
#define AR_PHY_AGC_CONTROL_FLTR_CAL      0x00010000
#define AR_PHY_AGC_CONTROL_NO_UPDATE_NF  0x00020000

#define AR_PHY_CCA                  0x9864
#define AR_PHY_MINCCA_PWR           0x0FF80000
#define AR_PHY_MINCCA_PWR_S         19
#define AR_PHY_CCA_THRESH62         0x0007F000
#define AR_PHY_CCA_THRESH62_S       12
#define AR9280_PHY_MINCCA_PWR       0x1FF00000
#define AR9280_PHY_MINCCA_PWR_S     20
#define AR9280_PHY_CCA_THRESH62     0x000FF000
#define AR9280_PHY_CCA_THRESH62_S   12

#define AR_PHY_SFCORR_LOW                    0x986C
#define AR_PHY_SFCORR_LOW_USE_SELF_CORR_LOW  0x00000001
#define AR_PHY_SFCORR_LOW_M2COUNT_THR_LOW    0x00003F00
#define AR_PHY_SFCORR_LOW_M2COUNT_THR_LOW_S  8
#define AR_PHY_SFCORR_LOW_M1_THRESH_LOW      0x001FC000
#define AR_PHY_SFCORR_LOW_M1_THRESH_LOW_S    14
#define AR_PHY_SFCORR_LOW_M2_THRESH_LOW      0x0FE00000
#define AR_PHY_SFCORR_LOW_M2_THRESH_LOW_S    21

#define AR_PHY_SFCORR                0x9868
#define AR_PHY_SFCORR_M2COUNT_THR    0x0000001F
#define AR_PHY_SFCORR_M2COUNT_THR_S  0
#define AR_PHY_SFCORR_M1_THRESH      0x00FE0000
#define AR_PHY_SFCORR_M1_THRESH_S    17
#define AR_PHY_SFCORR_M2_THRESH      0x7F000000
#define AR_PHY_SFCORR_M2_THRESH_S    24

#define AR_PHY_SLEEP_CTR_CONTROL    0x9870
#define AR_PHY_SLEEP_CTR_LIMIT      0x9874
#define AR_PHY_SYNTH_CONTROL        0x9874
#define AR_PHY_SLEEP_SCAL           0x9878

#define AR_PHY_PLL_CTL          0x987c
#define AR_PHY_PLL_CTL_40       0xaa
#define AR_PHY_PLL_CTL_40_5413  0x04
#define AR_PHY_PLL_CTL_44       0xab
#define AR_PHY_PLL_CTL_44_2133  0xeb
#define AR_PHY_PLL_CTL_40_2133  0xea

#define AR_PHY_RX_DELAY           0x9914
#define AR_PHY_SEARCH_START_DELAY 0x9918
#define AR_PHY_RX_DELAY_DELAY     0x00003FFF

#define AR_PHY_TIMING_CTRL4(_i)     (0x9920 + ((_i) << 12))
#define AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF 0x01F
#define AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF_S   0
#define AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF 0x7E0
#define AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF_S   5
#define AR_PHY_TIMING_CTRL4_IQCORR_ENABLE   0x800
#define AR_PHY_TIMING_CTRL4_IQCAL_LOG_COUNT_MAX 0xF000
#define AR_PHY_TIMING_CTRL4_IQCAL_LOG_COUNT_MAX_S   12
#define AR_PHY_TIMING_CTRL4_DO_CAL    0x10000

#define AR_PHY_TIMING_CTRL4_ENABLE_SPUR_RSSI	0x80000000
#define	AR_PHY_TIMING_CTRL4_ENABLE_SPUR_FILTER	0x40000000
#define	AR_PHY_TIMING_CTRL4_ENABLE_CHAN_MASK	0x20000000
#define	AR_PHY_TIMING_CTRL4_ENABLE_PILOT_MASK	0x10000000

#define AR_PHY_TIMING5               0x9924
#define AR_PHY_TIMING5_CYCPWR_THR1   0x000000FE
#define AR_PHY_TIMING5_CYCPWR_THR1_S 1

#define AR_PHY_POWER_TX_RATE1               0x9934
#define AR_PHY_POWER_TX_RATE2               0x9938
#define AR_PHY_POWER_TX_RATE_MAX            0x993c
#define AR_PHY_POWER_TX_RATE_MAX_TPC_ENABLE 0x00000040

#define AR_PHY_FRAME_CTL            0x9944
#define AR_PHY_FRAME_CTL_TX_CLIP    0x00000038
#define AR_PHY_FRAME_CTL_TX_CLIP_S  3

#define AR_PHY_TXPWRADJ                   0x994C
#define AR_PHY_TXPWRADJ_CCK_GAIN_DELTA    0x00000FC0
#define AR_PHY_TXPWRADJ_CCK_GAIN_DELTA_S  6
#define AR_PHY_TXPWRADJ_CCK_PCDAC_INDEX   0x00FC0000
#define AR_PHY_TXPWRADJ_CCK_PCDAC_INDEX_S 18

#define AR_PHY_RADAR_EXT      0x9940
#define AR_PHY_RADAR_EXT_ENA  0x00004000

#define AR_PHY_RADAR_0          0x9954
#define AR_PHY_RADAR_0_ENA      0x00000001
#define AR_PHY_RADAR_0_FFT_ENA  0x80000000
#define AR_PHY_RADAR_0_INBAND   0x0000003e
#define AR_PHY_RADAR_0_INBAND_S 1
#define AR_PHY_RADAR_0_PRSSI    0x00000FC0
#define AR_PHY_RADAR_0_PRSSI_S  6
#define AR_PHY_RADAR_0_HEIGHT   0x0003F000
#define AR_PHY_RADAR_0_HEIGHT_S 12
#define AR_PHY_RADAR_0_RRSSI    0x00FC0000
#define AR_PHY_RADAR_0_RRSSI_S  18
#define AR_PHY_RADAR_0_FIRPWR   0x7F000000
#define AR_PHY_RADAR_0_FIRPWR_S 24

#define AR_PHY_RADAR_1                  0x9958
#define AR_PHY_RADAR_1_RELPWR_ENA       0x00800000
#define AR_PHY_RADAR_1_USE_FIR128       0x00400000
#define AR_PHY_RADAR_1_RELPWR_THRESH    0x003F0000
#define AR_PHY_RADAR_1_RELPWR_THRESH_S  16
#define AR_PHY_RADAR_1_BLOCK_CHECK      0x00008000
#define AR_PHY_RADAR_1_MAX_RRSSI        0x00004000
#define AR_PHY_RADAR_1_RELSTEP_CHECK    0x00002000
#define AR_PHY_RADAR_1_RELSTEP_THRESH   0x00001F00
#define AR_PHY_RADAR_1_RELSTEP_THRESH_S 8
#define AR_PHY_RADAR_1_MAXLEN           0x000000FF
#define AR_PHY_RADAR_1_MAXLEN_S         0

#define AR_PHY_SWITCH_CHAIN_0     0x9960
#define AR_PHY_SWITCH_COM         0x9964

#define AR_PHY_SIGMA_DELTA            0x996C
#define AR_PHY_SIGMA_DELTA_ADC_SEL    0x00000003
#define AR_PHY_SIGMA_DELTA_ADC_SEL_S  0
#define AR_PHY_SIGMA_DELTA_FILT2      0x000000F8
#define AR_PHY_SIGMA_DELTA_FILT2_S    3
#define AR_PHY_SIGMA_DELTA_FILT1      0x00001F00
#define AR_PHY_SIGMA_DELTA_FILT1_S    8
#define AR_PHY_SIGMA_DELTA_ADC_CLIP   0x01FFE000
#define AR_PHY_SIGMA_DELTA_ADC_CLIP_S 13

#define AR_PHY_RESTART          0x9970
#define AR_PHY_RESTART_DIV_GC   0x001C0000
#define AR_PHY_RESTART_DIV_GC_S 18

#define AR_PHY_RFBUS_REQ        0x997C
#define AR_PHY_RFBUS_REQ_EN     0x00000001

#define	AR_PHY_TIMING7		        0x9980
#define	AR_PHY_TIMING8		        0x9984
#define	AR_PHY_TIMING8_PILOT_MASK_2	0x000FFFFF
#define	AR_PHY_TIMING8_PILOT_MASK_2_S	0

#define	AR_PHY_BIN_MASK2_1	0x9988
#define	AR_PHY_BIN_MASK2_2	0x998c
#define	AR_PHY_BIN_MASK2_3	0x9990
#define	AR_PHY_BIN_MASK2_4	0x9994

#define	AR_PHY_BIN_MASK_1	0x9900
#define	AR_PHY_BIN_MASK_2	0x9904
#define	AR_PHY_BIN_MASK_3	0x9908

#define	AR_PHY_MASK_CTL		0x990c

#define	AR_PHY_BIN_MASK2_4_MASK_4	0x00003FFF
#define	AR_PHY_BIN_MASK2_4_MASK_4_S	0

#define	AR_PHY_TIMING9		        0x9998
#define	AR_PHY_TIMING10		        0x999c
#define	AR_PHY_TIMING10_PILOT_MASK_2	0x000FFFFF
#define	AR_PHY_TIMING10_PILOT_MASK_2_S	0

#define	AR_PHY_TIMING11			        0x99a0
#define	AR_PHY_TIMING11_SPUR_DELTA_PHASE	0x000FFFFF
#define	AR_PHY_TIMING11_SPUR_DELTA_PHASE_S	0
#define	AR_PHY_TIMING11_SPUR_FREQ_SD		0x3FF00000
#define	AR_PHY_TIMING11_SPUR_FREQ_SD_S		20
#define AR_PHY_TIMING11_USE_SPUR_IN_AGC		0x40000000
#define AR_PHY_TIMING11_USE_SPUR_IN_SELFCOR	0x80000000

#define AR_PHY_RX_CHAINMASK     0x99a4
#define AR_PHY_NEW_ADC_DC_GAIN_CORR(_i) (0x99b4 + ((_i) << 12))
#define AR_PHY_NEW_ADC_GAIN_CORR_ENABLE 0x40000000
#define AR_PHY_NEW_ADC_DC_OFFSET_CORR_ENABLE 0x80000000
#define AR_PHY_MULTICHAIN_GAIN_CTL  0x99ac

#define AR_PHY_EXT_CCA0             0x99b8
#define AR_PHY_EXT_CCA0_THRESH62    0x000000FF
#define AR_PHY_EXT_CCA0_THRESH62_S  0

#define AR_PHY_EXT_CCA                  0x99bc
#define AR_PHY_EXT_CCA_CYCPWR_THR1      0x0000FE00
#define AR_PHY_EXT_CCA_CYCPWR_THR1_S    9
#define AR_PHY_EXT_CCA_THRESH62         0x007F0000
#define AR_PHY_EXT_CCA_THRESH62_S       16
#define AR_PHY_EXT_MINCCA_PWR           0xFF800000
#define AR_PHY_EXT_MINCCA_PWR_S         23
#define AR9280_PHY_EXT_MINCCA_PWR       0x01FF0000
#define AR9280_PHY_EXT_MINCCA_PWR_S     16

#define AR_PHY_SFCORR_EXT                 0x99c0
#define AR_PHY_SFCORR_EXT_M1_THRESH       0x0000007F
#define AR_PHY_SFCORR_EXT_M1_THRESH_S     0
#define AR_PHY_SFCORR_EXT_M2_THRESH       0x00003F80
#define AR_PHY_SFCORR_EXT_M2_THRESH_S     7
#define AR_PHY_SFCORR_EXT_M1_THRESH_LOW   0x001FC000
#define AR_PHY_SFCORR_EXT_M1_THRESH_LOW_S 14
#define AR_PHY_SFCORR_EXT_M2_THRESH_LOW   0x0FE00000
#define AR_PHY_SFCORR_EXT_M2_THRESH_LOW_S 21
#define AR_PHY_SFCORR_SPUR_SUBCHNL_SD_S   28

#define AR_PHY_HALFGI           0x99D0
#define AR_PHY_HALFGI_DSC_MAN   0x0007FFF0
#define AR_PHY_HALFGI_DSC_MAN_S 4
#define AR_PHY_HALFGI_DSC_EXP   0x0000000F
#define AR_PHY_HALFGI_DSC_EXP_S 0

#define AR_PHY_CHAN_INFO_MEMORY               0x99DC
#define AR_PHY_CHAN_INFO_MEMORY_CAPTURE_MASK  0x0001

#define AR_PHY_HEAVY_CLIP_ENABLE         0x99E0

#define AR_PHY_M_SLEEP      0x99f0
#define AR_PHY_REFCLKDLY    0x99f4
#define AR_PHY_REFCLKPD     0x99f8

#define AR_PHY_CALMODE      0x99f0

#define AR_PHY_CALMODE_IQ           0x00000000
#define AR_PHY_CALMODE_ADC_GAIN     0x00000001
#define AR_PHY_CALMODE_ADC_DC_PER   0x00000002
#define AR_PHY_CALMODE_ADC_DC_INIT  0x00000003

#define AR_PHY_CAL_MEAS_0(_i)     (0x9c10 + ((_i) << 12))
#define AR_PHY_CAL_MEAS_1(_i)     (0x9c14 + ((_i) << 12))
#define AR_PHY_CAL_MEAS_2(_i)     (0x9c18 + ((_i) << 12))
#define AR_PHY_CAL_MEAS_3(_i)     (0x9c1c + ((_i) << 12))

#define AR_PHY_CURRENT_RSSI 0x9c1c
#define AR9280_PHY_CURRENT_RSSI 0x9c3c

#define AR_PHY_RFBUS_GRANT       0x9C20
#define AR_PHY_RFBUS_GRANT_EN    0x00000001

#define AR_PHY_CHAN_INFO_GAIN_DIFF             0x9CF4
#define AR_PHY_CHAN_INFO_GAIN_DIFF_UPPER_LIMIT 320

#define AR_PHY_CHAN_INFO_GAIN          0x9CFC

#define AR_PHY_MODE         0xA200
#define AR_PHY_MODE_AR2133  0x08
#define AR_PHY_MODE_AR5111  0x00
#define AR_PHY_MODE_AR5112  0x08
#define AR_PHY_MODE_DYNAMIC 0x04
#define AR_PHY_MODE_RF2GHZ  0x02
#define AR_PHY_MODE_RF5GHZ  0x00
#define AR_PHY_MODE_CCK     0x01
#define AR_PHY_MODE_OFDM    0x00
#define AR_PHY_MODE_DYN_CCK_DISABLE 0x100

#define AR_PHY_CCK_TX_CTRL       0xA204
#define AR_PHY_CCK_TX_CTRL_JAPAN 0x00000010

#define AR_PHY_CCK_DETECT                           0xA208
#define AR_PHY_CCK_DETECT_WEAK_SIG_THR_CCK          0x0000003F
#define AR_PHY_CCK_DETECT_WEAK_SIG_THR_CCK_S        0
/* [12:6] settling time for antenna switch */
#define AR_PHY_CCK_DETECT_ANT_SWITCH_TIME           0x00001FC0
#define AR_PHY_CCK_DETECT_ANT_SWITCH_TIME_S         6
#define AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV    0x2000

#define AR_PHY_GAIN_2GHZ                0xA20C
#define AR_PHY_GAIN_2GHZ_RXTX_MARGIN    0x00FC0000
#define AR_PHY_GAIN_2GHZ_RXTX_MARGIN_S  18
#define AR_PHY_GAIN_2GHZ_BSW_MARGIN     0x00003C00
#define AR_PHY_GAIN_2GHZ_BSW_MARGIN_S   10
#define AR_PHY_GAIN_2GHZ_BSW_ATTEN      0x0000001F
#define AR_PHY_GAIN_2GHZ_BSW_ATTEN_S    0

#define AR_PHY_GAIN_2GHZ_XATTEN2_MARGIN     0x003E0000
#define AR_PHY_GAIN_2GHZ_XATTEN2_MARGIN_S   17
#define AR_PHY_GAIN_2GHZ_XATTEN1_MARGIN     0x0001F000
#define AR_PHY_GAIN_2GHZ_XATTEN1_MARGIN_S   12
#define AR_PHY_GAIN_2GHZ_XATTEN2_DB         0x00000FC0
#define AR_PHY_GAIN_2GHZ_XATTEN2_DB_S       6
#define AR_PHY_GAIN_2GHZ_XATTEN1_DB         0x0000003F
#define AR_PHY_GAIN_2GHZ_XATTEN1_DB_S       0

#define AR_PHY_CCK_RXCTRL4  0xA21C
#define AR_PHY_CCK_RXCTRL4_FREQ_EST_SHORT   0x01F80000
#define AR_PHY_CCK_RXCTRL4_FREQ_EST_SHORT_S 19

#define AR_PHY_DAG_CTRLCCK  0xA228
#define AR_PHY_DAG_CTRLCCK_EN_RSSI_THR  0x00000200
#define AR_PHY_DAG_CTRLCCK_RSSI_THR     0x0001FC00
#define AR_PHY_DAG_CTRLCCK_RSSI_THR_S   10

#define AR_PHY_FORCE_CLKEN_CCK              0xA22C
#define AR_PHY_FORCE_CLKEN_CCK_MRC_MUX      0x00000040

#define AR_PHY_POWER_TX_RATE3   0xA234
#define AR_PHY_POWER_TX_RATE4   0xA238

#define AR_PHY_SCRM_SEQ_XR       0xA23C
#define AR_PHY_HEADER_DETECT_XR  0xA240
#define AR_PHY_CHIRP_DETECTED_XR 0xA244
#define AR_PHY_BLUETOOTH         0xA254

#define AR_PHY_TPCRG1   0xA258
#define AR_PHY_TPCRG1_NUM_PD_GAIN   0x0000c000
#define AR_PHY_TPCRG1_NUM_PD_GAIN_S 14

#define AR_PHY_TPCRG1_PD_GAIN_1    0x00030000
#define AR_PHY_TPCRG1_PD_GAIN_1_S  16
#define AR_PHY_TPCRG1_PD_GAIN_2    0x000C0000
#define AR_PHY_TPCRG1_PD_GAIN_2_S  18
#define AR_PHY_TPCRG1_PD_GAIN_3    0x00300000
#define AR_PHY_TPCRG1_PD_GAIN_3_S  20

#define AR_PHY_VIT_MASK2_M_46_61 0xa3a0
#define AR_PHY_MASK2_M_31_45     0xa3a4
#define AR_PHY_MASK2_M_16_30     0xa3a8
#define AR_PHY_MASK2_M_00_15     0xa3ac
#define AR_PHY_MASK2_P_15_01     0xa3b8
#define AR_PHY_MASK2_P_30_16     0xa3bc
#define AR_PHY_MASK2_P_45_31     0xa3c0
#define AR_PHY_MASK2_P_61_45     0xa3c4
#define AR_PHY_SPUR_REG          0x994c

#define AR_PHY_SPUR_REG_MASK_RATE_CNTL       (0xFF << 18)
#define AR_PHY_SPUR_REG_MASK_RATE_CNTL_S     18

#define AR_PHY_SPUR_REG_ENABLE_MASK_PPM      0x20000
#define AR_PHY_SPUR_REG_MASK_RATE_SELECT     (0xFF << 9)
#define AR_PHY_SPUR_REG_MASK_RATE_SELECT_S   9
#define AR_PHY_SPUR_REG_ENABLE_VIT_SPUR_RSSI 0x100
#define AR_PHY_SPUR_REG_SPUR_RSSI_THRESH     0x7F
#define AR_PHY_SPUR_REG_SPUR_RSSI_THRESH_S   0

#define AR_PHY_PILOT_MASK_01_30   0xa3b0
#define AR_PHY_PILOT_MASK_31_60   0xa3b4

#define AR_PHY_CHANNEL_MASK_01_30 0x99d4
#define AR_PHY_CHANNEL_MASK_31_60 0x99d8

#define AR_PHY_ANALOG_SWAP      0xa268
#define AR_PHY_SWAP_ALT_CHAIN   0x00000040

#define AR_PHY_TPCRG5   0xA26C
#define AR_PHY_TPCRG5_PD_GAIN_OVERLAP       0x0000000F
#define AR_PHY_TPCRG5_PD_GAIN_OVERLAP_S     0
#define AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_1    0x000003F0
#define AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_1_S  4
#define AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_2    0x0000FC00
#define AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_2_S  10
#define AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_3    0x003F0000
#define AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_3_S  16
#define AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_4    0x0FC00000
#define AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_4_S  22

#define AR_PHY_POWER_TX_RATE5   0xA38C
#define AR_PHY_POWER_TX_RATE6   0xA390

#define AR_PHY_CAL_CHAINMASK    0xA39C

#define AR_PHY_POWER_TX_SUB     0xA3C8
#define AR_PHY_POWER_TX_RATE7   0xA3CC
#define AR_PHY_POWER_TX_RATE8   0xA3D0
#define AR_PHY_POWER_TX_RATE9   0xA3D4

#define AR_PHY_XPA_CFG  	0xA3D8
#define AR_PHY_FORCE_XPA_CFG	0x000000001
#define AR_PHY_FORCE_XPA_CFG_S	0

#define AR_PHY_CH1_CCA          0xa864
#define AR_PHY_CH1_MINCCA_PWR   0x0FF80000
#define AR_PHY_CH1_MINCCA_PWR_S 19
#define AR9280_PHY_CH1_MINCCA_PWR   0x1FF00000
#define AR9280_PHY_CH1_MINCCA_PWR_S 20

#define AR_PHY_CH2_CCA          0xb864
#define AR_PHY_CH2_MINCCA_PWR   0x0FF80000
#define AR_PHY_CH2_MINCCA_PWR_S 19

#define AR_PHY_CH1_EXT_CCA          0xa9bc
#define AR_PHY_CH1_EXT_MINCCA_PWR   0xFF800000
#define AR_PHY_CH1_EXT_MINCCA_PWR_S 23
#define AR9280_PHY_CH1_EXT_MINCCA_PWR   0x01FF0000
#define AR9280_PHY_CH1_EXT_MINCCA_PWR_S 16

#define AR_PHY_CH2_EXT_CCA          0xb9bc
#define AR_PHY_CH2_EXT_MINCCA_PWR   0xFF800000
#define AR_PHY_CH2_EXT_MINCCA_PWR_S 23

#define REG_WRITE_RF_ARRAY(iniarray, regData, regWr) do {               \
		int r;							\
		for (r = 0; r < ((iniarray)->ia_rows); r++) {		\
			REG_WRITE(ah, INI_RA((iniarray), r, 0), (regData)[r]); \
			DPRINTF(ah->ah_sc, ATH_DBG_CHANNEL, \
				"RF 0x%x V 0x%x\n", \
				INI_RA((iniarray), r, 0), (regData)[r]); \
			DO_DELAY(regWr);				\
		}							\
	} while (0)

#define ATH9K_KEY_XOR                 0xaa

#define ATH9K_IS_MIC_ENABLED(ah)					\
	(AH5416(ah)->ah_staId1Defaults & AR_STA_ID1_CRPT_MIC_ENABLE)

#define ANTSWAP_AB 0x0001
#define REDUCE_CHAIN_0 0x00000050
#define REDUCE_CHAIN_1 0x00000051

#define RF_BANK_SETUP(_bank, _iniarray, _col) do {			\
		int i;							\
		for (i = 0; i < (_iniarray)->ia_rows; i++)		\
			(_bank)[i] = INI_RA((_iniarray), i, _col);;	\
	} while (0)

#endif
