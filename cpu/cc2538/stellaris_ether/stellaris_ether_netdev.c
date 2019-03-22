/*
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_cc2538
 * @{
 *
 * @file
 * @brief       Netdev interface for the Stellaris Ethernet module
 *
 * @author      Ho Nghia Duc <nghiaducnt@gmail.com>
 */

#define ENABLE_DEBUG (0)
#include "debug.h"
#include "log.h"

#include <string.h>
#include <assert.h>
#include <errno.h>

#include "net/gnrc/netif/ethernet.h"
#include "net/gnrc.h"
#include "net/ethernet.h"
#include "net/netdev/eth.h"

#include "cpu.h"
#include "stellaris_ether_params.h"
#include "stellaris_ether_netdev.h"

#include "inc/hw_types.h"
#include "inc/hw_ethernet.h"
#include "ethernet.h"

#define IN_EMULATION 1
/* Private Stellaris Ethernet data */
static stellaris_eth_netdev_t priv_stellaris_eth_dev;

/* device thread stack */
static char priv_stellaris_eth_stack[STELLARIS_ETH_STACKSIZE];

static inline void _irq_enable(void)
{
    IRQn_Type irqn = ETHERNET_IRQn;

    NVIC_EnableIRQ(irqn);
}
#if 0
/* not use for now */
static inline void _irq_disable(void)
{
    IRQn_Type irqn = ETHERNET_IRQn;

    NVIC_DisableIRQ(irqn);
}
#endif
static int stellaris_eth_init(netdev_t *netdev)
{
    DEBUG("%s: netdev=%p\n", __func__, netdev);

    stellaris_eth_netdev_t* dev = (stellaris_eth_netdev_t*)netdev;

    mutex_lock(&dev->dev_lock);
    EthernetInitExpClk(ETH_BASE, 0);
    EthernetConfigSet(ETH_BASE, (ETH_CFG_TX_DPLXEN | ETH_CFG_TX_PADEN)); 
    /* enable interrupt, for link status */
    dev->irq = ETH_INT_PHY;
    EthernetEnable(ETH_BASE);
    EthernetIntEnable(ETH_BASE, dev->irq);
    #ifdef MODULE_NETSTATS_L2
    memset(&netdev->stats, 0, sizeof(netstats_t));
    #endif

    mutex_unlock(&dev->dev_lock);
#ifdef IN_EMULATION
    priv_stellaris_eth_dev.link_up = 1;
#endif
    IRQn_Type irqn = ETHERNET_IRQn;

    NVIC_SetPriority(irqn, STELLARIS_IRQ_PRIO);
    _irq_enable();
    return 0;
}

static int stellaris_eth_send(netdev_t *netdev, const iolist_t *iolist)
{
    DEBUG("%s: netdev=%p iolist=%p\n", __func__, netdev, iolist);
    assert(netdev != NULL);
    assert(iolist != NULL);

    stellaris_eth_netdev_t* dev = (stellaris_eth_netdev_t*)netdev;
    if (!priv_stellaris_eth_dev.link_up) {
        DEBUG("%s: link is down\n", __func__);
        return -ENODEV;
    }
    mutex_lock(&dev->dev_lock);

    dev->tx_len = 0;

    /* load packet data into TX buffer */
    for (const iolist_t *iol = iolist; iol; iol = iol->iol_next) {
        if (dev->tx_len + iol->iol_len > ETHERNET_DATA_LEN) {
            mutex_unlock(&dev->dev_lock);
            return -EOVERFLOW;
        }
	memcpy (dev->tx_buf + dev->tx_len, iol->iol_base, iol->iol_len);
        dev->tx_len += iol->iol_len;
    }

    #if ENABLE_DEBUG
    printf ("%s: send %d byte\n", __func__, dev->tx_len);
    #endif
    /* send the the packet to the peer(s) mac address */
    EthernetPacketPutNonBlocking(ETH_BASE, dev->tx_buf, dev->tx_len);
    mutex_unlock(&dev->dev_lock);
    return 0;
}

