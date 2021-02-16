// SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
/* Copyright (C) 2017-2018 Netronome Systems, Inc. */

#include <linux/skbuff.h>
#include <net/devlink.h>
#include <net/pkt_cls.h>

#include "cmsg.h"
#include "main.h"
#include "../nfpcore/nfp_cpp.h"
#include "../nfpcore/nfp_nsp.h"
#include "../nfp_app.h"
#include "../nfp_main.h"
#include "../nfp_net.h"
#include "../nfp_port.h"

#define NFP_FLOWER_SUPPORTED_TCPFLAGS \
	(TCPHDR_FIN | TCPHDR_SYN | TCPHDR_RST | \
	 TCPHDR_PSH | TCPHDR_URG)

#define NFP_FLOWER_SUPPORTED_CTLFLAGS \
	(FLOW_DIS_IS_FRAGMENT | \
	 FLOW_DIS_FIRST_FRAG)

#define NFP_FLOWER_WHITELIST_DISSECTOR \
	(BIT(FLOW_DISSECTOR_KEY_CONTROL) | \
	 BIT(FLOW_DISSECTOR_KEY_BASIC) | \
	 BIT(FLOW_DISSECTOR_KEY_IPV4_ADDRS) | \
	 BIT(FLOW_DISSECTOR_KEY_IPV6_ADDRS) | \
	 BIT(FLOW_DISSECTOR_KEY_TCP) | \
	 BIT(FLOW_DISSECTOR_KEY_PORTS) | \
	 BIT(FLOW_DISSECTOR_KEY_ETH_ADDRS) | \
	 BIT(FLOW_DISSECTOR_KEY_VLAN) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_KEYID) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_IPV4_ADDRS) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_IPV6_ADDRS) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_CONTROL) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_PORTS) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_OPTS) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_IP) | \
	 BIT(FLOW_DISSECTOR_KEY_MPLS) | \
	 BIT(FLOW_DISSECTOR_KEY_IP))

#define NFP_FLOWER_WHITELIST_TUN_DISSECTOR \
	(BIT(FLOW_DISSECTOR_KEY_ENC_CONTROL) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_KEYID) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_IPV4_ADDRS) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_IPV6_ADDRS) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_OPTS) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_PORTS) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_IP))

#define NFP_FLOWER_WHITELIST_TUN_DISSECTOR_R \
	(BIT(FLOW_DISSECTOR_KEY_ENC_CONTROL) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_IPV4_ADDRS) | \
	 BIT(FLOW_DISSECTOR_KEY_ENC_PORTS))

static int
nfp_flower_xmit_flow(struct nfp_app *app, struct nfp_fl_payload *nfp_flow,
		     u8 mtype)
{
	u32 meta_len, key_len, mask_len, act_len, tot_len;
	struct sk_buff *skb;
	unsigned char *msg;

	meta_len =  sizeof(struct nfp_fl_rule_metadata);
	key_len = nfp_flow->meta.key_len;
	mask_len = nfp_flow->meta.mask_len;
	act_len = nfp_flow->meta.act_len;

	tot_len = meta_len + key_len + mask_len + act_len;

	/* Convert to long words as firmware expects
	 * lengths in units of NFP_FL_LW_SIZ.
	 */
	nfp_flow->meta.key_len >>= NFP_FL_LW_SIZ;
	nfp_flow->meta.mask_len >>= NFP_FL_LW_SIZ;
	nfp_flow->meta.act_len >>= NFP_FL_LW_SIZ;

	skb = nfp_flower_cmsg_alloc(app, tot_len, mtype, GFP_KERNEL);
	if (!skb)
		return -ENOMEM;

	msg = nfp_flower_cmsg_get_data(skb);
	memcpy(msg, &nfp_flow->meta, meta_len);
	memcpy(&msg[meta_len], nfp_flow->unmasked_data, key_len);
	memcpy(&msg[meta_len + key_len], nfp_flow->mask_data, mask_len);
	memcpy(&msg[meta_len + key_len + mask_len],
	       nfp_flow->action_data, act_len);

	/* Convert back to bytes as software expects
	 * lengths in units of bytes.
	 */
	nfp_flow->meta.key_len <<= NFP_FL_LW_SIZ;
	nfp_flow->meta.mask_len <<= NFP_FL_LW_SIZ;
	nfp_flow->meta.act_len <<= NFP_FL_LW_SIZ;

	nfp_ctrl_tx(app->ctrl, skb);

	return 0;
}

