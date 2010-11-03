// ------------------------------------------------------------------
// Copyright (c) 2004-2010 Atheros Corporation.  All rights reserved.
// 
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
//
// ------------------------------------------------------------------
//===================================================================
// Author(s): ="Atheros"
//===================================================================


#ifndef _SI_REG_REG_H_
#define _SI_REG_REG_H_

#define SI_CONFIG_ADDRESS                        0x00000000
#define SI_CONFIG_OFFSET                         0x00000000
#define SI_CONFIG_ERR_INT_MSB                    19
#define SI_CONFIG_ERR_INT_LSB                    19
#define SI_CONFIG_ERR_INT_MASK                   0x00080000
#define SI_CONFIG_ERR_INT_GET(x)                 (((x) & SI_CONFIG_ERR_INT_MASK) >> SI_CONFIG_ERR_INT_LSB)
#define SI_CONFIG_ERR_INT_SET(x)                 (((x) << SI_CONFIG_ERR_INT_LSB) & SI_CONFIG_ERR_INT_MASK)
#define SI_CONFIG_BIDIR_OD_DATA_MSB              18
#define SI_CONFIG_BIDIR_OD_DATA_LSB              18
#define SI_CONFIG_BIDIR_OD_DATA_MASK             0x00040000
#define SI_CONFIG_BIDIR_OD_DATA_GET(x)           (((x) & SI_CONFIG_BIDIR_OD_DATA_MASK) >> SI_CONFIG_BIDIR_OD_DATA_LSB)
#define SI_CONFIG_BIDIR_OD_DATA_SET(x)           (((x) << SI_CONFIG_BIDIR_OD_DATA_LSB) & SI_CONFIG_BIDIR_OD_DATA_MASK)
#define SI_CONFIG_I2C_MSB                        16
#define SI_CONFIG_I2C_LSB                        16
#define SI_CONFIG_I2C_MASK                       0x00010000
#define SI_CONFIG_I2C_GET(x)                     (((x) & SI_CONFIG_I2C_MASK) >> SI_CONFIG_I2C_LSB)
#define SI_CONFIG_I2C_SET(x)                     (((x) << SI_CONFIG_I2C_LSB) & SI_CONFIG_I2C_MASK)
#define SI_CONFIG_POS_SAMPLE_MSB                 7
#define SI_CONFIG_POS_SAMPLE_LSB                 7
#define SI_CONFIG_POS_SAMPLE_MASK                0x00000080
#define SI_CONFIG_POS_SAMPLE_GET(x)              (((x) & SI_CONFIG_POS_SAMPLE_MASK) >> SI_CONFIG_POS_SAMPLE_LSB)
#define SI_CONFIG_POS_SAMPLE_SET(x)              (((x) << SI_CONFIG_POS_SAMPLE_LSB) & SI_CONFIG_POS_SAMPLE_MASK)
#define SI_CONFIG_POS_DRIVE_MSB                  6
#define SI_CONFIG_POS_DRIVE_LSB                  6
#define SI_CONFIG_POS_DRIVE_MASK                 0x00000040
#define SI_CONFIG_POS_DRIVE_GET(x)               (((x) & SI_CONFIG_POS_DRIVE_MASK) >> SI_CONFIG_POS_DRIVE_LSB)
#define SI_CONFIG_POS_DRIVE_SET(x)               (((x) << SI_CONFIG_POS_DRIVE_LSB) & SI_CONFIG_POS_DRIVE_MASK)
#define SI_CONFIG_INACTIVE_DATA_MSB              5
#define SI_CONFIG_INACTIVE_DATA_LSB              5
#define SI_CONFIG_INACTIVE_DATA_MASK             0x00000020
#define SI_CONFIG_INACTIVE_DATA_GET(x)           (((x) & SI_CONFIG_INACTIVE_DATA_MASK) >> SI_CONFIG_INACTIVE_DATA_LSB)
#define SI_CONFIG_INACTIVE_DATA_SET(x)           (((x) << SI_CONFIG_INACTIVE_DATA_LSB) & SI_CONFIG_INACTIVE_DATA_MASK)
#define SI_CONFIG_INACTIVE_CLK_MSB               4
#define SI_CONFIG_INACTIVE_CLK_LSB               4
#define SI_CONFIG_INACTIVE_CLK_MASK              0x00000010
#define SI_CONFIG_INACTIVE_CLK_GET(x)            (((x) & SI_CONFIG_INACTIVE_CLK_MASK) >> SI_CONFIG_INACTIVE_CLK_LSB)
#define SI_CONFIG_INACTIVE_CLK_SET(x)            (((x) << SI_CONFIG_INACTIVE_CLK_LSB) & SI_CONFIG_INACTIVE_CLK_MASK)
#define SI_CONFIG_DIVIDER_MSB                    3
#define SI_CONFIG_DIVIDER_LSB                    0
#define SI_CONFIG_DIVIDER_MASK                   0x0000000f
#define SI_CONFIG_DIVIDER_GET(x)                 (((x) & SI_CONFIG_DIVIDER_MASK) >> SI_CONFIG_DIVIDER_LSB)
#define SI_CONFIG_DIVIDER_SET(x)                 (((x) << SI_CONFIG_DIVIDER_LSB) & SI_CONFIG_DIVIDER_MASK)