static int stellaris_eth_recv(netdev_t *netdev, void *buf, size_t len, void *info)
{
    DEBUG("%s: netdev=%p buf=%p len=%u info=%p\n",
          __func__, netdev, buf, len, info);
    int size;
    assert(netdev != NULL);

    stellaris_eth_netdev_t* dev = (stellaris_eth_netdev_t*)netdev;

    mutex_lock(&dev->dev_lock);


    /* copy received date and reset the receive length */
    /* if buf and len are 0, then just get length */
    if (!buf) {
        size = EthernetPacketGet(ETH_BASE, dev->rx_buf, ETHERNET_DATA_LEN);
        dev->rx_len = size;
        /* get the size of the frame; if len > 0 then also drop the frame */
        if (len > 0) {
            /* drop frame requested */
            dev->rx_len = 0;
        }
        mutex_unlock(&dev->dev_lock);
        return size;
    }

    if (dev->rx_len > len) {
        /* buffer is smaller than the number of received bytes */
        DEBUG("%s: Not enough space in receive buffer for %d bytes\n",
              __func__, dev->rx_len);
        mutex_unlock(&dev->dev_lock);
        return -ENOBUFS;
    }
    /* hacker? */
    if (len > dev->rx_len)
	len = dev->rx_len;
    size = len;
    memcpy(buf, dev->rx_buf, len);
    /* data has been copied, ready for next data */
    dev->rx_len = 0;

    #ifdef MODULE_NETSTATS_L2
    netdev->stats.rx_count++;
    netdev->stats.rx_bytes += len;
    #endif

    mutex_unlock(&dev->dev_lock);
    return size;
}

static int stellaris_eth_get(netdev_t *netdev, netopt_t opt, void *val, size_t max_len)
{
    DEBUG("%s: netdev=%p opt=%s val=%p len=%u\n",
          __func__, netdev, netopt2str(opt), val, max_len);

    assert(netdev != NULL);
    assert(val != NULL);

    switch (opt) {
	case NETOPT_DEVICE_TYPE:
	    return NETDEV_TYPE_ETHERNET;
        case NETOPT_ADDRESS:
            assert(max_len == ETHERNET_ADDR_LEN);
	    EthernetMACAddrGet(ETH_BASE, (unsigned char *)val);
            return ETHERNET_ADDR_LEN;
        default:
            return netdev_eth_get(netdev, opt, val, max_len);
    }
    return 0;
}

static int stellaris_eth_set(netdev_t *netdev, netopt_t opt, const void *val, size_t max_len)
{
    DEBUG("%s: netdev=%p opt=%s val=%p len=%u\n",
          __func__, netdev, netopt2str(opt), val, max_len);

    assert(netdev != NULL);
    assert(val != NULL);

    stellaris_eth_netdev_t* dev = (stellaris_eth_netdev_t*)netdev;
    switch (opt) {
        case NETOPT_ADDRESS:
            assert(max_len == ETHERNET_ADDR_LEN);
	    EthernetMACAddrSet(ETH_BASE, (unsigned char *)val);
            return ETHERNET_ADDR_LEN;
        case NETOPT_TX_END_IRQ:
	{
	    uint32_t set = *(uint32_t *)val;
	    uint32_t irq = ETH_INT_TXER | ETH_INT_TX; 
	    if (set) {
	    	dev->irq |= irq;
    	    	EthernetIntEnable(ETH_BASE, irq);
	    } else {
	    	dev->irq &= (~irq);
    	    	EthernetIntClear(ETH_BASE, irq);
	    }
	    return 1;
	}
	case NETOPT_RX_END_IRQ:
	{
	    uint32_t set = *(uint32_t *)val;
	    uint32_t irq = ETH_INT_RXER | ETH_INT_RXOF;
	    if (set) {
	    	dev->irq |= irq;
    	    	EthernetIntEnable(ETH_BASE, irq);
	    } else {
	    	dev->irq &= (~irq);
    	    	EthernetIntClear(ETH_BASE, irq);
	    }
	    return 1;
	}
        default:
            return netdev_eth_set(netdev, opt, val, max_len);
    }
    return 0;
}

