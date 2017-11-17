/*
 * Copyright (C) 2017 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author  Martine Lenders <m.lenders@fu-berlin.de>
 */

#include "net/gnrc/netif/internal.h"
#include "net/gnrc/ipv6/nib.h"
#include "net/gnrc/ndp.h"

#include "_nib-6ln.h"
#include "_nib-6lr.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#if GNRC_IPV6_NIB_CONF_6LN
#if ENABLE_DEBUG
static char addr_str[IPV6_ADDR_MAX_STR_LEN];
#endif

extern void _handle_search_rtr(gnrc_netif_t *netif);

static inline bool _is_iface_eui64(gnrc_netif_t *netif, const eui64_t *eui64)
{
    /* TODO: adapt for short addresses */
    return (netif->l2addr_len == sizeof(eui64_t)) &&
           (memcmp(&netif->l2addr, eui64, netif->l2addr_len) == 0);
}

static inline uint8_t _reverse_iid(const ipv6_addr_t *dst,
                                   const gnrc_netif_t *netif, uint8_t *l2addr)
{
    switch (netif->device_type) {
#ifdef MODULE_NETDEV_ETH
        case NETDEV_TYPE_ETHERNET:
            l2addr[0] = dst->u8[8] ^ 0x02;
            l2addr[1] = dst->u8[9];
            l2addr[2] = dst->u8[10];
            l2addr[3] = dst->u8[13];
            l2addr[4] = dst->u8[14];
            l2addr[5] = dst->u8[15];
            return ETHERNET_ADDR_LEN;
#endif  /* MODULE_NETDEV_ETH */
#ifdef MODULE_NETDEV_IEEE802154
        case NETDEV_TYPE_IEEE802154:
            /* assume address was based on EUI-64
             * (see https://tools.ietf.org/html/rfc6775#section-5.2) */
            memcpy(l2addr, &dst->u64[1], sizeof(dst->u64[1]));
            l2addr[0] ^= 0x02;
            return sizeof(dst->u64[1]);
#endif  /* MODULE_NETDEV_IEEE802154 */
#ifdef MODULE_CC110X
        case NETDEV_TYPE_CC110X:
            l2addr[0] = dst->u8[15];
            return sizeof(uint8_t);
#endif  /* MODULE_CC110X */
        default:
            (void)dst;
            (void)l2addr;
            return 0;
    }
}

bool _resolve_addr_from_ipv6(const ipv6_addr_t *dst, gnrc_netif_t *netif,
                             gnrc_ipv6_nib_nc_t *nce)
{
    bool res = (netif != NULL) && gnrc_netif_is_6ln(netif) &&
               ipv6_addr_is_link_local(dst);

    if (res) {
        uint8_t l2addr_len;

        if ((l2addr_len = _reverse_iid(dst, netif, nce->l2addr)) > 0) {
            DEBUG("nib: resolve address %s%%%u by reverse translating to ",
                  ipv6_addr_to_str(addr_str, dst, sizeof(addr_str)),
                  (unsigned)netif->pid);
            nce->l2addr_len = l2addr_len;
            DEBUG("%s\n",
                  gnrc_netif_addr_to_str(nce->l2addr, nce->l2addr_len,
                                         addr_str));
            memcpy(&nce->ipv6, dst, sizeof(nce->ipv6));
            nce->info = 0;
            nce->info |= (netif->pid << GNRC_IPV6_NIB_NC_INFO_IFACE_POS) &
                         GNRC_IPV6_NIB_NC_INFO_IFACE_MASK;
            nce->info |= GNRC_IPV6_NIB_NC_INFO_NUD_STATE_REACHABLE;
            nce->info |= GNRC_IPV6_NIB_NC_INFO_AR_STATE_REGISTERED;
        }
        else {
            res = false;
        }
    }
    return res;
}