static bool nfp_flower_check_higher_than_mac(struct tc_cls_flower_offload *f)
{
	struct flow_rule *rule = tc_cls_flower_offload_flow_rule(f);

	return flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_IPV4_ADDRS) ||
	       flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_IPV6_ADDRS) ||
	       flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_PORTS) ||
	       flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_ICMP);
}

static int
nfp_flower_calc_opt_layer(struct flow_match_enc_opts *enc_opts,
			  u32 *key_layer_two, int *key_size)
{
	if (enc_opts->key->len > NFP_FL_MAX_GENEVE_OPT_KEY)
		return -EOPNOTSUPP;

	if (enc_opts->key->len > 0) {
		*key_layer_two |= NFP_FLOWER_LAYER2_GENEVE_OP;
		*key_size += sizeof(struct nfp_flower_geneve_options);
	}

	return 0;
}

static int
nfp_flower_calculate_key_layers(struct nfp_app *app,
				struct net_device *netdev,
				struct nfp_fl_key_ls *ret_key_ls,
				struct tc_cls_flower_offload *flow,
				enum nfp_flower_tun_type *tun_type)
{
	struct flow_rule *rule = tc_cls_flower_offload_flow_rule(flow);
	struct flow_dissector *dissector = rule->match.dissector;
	struct flow_match_basic basic = { NULL, NULL};
	struct nfp_flower_priv *priv = app->priv;
	u32 key_layer_two;
	u8 key_layer;
	int key_size;
	int err;

	if (dissector->used_keys & ~NFP_FLOWER_WHITELIST_DISSECTOR)
		return -EOPNOTSUPP;

	/* If any tun dissector is used then the required set must be used. */
	if (dissector->used_keys & NFP_FLOWER_WHITELIST_TUN_DISSECTOR &&
	    (dissector->used_keys & NFP_FLOWER_WHITELIST_TUN_DISSECTOR_R)
	    != NFP_FLOWER_WHITELIST_TUN_DISSECTOR_R)
		return -EOPNOTSUPP;

