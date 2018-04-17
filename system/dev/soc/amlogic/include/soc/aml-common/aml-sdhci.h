// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/protocol/sdmmc.h>
#include <ddk/io-buffer.h>
#include <string.h>

#pragma once

static inline void update_bits(uint32_t *x, uint32_t mask, uint32_t loc, uint32_t val) {
    *x &= ~mask;
    *x |= ((val << loc) & mask);
}

static inline uint32_t get_bits(uint32_t x, uint32_t mask, uint32_t loc) {
    return (x & mask) >> loc;
}

static inline bool get_bit(uint32_t x, uint32_t mask) {
    return (x & mask) ? 1 : 0;
}

//From EMMC Design Logic documentation provided by AMLOGIC
#define AML_SDHCI_IRQ_ALL_CLEAR            0x3fff
#define AML_SDHCI_MIN_FREQ                 400000
#define AML_SDHCI_MAX_FREQ                 40000000
#define DMA_DESC_COUNT                     512
#define CLK_SRC_24MHZ                      24000000 // 24 MHz
#define CLK_SRC_FCLK_DIV2                  1000000000 // 1GHZ

typedef struct {
    volatile uint32_t sd_emmc_clock;        // 0x00
#define SD_EMMC_CLOCK_CFG_DIV_LOC           0
#define SD_EMMC_CLOCK_CFG_DIV_MASK          0x0000003f
#define SD_EMMC_CLOCK_CFG_SRC_LOC           6
#define SD_EMMC_CLOCK_CFG_SRC_MASK          0x000000c0
#define SD_EMMC_CLOCK_CFG_CO_PHASE_LOC      8
#define SD_EMMC_CLOCK_CFG_CO_PHASE_MASK     0x00000300
#define SD_EMMC_CLOCK_CFG_TX_PHASE_LOC      10
#define SD_EMMC_CLOCK_CFG_TX_PHASE_MASK     0x00000c00
#define SD_EMMC_CLOCK_CFG_RX_PHASE_LOC      12
#define SD_EMMC_CLOCK_CFG_RX_PHASE_MASK     0x00003000
#define SD_EMMC_CLOCK_CFG_SRAM_PD_LOC       14
#define SD_EMMC_CLOCK_CFG_SRAM_PD_MASK      0x0000c000
#define SD_EMMC_CLOCK_CFG_TX_DELAY_LOC      16
#define SD_EMMC_CLOCK_CFG_TX_DELAY_MASK     0x003f0000
#define SD_EMMC_CLOCK_CFG_RX_DELAY_LOC      22
#define SD_EMMC_CLOCK_CFG_RX_DELAY_MASK     0x0fc00000
#define SD_EMMC_CLOCK_CFG_ALWAYS_ON         0x10000000
#define SD_EMMC_CLOCK_CFG_IRQ_SDIO_SLEEP    0x20000000
#define SD_EMMC_CLOCK_CFG_IRQ_SDIO_SLEEP_DS 0x40000000
#define SD_EMMC_CLOCK_CFG_NAND              0x80000000

    volatile uint32_t sd_emmc_delay1;       // 0x04
#define SD_EMMC_DELAY_DATA0_LOC             0
#define SD_EMMC_DELAY_DATA0_MASK            0x0000003f
#define SD_EMMC_DELAY_DATA1_LOC             6
#define SD_EMMC_DELAY_DATA1_MASK            0x00000fc0
#define SD_EMMC_DELAY_DATA2_LOC             12
#define SD_EMMC_DELAY_DATA2_MASK            0x0003f000
#define SD_EMMC_DELAY_DATA3_LOC             18
#define SD_EMMC_DELAY_DATA3_MASK            0x00fc0000
#define SD_EMMC_DELAY_DATA4_LOC             24
#define SD_EMMC_DELAY_DATA4_MASK            0x3f000000
#define SD_EMMC_DELAY_SPARE_LOC             30
#define SD_EMMC_DELAY_SPARE_MASK            0xc0000000

    volatile uint32_t sd_emmc_delay2;       //0x08
    volatile uint32_t sd_emmc_adjust;       // 0x0c
    volatile uint32_t sd_emmc_calout;       // 0x10
    volatile uint32_t sd_emmc_calout_v2[3]; // 0x14~0x1c
    volatile uint32_t resvd_test[6];        // 0x20~0x34
    volatile uint32_t sd_emmc_intf3[2];     // 0x38, 0x39
    volatile uint32_t sd_emmc_start;        // 0x40

    volatile uint32_t sd_emmc_cfg;          // 0x44
#define SD_EMMC_CFG_BUS_WIDTH_LOC           0
#define SD_EMMC_CFG_BUS_WIDTH_MASK          0x00000003
#define SD_EMMC_CFG_BUS_WIDTH_1BIT          0x00000000
#define SD_EMMC_CFG_BUS_WIDTH_4BIT          0x00000001
#define SD_EMMC_CFG_BUS_WIDTH_8BIT          0x00000002
#define SD_EMMC_CFG_DDR                     0x00000004
#define SD_EMMC_CFG_DC_UGT                  0x00000008
#define SD_EMMC_CFG_BL_LEN_LOC              4
#define SD_EMMC_CFG_BL_LEN_MASK             0x000000f0
#define SD_EMMC_CFG_RESP_TIMEOUT_LOC        8
#define SD_EMMC_CFG_RESP_TIMEOUT_MASK       0x00000f00
#define SD_EMMC_CFG_RC_CC_LOC               12
#define SD_EMMC_CFG_RC_CC_MASK              0x0000f000
#define SD_EMMC_CFG_OUT_FALL                0x00010000
#define SD_EMMC_CFG_BLK_GAP_IP              0x00020000
#define SD_EMMC_CFG_SDCLK_ALWAYS_ON         0x00040000
#define SD_EMMC_CFG_IGNORE_OWNER            0x00080000
#define SD_EMMC_CFG_CHK_DS                  0x00100000
#define SD_EMMC_CFG_CMD_LOW                 0x00200000
#define SD_EMMC_CFG_STOP_CLK                0x00400000
#define SD_EMMC_CFG_AUTO_CLK                0x00800000
#define SD_EMMC_CFG_TXD_ADD_ERR             0x01000000
#define SD_EMMC_CFG_TXD_RETRY               0x02000000
#define SD_EMMC_CFG_IRQ_DS                  0x04000000
#define SD_EMMC_CFG_ERR_ABORT               0x08000000
#define SD_EMMC_CFG_IP_TXD_ADJ_LOC          28
#define SD_EMMC_CFG_IP_TXD_ADJ_MASK         0xf0000000

    volatile uint32_t sd_emmc_status;       // 0x48
#define SD_EMMC_STATUS_RXD_ERR_LOC          0
#define SD_EMMC_STATUS_RXD_ERR_MASK         0x000000ff
#define SD_EMMC_STATUS_TXD_ERR              0x00000100
#define SD_EMMC_STATUS_DESC_ERR             0x00000200
#define SD_EMMC_STATUS_RESP_ERR             0x00000400
#define SD_EMMC_STATUS_RESP_TIMEOUT         0x00000800
#define SD_EMMC_STATUS_DESC_TIMEOUT         0x00001000
#define SD_EMMC_STATUS_END_OF_CHAIN         0x00002000
#define SD_EMMC_STATUS_RESP_STATUS          0x00004000
#define SD_EMMC_STATUS_IRQ_SDIO             0x00008000
#define SD_EMMC_STATUS_DAT_I_LOC            16
#define SD_EMMC_STATUS_DAT_I_MASK           0x00ff0000
#define SD_EMMC_STATUS_CMD_I                0x01000000
#define SD_EMMC_STATUS_DS                   0x02000000
#define SD_EMMC_STATUS_BUS_FSM_LOC          26
#define SD_EMMC_STATUS_BUS_FSM_MASK         0x3c000000
#define SD_EMMC_STATUS_BUS_DESC_BUSY        0x40000000
#define SD_EMMC_STATUS_BUS_CORE_BUSY        0x80000000

    volatile uint32_t sd_emmc_irq_en;       // 0x4c

    volatile uint32_t sd_emmc_cmd_cfg;      // 0x50
#define SD_EMMC_CMD_INFO_LEN_LOC            0
#define SD_EMMC_CMD_INFO_LEN_MASK           0x000001ff
#define SD_EMMC_CMD_INFO_BLOCK_MODE         0x00000200
#define SD_EMMC_CMD_INFO_R1B                0x00000400
#define SD_EMMC_CMD_INFO_END_OF_CHAIN       0x00000800
#define SD_EMMC_CMD_INFO_TIMEOUT_LOC        12
#define SD_EMMC_CMD_INFO_TIMEOUT_MASK       0x0000f000
#define SD_EMMC_CMD_INFO_NO_RESP            0x00010000
#define SD_EMMC_CMD_INFO_NO_CMD             0x00020000
#define SD_EMMC_CMD_INFO_DATA_IO            0x00040000
#define SD_EMMC_CMD_INFO_DATA_WR            0x00080000
#define SD_EMMC_CMD_INFO_RESP_NO_CRC        0x00100000
#define SD_EMMC_CMD_INFO_RESP_128           0x00200000
#define SD_EMMC_CMD_INFO_RESP_NUM           0x00400000
#define SD_EMMC_CMD_INFO_DATA_NUM           0x00800000
#define SD_EMMC_CMD_INFO_CMD_IDX_LOC        24
#define SD_EMMC_CMD_INFO_CMD_IDX_MASK       0x3f000000
#define SD_EMMC_CMD_INFO_ERROR              0x40000000
#define SD_EMMC_CMD_INFO_OWNER              0x80000000

    volatile uint32_t sd_emmc_cmd_arg;      // 0x54
    volatile uint32_t sd_emmc_cmd_dat;      // 0x58
    volatile uint32_t sd_emmc_cmd_rsp;      // 0x5c
    volatile uint32_t sd_emmc_cmd_rsp1;     // 0x60
    volatile uint32_t sd_emmc_cmd_rsp2;     // 0x64
    volatile uint32_t sd_emmc_cmd_rsp3;     // 0x68
    volatile uint32_t bus_err;              // 0x6c
    volatile uint32_t sd_emmc_curr_cfg;     // 0x70
    volatile uint32_t sd_emmc_curr_arg;     // 0x74
    volatile uint32_t sd_emmc_curr_dat;     // 0x78
    volatile uint32_t sd_emmc_curr_rsp;     // 0x7c
    volatile uint32_t sd_emmc_next_cfg;     // 0x80
    volatile uint32_t sd_emmc_next_arg;     // 0x84
    volatile uint32_t sd_emmc_next_dat;     // 0x88
    volatile uint32_t sd_emmc_next_rsp;     // 0x8c
    volatile uint32_t sd_emmc_rxd;          // 0x90
    volatile uint32_t sd_emmc_txd;          // 0x94
    volatile uint32_t resvd[90];            // 0x98~0x1fc
    volatile uint32_t sramDesc[128];        // 0x200
    volatile uint32_t ping[128];            // 0x400
    volatile uint32_t pong[128];            // 0x800
} aml_sdhci_regs_t;

typedef struct {
    uint32_t cmd_info;
    uint32_t cmd_arg;
    uint32_t data_addr;
    uint32_t resp_addr;
} aml_sdhci_desc_t;


typedef struct aml_sdhci_t {
    platform_device_protocol_t pdev;
    zx_device_t* zxdev;
    gpio_protocol_t gpio;
    uint32_t gpio_count;

    io_buffer_t mmio;
    zx_handle_t bti;
    io_buffer_t desc_buffer;

    uint64_t regs_size;
    zx_handle_t regs_handle;

    // Controller info
    sdmmc_host_info_t info;
    uint32_t f_max;
    uint32_t f_min;

    zx_handle_t irq_handle;
} aml_sdhci_t;