#define SI_CS_ADDRESS                            0x00000004
#define SI_CS_OFFSET                             0x00000004
#define SI_CS_BIT_CNT_IN_LAST_BYTE_MSB           13
#define SI_CS_BIT_CNT_IN_LAST_BYTE_LSB           11
#define SI_CS_BIT_CNT_IN_LAST_BYTE_MASK          0x00003800
#define SI_CS_BIT_CNT_IN_LAST_BYTE_GET(x)        (((x) & SI_CS_BIT_CNT_IN_LAST_BYTE_MASK) >> SI_CS_BIT_CNT_IN_LAST_BYTE_LSB)
#define SI_CS_BIT_CNT_IN_LAST_BYTE_SET(x)        (((x) << SI_CS_BIT_CNT_IN_LAST_BYTE_LSB) & SI_CS_BIT_CNT_IN_LAST_BYTE_MASK)
#define SI_CS_DONE_ERR_MSB                       10
#define SI_CS_DONE_ERR_LSB                       10
#define SI_CS_DONE_ERR_MASK                      0x00000400
#define SI_CS_DONE_ERR_GET(x)                    (((x) & SI_CS_DONE_ERR_MASK) >> SI_CS_DONE_ERR_LSB)
#define SI_CS_DONE_ERR_SET(x)                    (((x) << SI_CS_DONE_ERR_LSB) & SI_CS_DONE_ERR_MASK)
#define SI_CS_DONE_INT_MSB                       9
#define SI_CS_DONE_INT_LSB                       9
#define SI_CS_DONE_INT_MASK                      0x00000200
#define SI_CS_DONE_INT_GET(x)                    (((x) & SI_CS_DONE_INT_MASK) >> SI_CS_DONE_INT_LSB)
#define SI_CS_DONE_INT_SET(x)                    (((x) << SI_CS_DONE_INT_LSB) & SI_CS_DONE_INT_MASK)
#define SI_CS_START_MSB                          8
#define SI_CS_START_LSB                          8
#define SI_CS_START_MASK                         0x00000100
#define SI_CS_START_GET(x)                       (((x) & SI_CS_START_MASK) >> SI_CS_START_LSB)
#define SI_CS_START_SET(x)                       (((x) << SI_CS_START_LSB) & SI_CS_START_MASK)
#define SI_CS_RX_CNT_MSB                         7
#define SI_CS_RX_CNT_LSB                         4
#define SI_CS_RX_CNT_MASK                        0x000000f0
#define SI_CS_RX_CNT_GET(x)                      (((x) & SI_CS_RX_CNT_MASK) >> SI_CS_RX_CNT_LSB)
#define SI_CS_RX_CNT_SET(x)                      (((x) << SI_CS_RX_CNT_LSB) & SI_CS_RX_CNT_MASK)
#define SI_CS_TX_CNT_MSB                         3
#define SI_CS_TX_CNT_LSB                         0
#define SI_CS_TX_CNT_MASK                        0x0000000f
#define SI_CS_TX_CNT_GET(x)                      (((x) & SI_CS_TX_CNT_MASK) >> SI_CS_TX_CNT_LSB)
#define SI_CS_TX_CNT_SET(x)                      (((x) << SI_CS_TX_CNT_LSB) & SI_CS_TX_CNT_MASK)