	key_layer_two = 0;
	key_layer = NFP_FLOWER_LAYER_PORT;
	key_size = sizeof(struct nfp_flower_meta_tci) +
		   sizeof(struct nfp_flower_in_port);

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_ETH_ADDRS) ||
	    flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_MPLS)) {
		key_layer |= NFP_FLOWER_LAYER_MAC;
		key_size += sizeof(struct nfp_flower_mac_mpls);
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_VLAN)) {
		struct flow_match_vlan vlan;

		flow_rule_match_vlan(rule, &vlan);
		if (!(priv->flower_ext_feats & NFP_FL_FEATS_VLAN_PCP) &&
		    vlan.key->vlan_priority)
			return -EOPNOTSUPP;
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_ENC_CONTROL)) {
		struct flow_match_enc_opts enc_op = { NULL, NULL };
		struct flow_match_ipv4_addrs ipv4_addrs;
		struct flow_match_control enc_ctl;
		struct flow_match_ports enc_ports;

		flow_rule_match_enc_control(rule, &enc_ctl);

		if (enc_ctl.mask->addr_type != 0xffff ||
		    enc_ctl.key->addr_type != FLOW_DISSECTOR_KEY_IPV4_ADDRS)
			return -EOPNOTSUPP;

		/* These fields are already verified as used. */
		flow_rule_match_enc_ipv4_addrs(rule, &ipv4_addrs);
		if (ipv4_addrs.mask->dst != cpu_to_be32(~0))
			return -EOPNOTSUPP;

		flow_rule_match_enc_ports(rule, &enc_ports);
		if (enc_ports.mask->dst != cpu_to_be16(~0))
			return -EOPNOTSUPP;

		if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_ENC_OPTS))
			flow_rule_match_enc_opts(rule, &enc_op);

		switch (enc_ports.key->dst) {
		case htons(NFP_FL_VXLAN_PORT):
			*tun_type = NFP_FL_TUNNEL_VXLAN;
			key_layer |= NFP_FLOWER_LAYER_VXLAN;
			key_size += sizeof(struct nfp_flower_ipv4_udp_tun);

			if (enc_op.key)
				return -EOPNOTSUPP;
			break;
		case htons(NFP_FL_GENEVE_PORT):
			if (!(priv->flower_ext_feats & NFP_FL_FEATS_GENEVE))
				return -EOPNOTSUPP;
			*tun_type = NFP_FL_TUNNEL_GENEVE;
			key_layer |= NFP_FLOWER_LAYER_EXT_META;
			key_size += sizeof(struct nfp_flower_ext_meta);
			key_layer_two |= NFP_FLOWER_LAYER2_GENEVE;
			key_size += sizeof(struct nfp_flower_ipv4_udp_tun);

			if (!enc_op.key)
				break;
			if (!(priv->flower_ext_feats & NFP_FL_FEATS_GENEVE_OPT))
				return -EOPNOTSUPP;
			err = nfp_flower_calc_opt_layer(&enc_op, &key_layer_two,
							&key_size);
			if (err)
				return err;
			break;
		default:
			return -EOPNOTSUPP;
		}

		/* Ensure the ingress netdev matches the expected tun type. */
		if (!nfp_fl_netdev_is_tunnel_type(netdev, *tun_type))
			return -EOPNOTSUPP;
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_BASIC))
		flow_rule_match_basic(rule, &basic);

	if (basic.mask && basic.mask->n_proto) {
		/* Ethernet type is present in the key. */
		switch (basic.key->n_proto) {
		case cpu_to_be16(ETH_P_IP):
			key_layer |= NFP_FLOWER_LAYER_IPV4;
			key_size += sizeof(struct nfp_flower_ipv4);
			break;

		case cpu_to_be16(ETH_P_IPV6):
			key_layer |= NFP_FLOWER_LAYER_IPV6;
			key_size += sizeof(struct nfp_flower_ipv6);
			break;

		/* Currently we do not offload ARP
		 * because we rely on it to get to the host.
		 */
		case cpu_to_be16(ETH_P_ARP):
			return -EOPNOTSUPP;

		case cpu_to_be16(ETH_P_MPLS_UC):
		case cpu_to_be16(ETH_P_MPLS_MC):
			if (!(key_layer & NFP_FLOWER_LAYER_MAC)) {
				key_layer |= NFP_FLOWER_LAYER_MAC;
				key_size += sizeof(struct nfp_flower_mac_mpls);
			}
			break;

		/* Will be included in layer 2. */
		case cpu_to_be16(ETH_P_8021Q):
			break;

		default:
			/* Other ethtype - we need check the masks for the
			 * remainder of the key to ensure we can offload.
			 */
			if (nfp_flower_check_higher_than_mac(flow))
				return -EOPNOTSUPP;
			break;
		}
	}

	if (basic.mask && basic.mask->ip_proto) {
		/* Ethernet type is present in the key. */
		switch (basic.key->ip_proto) {
		case IPPROTO_TCP:
		case IPPROTO_UDP:
		case IPPROTO_SCTP:
		case IPPROTO_ICMP:
		case IPPROTO_ICMPV6:
			key_layer |= NFP_FLOWER_LAYER_TP;
			key_size += sizeof(struct nfp_flower_tp_ports);
			break;
		default:
			/* Other ip proto - we need check the masks for the
			 * remainder of the key to ensure we can offload.
			 */
			return -EOPNOTSUPP;
		}
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_TCP)) {
		struct flow_match_tcp tcp;
		u32 tcp_flags;

		flow_rule_match_tcp(rule, &tcp);
		tcp_flags = be16_to_cpu(tcp.key->flags);

		if (tcp_flags & ~NFP_FLOWER_SUPPORTED_TCPFLAGS)
			return -EOPNOTSUPP;

		/* We only support PSH and URG flags when either
		 * FIN, SYN or RST is present as well.
		 */
		if ((tcp_flags & (TCPHDR_PSH | TCPHDR_URG)) &&
		    !(tcp_flags & (TCPHDR_FIN | TCPHDR_SYN | TCPHDR_RST)))
			return -EOPNOTSUPP;

		/* We need to store TCP flags in the either the IPv4 or IPv6 key
		 * space, thus we need to ensure we include a IPv4/IPv6 key
		 * layer if we have not done so already.
		 */
		if (!basic.key)
			return -EOPNOTSUPP;

		if (!(key_layer & NFP_FLOWER_LAYER_IPV4) &&
		    !(key_layer & NFP_FLOWER_LAYER_IPV6)) {
			switch (basic.key->n_proto) {
			case cpu_to_be16(ETH_P_IP):
				key_layer |= NFP_FLOWER_LAYER_IPV4;
				key_size += sizeof(struct nfp_flower_ipv4);
				break;

			case cpu_to_be16(ETH_P_IPV6):
				key_layer |= NFP_FLOWER_LAYER_IPV6;
				key_size += sizeof(struct nfp_flower_ipv6);
				break;

			default:
				return -EOPNOTSUPP;
			}
		}
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_CONTROL)) {
		struct flow_match_control ctl;

		flow_rule_match_control(rule, &ctl);
		if (ctl.key->flags & ~NFP_FLOWER_SUPPORTED_CTLFLAGS)
			return -EOPNOTSUPP;
	}

	ret_key_ls->key_layer = key_layer;
	ret_key_ls->key_layer_two = key_layer_two;
	ret_key_ls->key_size = key_size;

	return 0;
}

