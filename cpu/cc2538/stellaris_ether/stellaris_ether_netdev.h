/*
 * Copyright (C) 2018 Gunar Schorcht
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_cc2538
 * @{
 *
 * @file
 * @brief       Netdev interface for the Stellaris Ethernet MAC module
 *
 * @author      Ho Nghia Duc <nghiaducnt@gmail.com>
 */

#ifndef STELLARIS_ETH_NETDEV_H
#define STELLARIS_ETH_NETDEV_H

#include "net/netdev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Device descriptor for Stellaris devices
 */
typedef struct
{
    netdev_t netdev;                    /**< netdev parent struct */

    size_t rx_len;                     /**< number of bytes received */
    size_t tx_len;                     /**< number of bytes in transmit buffer */

    uint8_t  rx_buf[ETHERNET_DATA_LEN];  /**< receive buffer */
    uint8_t  tx_buf[ETHERNET_DATA_LEN];  /**< transmit buffer */
    uint8_t  addr[ETHERNET_ADDR_LEN];
    bool     link_up;                   /**< indicates whether link is up */
    uint32_t irq;			/* irq setting */
    uint32_t irq_num;
    gnrc_netif_t* netif;                /**< reference to the corresponding netif */

    mutex_t dev_lock;                   /**< device is already in use */

} stellaris_eth_netdev_t;

#ifdef __cplusplus
}
#endif

#endif /* STELLARIS_ETH_NETDEV_H */
/** @} */