uint8_t _handle_aro(gnrc_netif_t *netif, const ipv6_hdr_t *ipv6,
                    const icmpv6_hdr_t *icmpv6,
                    const sixlowpan_nd_opt_ar_t *aro, const ndp_opt_t *sl2ao,
                    _nib_onl_entry_t *nce)
{
    assert(netif != NULL);
    if (gnrc_netif_is_6ln(netif) && (aro->len == SIXLOWPAN_ND_OPT_AR_LEN)) {
        DEBUG("nib: valid ARO received\n");
        DEBUG(" - length: %u\n", aro->len);
        DEBUG(" - status: %u\n", aro->status);
        DEBUG(" - registration lifetime: %u\n", byteorder_ntohs(aro->ltime));
        DEBUG(" - EUI-64: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
              aro->eui64.uint8[0], aro->eui64.uint8[1], aro->eui64.uint8[2],
              aro->eui64.uint8[3], aro->eui64.uint8[4], aro->eui64.uint8[5],
              aro->eui64.uint8[6], aro->eui64.uint8[7]);
        if (icmpv6->type == ICMPV6_NBR_ADV) {
            if (!_is_iface_eui64(netif, &aro->eui64)) {
                DEBUG("nib: ARO EUI-64 is not mine, ignoring ARO\n");
                return _ADDR_REG_STATUS_IGNORE;
            }
            switch (aro->status) {
                case SIXLOWPAN_ND_STATUS_SUCCESS: {
                    uint16_t ltime = byteorder_ntohs(aro->ltime);
                    uint32_t rereg_time;
                    int idx = gnrc_netif_ipv6_addr_idx(netif, &ipv6->dst);
                    /* if ltime 1min, reschedule NS in 30sec, otherwise 1min
                     * before timeout */
                    rereg_time = (ltime == 1U) ? (30 * MS_PER_SEC) :
                                 (ltime - 1U) * SEC_PER_MIN * MS_PER_SEC;
                    DEBUG("nib: Address registration of %s successful. "
                          "Scheduling re-registration in %" PRIu32 "ms\n",
                          ipv6_addr_to_str(addr_str, &ipv6->dst,
                                           sizeof(addr_str)), rereg_time);
                    netif->ipv6.addrs_flags[idx] &= ~GNRC_NETIF_IPV6_ADDRS_FLAGS_STATE_MASK;
                    netif->ipv6.addrs_flags[idx] |= GNRC_NETIF_IPV6_ADDRS_FLAGS_STATE_VALID;
                    _evtimer_add(&netif->ipv6.addrs[idx],
                                 GNRC_IPV6_NIB_REREG_ADDRESS,
                                 &netif->ipv6.addrs_timers[idx],
                                 rereg_time);
                    break;
                }
                case SIXLOWPAN_ND_STATUS_DUP:
                    DEBUG("nib: Address registration reports duplicate. "
                          "Removing address %s%%%u\n",
                          ipv6_addr_to_str(addr_str,
                                           &ipv6->dst,
                                           sizeof(addr_str)), netif->pid);
                    gnrc_netif_ipv6_addr_remove(netif, &ipv6->dst);
                    /* TODO: generate new address */
                    break;
                case SIXLOWPAN_ND_STATUS_NC_FULL: {
                        DEBUG("nib: Router's neighbor cache is full. "
                              "Searching new router for DAD\n");
                        _nib_dr_entry_t *dr = _nib_drl_get(&ipv6->src, netif->pid);
                        assert(dr != NULL); /* otherwise we wouldn't be here */
                        _nib_drl_remove(dr);
                        if (_nib_drl_iter(NULL) == NULL) { /* no DRL left */
                            netif->ipv6.rs_sent = 0;
                            /* search (hopefully) new router */
                            _handle_search_rtr(netif);
                        }
                        else {
                            assert(dr->next_hop != NULL);
                            _handle_rereg_address(&ipv6->dst);
                        }
                    }
                    break;
            }
            return aro->status;
        }
#if GNRC_IPV6_NIB_CONF_6LR
        else if (gnrc_netif_is_6lr(netif) &&
                 (icmpv6->type == ICMPV6_NBR_SOL)) {
            assert(nce != NULL);
            return _reg_addr_upstream(netif, ipv6, icmpv6, aro, sl2ao, nce);
        }
#else   /* GNRC_IPV6_NIB_CONF_6LR */
        (void)sl2ao;
        (void)nce;
#endif  /* GNRC_IPV6_NIB_CONF_6LR */
    }
#if ENABLE_DEBUG
    else if (aro->len != SIXLOWPAN_ND_OPT_AR_LEN) {
        DEBUG("nib: ARO of unexpected length %u, ignoring ARO\n", aro->len);
    }
#endif  /* ENABLE_DEBUG */
    return _ADDR_REG_STATUS_IGNORE;
}

static inline bool _is_tentative(const gnrc_netif_t *netif, int idx)
{
    return (gnrc_netif_ipv6_addr_get_state(netif, idx) &
            GNRC_NETIF_IPV6_ADDRS_FLAGS_STATE_TENTATIVE);
}

static inline bool _is_valid(const gnrc_netif_t *netif, int idx)
{
    return (gnrc_netif_ipv6_addr_get_state(netif, idx) ==
            GNRC_NETIF_IPV6_ADDRS_FLAGS_STATE_VALID);
}

void _handle_rereg_address(const ipv6_addr_t *addr)
{
    gnrc_netif_t *netif = gnrc_netif_get_by_ipv6_addr(addr);
    _nib_dr_entry_t *router = _nib_drl_get_dr();

    if ((netif != NULL) && (router != NULL)) {
        assert((unsigned)netif->pid == _nib_onl_get_if(router->next_hop));
        DEBUG("nib: Re-registering %s",
              ipv6_addr_to_str(addr_str, addr, sizeof(addr_str)));
        DEBUG(" with upstream router %s\n",
              ipv6_addr_to_str(addr_str, &router->next_hop->ipv6,
                               sizeof(addr_str)));
        _snd_ns(&router->next_hop->ipv6, netif, addr, &router->next_hop->ipv6);
    }
    else {
        DEBUG("nib: Couldn't re-register %s, no current router found or address "
              "wasn't assigned to any interface anymore.\n",
              ipv6_addr_to_str(addr_str, addr, sizeof(addr_str)));
    }
    if (netif != NULL) {
        int idx = gnrc_netif_ipv6_addr_idx(netif, addr);

        if (_is_valid(netif, idx) || (_is_tentative(netif, idx) &&
             (gnrc_netif_ipv6_addr_dad_trans(netif, idx) <
              SIXLOWPAN_ND_REG_TRANSMIT_NUMOF))) {
            uint32_t retrans_time;

            if (_is_valid(netif, idx)) {
                retrans_time = SIXLOWPAN_ND_MAX_RS_SEC_INTERVAL;
            }
            else {
                retrans_time = netif->ipv6.retrans_time;
                /* increment encoded retransmission count */
                netif->ipv6.addrs_flags[idx]++;
            }
            _evtimer_add(&netif->ipv6.addrs[idx], GNRC_IPV6_NIB_REREG_ADDRESS,
                         &netif->ipv6.addrs_timers[idx], retrans_time);
        }
        else {
            netif->ipv6.rs_sent = 0;
            _handle_search_rtr(netif);
        }
    }
}