static struct nfp_fl_payload *
nfp_flower_allocate_new(struct nfp_fl_key_ls *key_layer)
{
	struct nfp_fl_payload *flow_pay;

	flow_pay = kmalloc(sizeof(*flow_pay), GFP_KERNEL);
	if (!flow_pay)
		return NULL;

	flow_pay->meta.key_len = key_layer->key_size;
	flow_pay->unmasked_data = kmalloc(key_layer->key_size, GFP_KERNEL);
	if (!flow_pay->unmasked_data)
		goto err_free_flow;

	flow_pay->meta.mask_len = key_layer->key_size;
	flow_pay->mask_data = kmalloc(key_layer->key_size, GFP_KERNEL);
	if (!flow_pay->mask_data)
		goto err_free_unmasked;

	flow_pay->action_data = kmalloc(NFP_FL_MAX_A_SIZ, GFP_KERNEL);
	if (!flow_pay->action_data)
		goto err_free_mask;

	flow_pay->nfp_tun_ipv4_addr = 0;
	flow_pay->meta.flags = 0;

	return flow_pay;

err_free_mask:
	kfree(flow_pay->mask_data);
err_free_unmasked:
	kfree(flow_pay->unmasked_data);
err_free_flow:
	kfree(flow_pay);
	return NULL;
}

/**
 * nfp_flower_add_offload() - Adds a new flow to hardware.
 * @app:	Pointer to the APP handle
 * @netdev:	netdev structure.
 * @flow:	TC flower classifier offload structure.
 *
 * Adds a new flow to the repeated hash structure and action payload.
 *
 * Return: negative value on error, 0 if configured successfully.
 */
