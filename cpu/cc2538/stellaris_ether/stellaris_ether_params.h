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
 * @brief       Parameters for the netdev interface for Stellaris Ethernet MAC module
 *
 * @author      Ho Nghia Duc <nghiaducnt@gmail.com>
 */

#ifndef STELLARIS_ETH_PARAMS_H
#define STELLARIS_ETH_PARAMS_H

#if defined(MODULE_STELLARIS_ETHER) || defined(DOXYGEN)

/**
 * @name    Set default configuration parameters for the Stellaris ethernet netdev driver
 * @{
 */
#define STELLARIS_ETH_STACKSIZE    THREAD_STACKSIZE_DEFAULT
#define STELLARIS_ETH_PRIO         GNRC_NETIF_PRIO
#define STELLARIS_IRQ_PRIO	   1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* MODULE_STELLARIS_ETHER || DOXYGEN */

#endif /* STELLARIS_ETH_PARAMS_H */
/**@}*/