#define SI_TX_DATA0_ADDRESS                      0x00000008
#define SI_TX_DATA0_OFFSET                       0x00000008
#define SI_TX_DATA0_DATA3_MSB                    31
#define SI_TX_DATA0_DATA3_LSB                    24
#define SI_TX_DATA0_DATA3_MASK                   0xff000000
#define SI_TX_DATA0_DATA3_GET(x)                 (((x) & SI_TX_DATA0_DATA3_MASK) >> SI_TX_DATA0_DATA3_LSB)
#define SI_TX_DATA0_DATA3_SET(x)                 (((x) << SI_TX_DATA0_DATA3_LSB) & SI_TX_DATA0_DATA3_MASK)
#define SI_TX_DATA0_DATA2_MSB                    23
#define SI_TX_DATA0_DATA2_LSB                    16
#define SI_TX_DATA0_DATA2_MASK                   0x00ff0000
#define SI_TX_DATA0_DATA2_GET(x)                 (((x) & SI_TX_DATA0_DATA2_MASK) >> SI_TX_DATA0_DATA2_LSB)
#define SI_TX_DATA0_DATA2_SET(x)                 (((x) << SI_TX_DATA0_DATA2_LSB) & SI_TX_DATA0_DATA2_MASK)
#define SI_TX_DATA0_DATA1_MSB                    15
#define SI_TX_DATA0_DATA1_LSB                    8
#define SI_TX_DATA0_DATA1_MASK                   0x0000ff00
#define SI_TX_DATA0_DATA1_GET(x)                 (((x) & SI_TX_DATA0_DATA1_MASK) >> SI_TX_DATA0_DATA1_LSB)
#define SI_TX_DATA0_DATA1_SET(x)                 (((x) << SI_TX_DATA0_DATA1_LSB) & SI_TX_DATA0_DATA1_MASK)
#define SI_TX_DATA0_DATA0_MSB                    7
#define SI_TX_DATA0_DATA0_LSB                    0
#define SI_TX_DATA0_DATA0_MASK                   0x000000ff
#define SI_TX_DATA0_DATA0_GET(x)                 (((x) & SI_TX_DATA0_DATA0_MASK) >> SI_TX_DATA0_DATA0_LSB)
#define SI_TX_DATA0_DATA0_SET(x)                 (((x) << SI_TX_DATA0_DATA0_LSB) & SI_TX_DATA0_DATA0_MASK)

#define SI_TX_DATA1_ADDRESS                      0x0000000c
#define SI_TX_DATA1_OFFSET                       0x0000000c
#define SI_TX_DATA1_DATA7_MSB                    31
#define SI_TX_DATA1_DATA7_LSB                    24
#define SI_TX_DATA1_DATA7_MASK                   0xff000000
#define SI_TX_DATA1_DATA7_GET(x)                 (((x) & SI_TX_DATA1_DATA7_MASK) >> SI_TX_DATA1_DATA7_LSB)
#define SI_TX_DATA1_DATA7_SET(x)                 (((x) << SI_TX_DATA1_DATA7_LSB) & SI_TX_DATA1_DATA7_MASK)
#define SI_TX_DATA1_DATA6_MSB                    23
#define SI_TX_DATA1_DATA6_LSB                    16
#define SI_TX_DATA1_DATA6_MASK                   0x00ff0000
#define SI_TX_DATA1_DATA6_GET(x)                 (((x) & SI_TX_DATA1_DATA6_MASK) >> SI_TX_DATA1_DATA6_LSB)
#define SI_TX_DATA1_DATA6_SET(x)                 (((x) << SI_TX_DATA1_DATA6_LSB) & SI_TX_DATA1_DATA6_MASK)
#define SI_TX_DATA1_DATA5_MSB                    15
#define SI_TX_DATA1_DATA5_LSB                    8
#define SI_TX_DATA1_DATA5_MASK                   0x0000ff00
#define SI_TX_DATA1_DATA5_GET(x)                 (((x) & SI_TX_DATA1_DATA5_MASK) >> SI_TX_DATA1_DATA5_LSB)
#define SI_TX_DATA1_DATA5_SET(x)                 (((x) << SI_TX_DATA1_DATA5_LSB) & SI_TX_DATA1_DATA5_MASK)
#define SI_TX_DATA1_DATA4_MSB                    7
#define SI_TX_DATA1_DATA4_LSB                    0
#define SI_TX_DATA1_DATA4_MASK                   0x000000ff
#define SI_TX_DATA1_DATA4_GET(x)                 (((x) & SI_TX_DATA1_DATA4_MASK) >> SI_TX_DATA1_DATA4_LSB)
#define SI_TX_DATA1_DATA4_SET(x)                 (((x) << SI_TX_DATA1_DATA4_LSB) & SI_TX_DATA1_DATA4_MASK)