#if GNRC_IPV6_NIB_CONF_MULTIHOP_P6C
_nib_abr_entry_t *_handle_abro(const sixlowpan_nd_opt_abr_t *abro)
{
    _nib_abr_entry_t *abr = NULL;

    if (abro->len != SIXLOWPAN_ND_OPT_ABR_LEN) {
        /* ignore silently */
        return NULL;
    }
    abr = _nib_abr_add(&abro->braddr);
    if (abr != NULL) {
        uint32_t abro_version = sixlowpan_nd_opt_abr_get_version(abro);
        uint16_t ltime = byteorder_ntohs(abro->ltime);

        if (abr->version >= abro_version) {
            abr->version = abro_version;
            abr->valid_until = _now_min() + ltime;
        }
        /* correct for default value */
        ltime = (ltime == 0) ? SIXLOWPAN_ND_OPT_ABR_LTIME_DEFAULT : ltime;
        _evtimer_add(abr, GNRC_IPV6_NIB_ABR_TIMEOUT, &abr->timeout,
                     /* UINT16_MAX min < UINT32_MAX ms so no risk of overflow */
                     MS_PER_SEC * SEC_PER_MIN * ltime);
    }
    return abr;
}
#endif /* GNRC_IPV6_NIB_CONF_MULTIHOP_P6C */

#if GNRC_IPV6_NIB_CONF_MULTIHOP_P6C
uint32_t _handle_6co(const icmpv6_hdr_t *icmpv6,
                     const sixlowpan_nd_opt_6ctx_t *sixco,
                     _nib_abr_entry_t *abr)
#else   /* GNRC_IPV6_NIB_CONF_MULTIHOP_P6C */
uint32_t _handle_6co(const icmpv6_hdr_t *icmpv6,
                     const sixlowpan_nd_opt_6ctx_t *sixco)
#endif  /* GNRC_IPV6_NIB_CONF_MULTIHOP_P6C */
{
    uint16_t ltime;

#ifdef MODULE_GNRC_SIXLOWPAN_CTX
    uint8_t cid;
#endif  /* MODULE_GNRC_SIXLOWPAN_CTX */

    if ((sixco->len != SIXLOWPAN_ND_OPT_6CTX_LEN_MIN) ||
        ((sixco->len != SIXLOWPAN_ND_OPT_6CTX_LEN_MAX) &&
         (sixco->ctx_len > 64U)) ||
        (icmpv6->type != ICMPV6_RTR_ADV)) {
        DEBUG("nib: received 6CO of invalid length (%u), must be %u "
              "or wasn't delivered by RA."
              "\n",
              sixco->len,
              (sixco->ctx_len > 64U) ? SIXLOWPAN_ND_OPT_6CTX_LEN_MAX :
                                       SIXLOWPAN_ND_OPT_6CTX_LEN_MIN);
        return UINT32_MAX;
    }
    ltime = byteorder_ntohs(sixco->ltime);
#ifdef MODULE_GNRC_SIXLOWPAN_CTX
    cid = sixlowpan_nd_opt_6ctx_get_cid(sixco);
    gnrc_sixlowpan_ctx_update(cid, (ipv6_addr_t *)(sixco + 1), sixco->ctx_len,
                              ltime, sixlowpan_nd_opt_6ctx_is_comp(sixco));
#if GNRC_IPV6_NIB_CONF_MULTIHOP_P6C
    assert(abr != NULL);    /* should have been set in _handle_abro() */
    if (ltime == 0) {
        bf_unset(abr->ctxs, cid);
    }
    else {
        bf_set(abr->ctxs, cid);
    }
#endif  /* GNRC_IPV6_NIB_CONF_MULTIHOP_P6C */
#else   /* MODULE_GNRC_SIXLOWPAN_CTX */
    (void)abr;
#endif  /* MODULE_GNRC_SIXLOWPAN_CTX */
    return ltime * SEC_PER_MIN * MS_PER_SEC;
}
#else  /* GNRC_IPV6_NIB_CONF_6LN */
typedef int dont_be_pedantic;
#endif /* GNRC_IPV6_NIB_CONF_6LN */


/** @} */
