/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <string.h>
#include <errno.h>

#include <board.h>
#include <device.h>
#include <init.h>

#include <net/net_pkt.h>
#include <net/net_core.h>
#include <net/net_l2.h>
#include <net/net_if.h>

struct wifi_context {
	struct net_if *iface;
};

static enum net_verdict net_wifi_recv(struct net_if *iface, struct net_pkt *pkt)
{
	NET_DBG("iface %p pkt %p len %zu", iface, pkt, net_pkt_get_len(pkt));

	return NET_CONTINUE;
}

static enum net_verdict net_wifi_send(struct net_if *iface, struct net_pkt *pkt)
{
	struct wifi_context *ctxt = net_if_get_device(iface)->driver_data;

	NET_DBG("iface %p pkt %p len %zu", iface, pkt, net_pkt_get_len(pkt));

	net_if_queue_tx(ctxt->iface, pkt);

	return NET_OK;
}

static inline u16_t net_wifi_reserve(struct net_if *iface, void *unused)
{
	ARG_UNUSED(iface);
	ARG_UNUSED(unused);

	return 0;
}

static int net_wifi_enable(struct net_if *iface, bool state)
{
	struct wifi_context *ctxt = net_if_get_device(iface)->driver_data;

	NET_DBG("iface %p %s", iface, state ? "up" : "down");

	return 0;
}

static enum net_l2_flags net_wifi_flags(struct net_if *iface)
{
	return NET_L2_MULTICAST | NET_L2_MULTICAST_SKIP_JOIN_SOLICIT_NODE;
}

NET_L2_INIT(WIFI_L2, net_wifi_recv, net_wifi_send, net_wifi_reserve,
	    net_wifi_enable, net_wifi_flags);