static int
nfp_flower_add_offload(struct nfp_app *app, struct net_device *netdev,
		       struct tc_cls_flower_offload *flow)
{
	enum nfp_flower_tun_type tun_type = NFP_FL_TUNNEL_NONE;
	struct nfp_flower_priv *priv = app->priv;
	struct nfp_fl_payload *flow_pay;
	struct nfp_fl_key_ls *key_layer;
	struct nfp_port *port = NULL;
	int err;

	if (nfp_netdev_is_nfp_repr(netdev))
		port = nfp_port_from_netdev(netdev);

	key_layer = kmalloc(sizeof(*key_layer), GFP_KERNEL);
	if (!key_layer)
		return -ENOMEM;

	err = nfp_flower_calculate_key_layers(app, netdev, key_layer, flow,
					      &tun_type);
	if (err)
		goto err_free_key_ls;

	flow_pay = nfp_flower_allocate_new(key_layer);
	if (!flow_pay) {
		err = -ENOMEM;
		goto err_free_key_ls;
	}

	err = nfp_flower_compile_flow_match(app, flow, key_layer, netdev,
					    flow_pay, tun_type);
	if (err)
		goto err_destroy_flow;

	err = nfp_flower_compile_action(app, flow, netdev, flow_pay);
	if (err)
		goto err_destroy_flow;

	err = nfp_compile_flow_metadata(app, flow, flow_pay, netdev);
	if (err)
		goto err_destroy_flow;

	flow_pay->tc_flower_cookie = flow->cookie;
	err = rhashtable_insert_fast(&priv->flow_table, &flow_pay->fl_node,
				     nfp_flower_table_params);
	if (err)
		goto err_release_metadata;

	err = nfp_flower_xmit_flow(app, flow_pay,
				   NFP_FLOWER_CMSG_TYPE_FLOW_ADD);
	if (err)
		goto err_remove_rhash;

	if (port)
		port->tc_offload_cnt++;

	/* Deallocate flow payload when flower rule has been destroyed. */
	kfree(key_layer);

	return 0;

err_remove_rhash:
	WARN_ON_ONCE(rhashtable_remove_fast(&priv->flow_table,
					    &flow_pay->fl_node,
					    nfp_flower_table_params));
err_release_metadata:
	nfp_modify_flow_metadata(app, flow_pay);
err_destroy_flow:
	kfree(flow_pay->action_data);
	kfree(flow_pay->mask_data);
	kfree(flow_pay->unmasked_data);
	kfree(flow_pay);
err_free_key_ls:
	kfree(key_layer);
	return err;
}

/**
 * nfp_flower_del_offload() - Removes a flow from hardware.
 * @app:	Pointer to the APP handle
 * @netdev:	netdev structure.
 * @flow:	TC flower classifier offload structure
 *
 * Removes a flow from the repeated hash structure and clears the
 * action payload.
 *
 * Return: negative value on error, 0 if removed successfully.
 */
static int
nfp_flower_del_offload(struct nfp_app *app, struct net_device *netdev,
		       struct tc_cls_flower_offload *flow)
{
	struct nfp_flower_priv *priv = app->priv;
	struct nfp_fl_payload *nfp_flow;
	struct nfp_port *port = NULL;
	int err;

	if (nfp_netdev_is_nfp_repr(netdev))
		port = nfp_port_from_netdev(netdev);

	nfp_flow = nfp_flower_search_fl_table(app, flow->cookie, netdev);
	if (!nfp_flow)
		return -ENOENT;

	err = nfp_modify_flow_metadata(app, nfp_flow);
	if (err)
		goto err_free_flow;

	if (nfp_flow->nfp_tun_ipv4_addr)
		nfp_tunnel_del_ipv4_off(app, nfp_flow->nfp_tun_ipv4_addr);