static void stellaris_eth_isr(netdev_t *netdev)
{
    DEBUG("%s: netdev=%p\n", __func__, netdev);

    assert(netdev != NULL);
    stellaris_eth_netdev_t *dev = (stellaris_eth_netdev_t *) netdev;
    dev->irq_num++;
    return;
}

static const netdev_driver_t stellaris_eth_driver =
{
    .send = stellaris_eth_send,
    .recv = stellaris_eth_recv,
    .init = stellaris_eth_init,
    .isr = stellaris_eth_isr,
    .get = stellaris_eth_get,
    .set = stellaris_eth_set,
};

void auto_init_stellaris_eth (void)
{
    /* initialize locking */
    mutex_init(&priv_stellaris_eth_dev.dev_lock);

    /* set the netdev driver */
    priv_stellaris_eth_dev.netdev.driver = &stellaris_eth_driver;

    /* initialize netdev data structure */
    //priv_stellaris_eth_dev.event = SYSTEM_EVENT_MAX; /* no event */
    priv_stellaris_eth_dev.link_up = false;
    priv_stellaris_eth_dev.rx_len = 0;
    priv_stellaris_eth_dev.tx_len = 0;
    priv_stellaris_eth_dev.netif = gnrc_netif_ethernet_create(priv_stellaris_eth_stack,
                                                   STELLARIS_ETH_STACKSIZE,
                                                   STELLARIS_ETH_PRIO,
                                                   "netdev-stellaris-eth",
                                                   (netdev_t *)&priv_stellaris_eth_dev);
}

/* IRQ handler */
void isr_ethernet(void)
{
	unsigned long irq_status = EthernetIntStatus(ETH_BASE, 0);
	netdev_t *netdev = &priv_stellaris_eth_dev.netdev;
	EthernetIntClear(ETH_BASE, irq_status);
	if (irq_status & ETH_INT_PHY) {
		/* check link status */
		uint16_t mr1 = EthernetPHYRead(ETH_BASE, PHY_MR1);
		if (mr1 & PHY_MR1_LINK) {
			priv_stellaris_eth_dev.link_up = true;
        		netdev->event_callback(netdev,
				NETDEV_EVENT_LINK_UP);
		} else {
			priv_stellaris_eth_dev.link_up = false;
        		netdev->event_callback(netdev,
				NETDEV_EVENT_LINK_DOWN);
		}
			
	}
	if (irq_status & ETH_INT_RXOF || irq_status & ETH_INT_RXER) {
        	netdev->event_callback(netdev,
			NETDEV_EVENT_RX_TIMEOUT);
	}
	if (irq_status & ETH_INT_RX) {
        	netdev->event_callback(netdev,
			NETDEV_EVENT_RX_COMPLETE);
	}
    	if (irq_status & ETH_INT_TX) {
        	#ifdef MODULE_NETSTATS_L2
        	netdev->stats.tx_success++;
        	netdev->stats.tx_bytes += priv_stellaris_eth_dev.tx_len;
        	#endif
        	netdev->event_callback(netdev, NETDEV_EVENT_TX_COMPLETE);
    	}
	if (irq_status & ETH_INT_TXER) {
        	#ifdef MODULE_NETSTATS_L2
        	netdev->stats.tx_failed++;
        	#endif
        	netdev->event_callback(netdev, NETDEV_EVENT_TX_TIMEOUT);
    	}
        netdev->event_callback(netdev, NETDEV_EVENT_ISR);
    	cortexm_isr_end();
}