#define SI_RX_DATA0_ADDRESS                      0x00000010
#define SI_RX_DATA0_OFFSET                       0x00000010
#define SI_RX_DATA0_DATA3_MSB                    31
#define SI_RX_DATA0_DATA3_LSB                    24
#define SI_RX_DATA0_DATA3_MASK                   0xff000000
#define SI_RX_DATA0_DATA3_GET(x)                 (((x) & SI_RX_DATA0_DATA3_MASK) >> SI_RX_DATA0_DATA3_LSB)
#define SI_RX_DATA0_DATA3_SET(x)                 (((x) << SI_RX_DATA0_DATA3_LSB) & SI_RX_DATA0_DATA3_MASK)
#define SI_RX_DATA0_DATA2_MSB                    23
#define SI_RX_DATA0_DATA2_LSB                    16
#define SI_RX_DATA0_DATA2_MASK                   0x00ff0000
#define SI_RX_DATA0_DATA2_GET(x)                 (((x) & SI_RX_DATA0_DATA2_MASK) >> SI_RX_DATA0_DATA2_LSB)
#define SI_RX_DATA0_DATA2_SET(x)                 (((x) << SI_RX_DATA0_DATA2_LSB) & SI_RX_DATA0_DATA2_MASK)
#define SI_RX_DATA0_DATA1_MSB                    15
#define SI_RX_DATA0_DATA1_LSB                    8
#define SI_RX_DATA0_DATA1_MASK                   0x0000ff00
#define SI_RX_DATA0_DATA1_GET(x)                 (((x) & SI_RX_DATA0_DATA1_MASK) >> SI_RX_DATA0_DATA1_LSB)
#define SI_RX_DATA0_DATA1_SET(x)                 (((x) << SI_RX_DATA0_DATA1_LSB) & SI_RX_DATA0_DATA1_MASK)
#define SI_RX_DATA0_DATA0_MSB                    7
#define SI_RX_DATA0_DATA0_LSB                    0
#define SI_RX_DATA0_DATA0_MASK                   0x000000ff
#define SI_RX_DATA0_DATA0_GET(x)                 (((x) & SI_RX_DATA0_DATA0_MASK) >> SI_RX_DATA0_DATA0_LSB)
#define SI_RX_DATA0_DATA0_SET(x)                 (((x) << SI_RX_DATA0_DATA0_LSB) & SI_RX_DATA0_DATA0_MASK)

#define SI_RX_DATA1_ADDRESS                      0x00000014
#define SI_RX_DATA1_OFFSET                       0x00000014
#define SI_RX_DATA1_DATA7_MSB                    31
#define SI_RX_DATA1_DATA7_LSB                    24
#define SI_RX_DATA1_DATA7_MASK                   0xff000000
#define SI_RX_DATA1_DATA7_GET(x)                 (((x) & SI_RX_DATA1_DATA7_MASK) >> SI_RX_DATA1_DATA7_LSB)
#define SI_RX_DATA1_DATA7_SET(x)                 (((x) << SI_RX_DATA1_DATA7_LSB) & SI_RX_DATA1_DATA7_MASK)
#define SI_RX_DATA1_DATA6_MSB                    23
#define SI_RX_DATA1_DATA6_LSB                    16
#define SI_RX_DATA1_DATA6_MASK                   0x00ff0000
#define SI_RX_DATA1_DATA6_GET(x)                 (((x) & SI_RX_DATA1_DATA6_MASK) >> SI_RX_DATA1_DATA6_LSB)
#define SI_RX_DATA1_DATA6_SET(x)                 (((x) << SI_RX_DATA1_DATA6_LSB) & SI_RX_DATA1_DATA6_MASK)
#define SI_RX_DATA1_DATA5_MSB                    15
#define SI_RX_DATA1_DATA5_LSB                    8
#define SI_RX_DATA1_DATA5_MASK                   0x0000ff00
#define SI_RX_DATA1_DATA5_GET(x)                 (((x) & SI_RX_DATA1_DATA5_MASK) >> SI_RX_DATA1_DATA5_LSB)
#define SI_RX_DATA1_DATA5_SET(x)                 (((x) << SI_RX_DATA1_DATA5_LSB) & SI_RX_DATA1_DATA5_MASK)
#define SI_RX_DATA1_DATA4_MSB                    7
#define SI_RX_DATA1_DATA4_LSB                    0
#define SI_RX_DATA1_DATA4_MASK                   0x000000ff
#define SI_RX_DATA1_DATA4_GET(x)                 (((x) & SI_RX_DATA1_DATA4_MASK) >> SI_RX_DATA1_DATA4_LSB)
#define SI_RX_DATA1_DATA4_SET(x)                 (((x) << SI_RX_DATA1_DATA4_LSB) & SI_RX_DATA1_DATA4_MASK)


#ifndef __ASSEMBLER__

typedef struct si_reg_reg_s {
  volatile unsigned int si_config;
  volatile unsigned int si_cs;
  volatile unsigned int si_tx_data0;
  volatile unsigned int si_tx_data1;
  volatile unsigned int si_rx_data0;
  volatile unsigned int si_rx_data1;
} si_reg_reg_t;

#endif /* __ASSEMBLER__ */

#endif /* _SI_REG_H_ */