	err = nfp_flower_xmit_flow(app, nfp_flow,
				   NFP_FLOWER_CMSG_TYPE_FLOW_DEL);
	if (err)
		goto err_free_flow;

err_free_flow:
	if (port)
		port->tc_offload_cnt--;
	kfree(nfp_flow->action_data);
	kfree(nfp_flow->mask_data);
	kfree(nfp_flow->unmasked_data);
	WARN_ON_ONCE(rhashtable_remove_fast(&priv->flow_table,
					    &nfp_flow->fl_node,
					    nfp_flower_table_params));
	kfree_rcu(nfp_flow, rcu);
	return err;
}

/**
 * nfp_flower_get_stats() - Populates flow stats obtained from hardware.
 * @app:	Pointer to the APP handle
 * @netdev:	Netdev structure.
 * @flow:	TC flower classifier offload structure
 *
 * Populates a flow statistics structure which which corresponds to a
 * specific flow.
 *
 * Return: negative value on error, 0 if stats populated successfully.
 */
static int
nfp_flower_get_stats(struct nfp_app *app, struct net_device *netdev,
		     struct tc_cls_flower_offload *flow)
{
	struct nfp_flower_priv *priv = app->priv;
	struct nfp_fl_payload *nfp_flow;
	u32 ctx_id;

	nfp_flow = nfp_flower_search_fl_table(app, flow->cookie, netdev);
	if (!nfp_flow)
		return -EINVAL;

	ctx_id = be32_to_cpu(nfp_flow->meta.host_ctx_id);

	spin_lock_bh(&priv->stats_lock);
	flow_stats_update(&flow->stats, priv->stats[ctx_id].bytes,
			  priv->stats[ctx_id].pkts, priv->stats[ctx_id].used);

	priv->stats[ctx_id].pkts = 0;
	priv->stats[ctx_id].bytes = 0;
	spin_unlock_bh(&priv->stats_lock);

	return 0;
}

static int
nfp_flower_repr_offload(struct nfp_app *app, struct net_device *netdev,
			struct tc_cls_flower_offload *flower)
{
	if (!eth_proto_is_802_3(flower->common.protocol))
		return -EOPNOTSUPP;

	switch (flower->command) {
	case TC_CLSFLOWER_REPLACE:
		return nfp_flower_add_offload(app, netdev, flower);
	case TC_CLSFLOWER_DESTROY:
		return nfp_flower_del_offload(app, netdev, flower);
	case TC_CLSFLOWER_STATS:
		return nfp_flower_get_stats(app, netdev, flower);
	default:
		return -EOPNOTSUPP;
	}
}

static int nfp_flower_setup_tc_block_cb(enum tc_setup_type type,
					void *type_data, void *cb_priv)
{
	struct nfp_repr *repr = cb_priv;

	if (!tc_cls_can_offload_and_chain0(repr->netdev, type_data))
		return -EOPNOTSUPP;

	switch (type) {
	case TC_SETUP_CLSFLOWER:
		return nfp_flower_repr_offload(repr->app, repr->netdev,
					       type_data);
	default:
		return -EOPNOTSUPP;
	}
}

static int nfp_flower_setup_tc_block(struct net_device *netdev,
				     struct tc_block_offload *f)
{
	struct nfp_repr *repr = netdev_priv(netdev);

	if (f->binder_type != TCF_BLOCK_BINDER_TYPE_CLSACT_INGRESS)
		return -EOPNOTSUPP;

	switch (f->command) {
	case TC_BLOCK_BIND:
		return tcf_block_cb_register(f->block,
					     nfp_flower_setup_tc_block_cb,
					     repr, repr, f->extack);
	case TC_BLOCK_UNBIND:
		tcf_block_cb_unregister(f->block,
					nfp_flower_setup_tc_block_cb,
					repr);
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

int nfp_flower_setup_tc(struct nfp_app *app, struct net_device *netdev,
			enum tc_setup_type type, void *type_data)
{
	switch (type) {
	case TC_SETUP_BLOCK:
		return nfp_flower_setup_tc_block(netdev, type_data);
	default:
		return -EOPNOTSUPP;
	}
}

struct nfp_flower_indr_block_cb_priv {
	struct net_device *netdev;
	struct nfp_app *app;
	struct list_head list;
};

static struct nfp_flower_indr_block_cb_priv *
nfp_flower_indr_block_cb_priv_lookup(struct nfp_app *app,
				     struct net_device *netdev)
{
	struct nfp_flower_indr_block_cb_priv *cb_priv;
	struct nfp_flower_priv *priv = app->priv;

	/* All callback list access should be protected by RTNL. */
	ASSERT_RTNL();

	list_for_each_entry(cb_priv, &priv->indr_block_cb_priv, list)
		if (cb_priv->netdev == netdev)
			return cb_priv;

	return NULL;
}

static int nfp_flower_setup_indr_block_cb(enum tc_setup_type type,
					  void *type_data, void *cb_priv)
{
	struct nfp_flower_indr_block_cb_priv *priv = cb_priv;
	struct tc_cls_flower_offload *flower = type_data;

	if (flower->common.chain_index)
		return -EOPNOTSUPP;

	switch (type) {
	case TC_SETUP_CLSFLOWER:
		return nfp_flower_repr_offload(priv->app, priv->netdev,
					       type_data);
	default:
		return -EOPNOTSUPP;
	}
}

static int
nfp_flower_setup_indr_tc_block(struct net_device *netdev, struct nfp_app *app,
			       struct tc_block_offload *f)
{
	struct nfp_flower_indr_block_cb_priv *cb_priv;
	struct nfp_flower_priv *priv = app->priv;
	int err;

	if (f->binder_type != TCF_BLOCK_BINDER_TYPE_CLSACT_INGRESS)
		return -EOPNOTSUPP;

	switch (f->command) {
	case TC_BLOCK_BIND:
		cb_priv = kmalloc(sizeof(*cb_priv), GFP_KERNEL);
		if (!cb_priv)
			return -ENOMEM;

		cb_priv->netdev = netdev;
		cb_priv->app = app;
		list_add(&cb_priv->list, &priv->indr_block_cb_priv);

		err = tcf_block_cb_register(f->block,
					    nfp_flower_setup_indr_block_cb,
					    cb_priv, cb_priv, f->extack);
		if (err) {
			list_del(&cb_priv->list);
			kfree(cb_priv);
		}

		return err;
	case TC_BLOCK_UNBIND:
		cb_priv = nfp_flower_indr_block_cb_priv_lookup(app, netdev);
		if (!cb_priv)
			return -ENOENT;

		tcf_block_cb_unregister(f->block,
					nfp_flower_setup_indr_block_cb,
					cb_priv);
		list_del(&cb_priv->list);
		kfree(cb_priv);

		return 0;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static int
nfp_flower_indr_setup_tc_cb(struct net_device *netdev, void *cb_priv,
			    enum tc_setup_type type, void *type_data)
{
	switch (type) {
	case TC_SETUP_BLOCK:
		return nfp_flower_setup_indr_tc_block(netdev, cb_priv,
						      type_data);
	default:
		return -EOPNOTSUPP;
	}
}

int nfp_flower_reg_indir_block_handler(struct nfp_app *app,
				       struct net_device *netdev,
				       unsigned long event)
{
	int err;

	if (!nfp_fl_is_netdev_to_offload(netdev))
		return NOTIFY_OK;

	if (event == NETDEV_REGISTER) {
		err = __tc_indr_block_cb_register(netdev, app,
						  nfp_flower_indr_setup_tc_cb,
						  app);
		if (err)
			nfp_flower_cmsg_warn(app,
					     "Indirect block reg failed - %s\n",
					     netdev->name);
	} else if (event == NETDEV_UNREGISTER) {
		__tc_indr_block_cb_unregister(netdev,
					      nfp_flower_indr_setup_tc_cb, app);
	}

	return NOTIFY_OK;
}