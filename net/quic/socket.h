/* SPDX-License-Identifier: GPL-2.0-or-later */
/* QUIC kernel implementation
 * (C) Copyright Red Hat Corp. 2023
 *
 * This file is part of the QUIC kernel implementation
 *
 * Written or modified by:
 *    Xin Long <lucien.xin@gmail.com>
 */

#ifndef __net_quic_h__
#define __net_quic_h__

#include <net/udp_tunnel.h>
#include <linux/quic.h>

#include "hashtable.h"
#include "protocol.h"
#include "pnspace.h"
#include "number.h"
#include "connid.h"
#include "stream.h"
#include "crypto.h"
#include "frame.h"
#include "cong.h"
#include "path.h"

#include "packet.h"
#include "output.h"
#include "input.h"
#include "timer.h"

extern struct proto quic_prot;
extern struct proto quicv6_prot;

extern struct proto quic_handshake_prot;
extern struct proto quicv6_handshake_prot;

enum quic_state {
	QUIC_SS_CLOSED		= TCP_CLOSE,
	QUIC_SS_LISTENING	= TCP_LISTEN,
	QUIC_SS_ESTABLISHING	= TCP_SYN_RECV,
	QUIC_SS_ESTABLISHED	= TCP_ESTABLISHED,
};

struct quic_request_sock {
	struct list_head	list;
	union quic_addr		da;
	union quic_addr		sa;
	struct quic_conn_id	dcid;
	struct quic_conn_id	scid;
	struct quic_conn_id	orig_dcid;
	u8			retry;
	u32			version;
};

enum quic_tsq_enum {
	QUIC_MTU_REDUCED_DEFERRED,
	QUIC_AP_LOSS_DEFERRED,
	QUIC_IN_LOSS_DEFERRED,
	QUIC_HS_LOSS_DEFERRED,
	QUIC_SACK_DEFERRED,
	QUIC_PATH_DEFERRED,
	QUIC_TSQ_DEFERRED,
};

enum quic_tsq_flags {
	QUIC_F_MTU_REDUCED_DEFERRED	= BIT(QUIC_MTU_REDUCED_DEFERRED),
	QUIC_F_AP_LOSS_DEFERRED		= BIT(QUIC_AP_LOSS_DEFERRED),
	QUIC_F_IN_LOSS_DEFERRED		= BIT(QUIC_IN_LOSS_DEFERRED),
	QUIC_F_HS_LOSS_DEFERRED		= BIT(QUIC_HS_LOSS_DEFERRED),
	QUIC_F_SACK_DEFERRED		= BIT(QUIC_SACK_DEFERRED),
	QUIC_F_PATH_DEFERRED		= BIT(QUIC_PATH_DEFERRED),
	QUIC_F_TSQ_DEFERRED		= BIT(QUIC_TSQ_DEFERRED),
};

#define QUIC_DEFERRED_ALL (QUIC_F_MTU_REDUCED_DEFERRED |	\
			   QUIC_F_AP_LOSS_DEFERRED |		\
			   QUIC_F_IN_LOSS_DEFERRED |		\
			   QUIC_F_HS_LOSS_DEFERRED |		\
			   QUIC_F_SACK_DEFERRED |		\
			   QUIC_F_PATH_DEFERRED |		\
			   QUIC_F_TSQ_DEFERRED)

struct quic_sock {
	struct inet_sock		inet;
	struct list_head		reqs;
	struct quic_path_src		src;
	struct quic_path_dst		dst;
	struct quic_addr_family_ops	*af_ops; /* inet4 or inet6 */

	struct quic_conn_id_set		source;
	struct quic_conn_id_set		dest;
	struct quic_stream_table	streams;
	struct quic_cong		cong;
	struct quic_crypto		crypto[QUIC_CRYPTO_MAX];
	struct quic_pnspace		space[QUIC_PNSPACE_MAX];

	struct quic_transport_param	local;
	struct quic_transport_param	remote;
	struct quic_config		config;
	struct quic_data		token;
	struct quic_data		ticket;
	struct quic_data		alpn;

	struct quic_outqueue		outq;
	struct quic_inqueue		inq;
	struct quic_packet		packet;
	struct quic_timer		timers[QUIC_TIMER_MAX];
};

struct quic6_sock {
	struct quic_sock	quic;
	struct ipv6_pinfo	inet6;
};

static inline struct quic_sock *quic_sk(const struct sock *sk)
{
	return (struct quic_sock *)sk;
}

static inline void quic_set_af_ops(struct sock *sk, struct quic_addr_family_ops *af_ops)
{
	quic_sk(sk)->af_ops = af_ops;
}

static inline struct quic_addr_family_ops *quic_af_ops(const struct sock *sk)
{
	return quic_sk(sk)->af_ops;
}

static inline struct quic_path_addr *quic_src(const struct sock *sk)
{
	return &quic_sk(sk)->src.a;
}

static inline struct quic_path_addr *quic_dst(const struct sock *sk)
{
	return &quic_sk(sk)->dst.a;
}

static inline struct quic_packet *quic_packet(const struct sock *sk)
{
	return &quic_sk(sk)->packet;
}

static inline struct quic_outqueue *quic_outq(const struct sock *sk)
{
	return &quic_sk(sk)->outq;
}

static inline struct quic_inqueue *quic_inq(const struct sock *sk)
{
	return &quic_sk(sk)->inq;
}

static inline struct quic_cong *quic_cong(const struct sock *sk)
{
	return &quic_sk(sk)->cong;
}

static inline struct quic_crypto *quic_crypto(const struct sock *sk, u8 level)
{
	return &quic_sk(sk)->crypto[level];
}

static inline struct quic_pnspace *quic_pnspace(const struct sock *sk, u8 level)
{
	return &quic_sk(sk)->space[level];
}

static inline struct quic_stream_table *quic_streams(const struct sock *sk)
{
	return &quic_sk(sk)->streams;
}

static inline void *quic_timer(const struct sock *sk, u8 type)
{
	return (void *)&quic_sk(sk)->timers[type];
}

static inline struct list_head *quic_reqs(const struct sock *sk)
{
	return &quic_sk(sk)->reqs;
}

static inline struct quic_config *quic_config(const struct sock *sk)
{
	return &quic_sk(sk)->config;
}

static inline struct quic_data *quic_token(const struct sock *sk)
{
	return &quic_sk(sk)->token;
}

static inline struct quic_data *quic_ticket(const struct sock *sk)
{
	return &quic_sk(sk)->ticket;
}

static inline struct quic_data *quic_alpn(const struct sock *sk)
{
	return &quic_sk(sk)->alpn;
}

static inline struct quic_conn_id_set *quic_source(const struct sock *sk)
{
	return &quic_sk(sk)->source;
}

static inline struct quic_conn_id_set *quic_dest(const struct sock *sk)
{
	return &quic_sk(sk)->dest;
}

static inline struct quic_transport_param *quic_local(const struct sock *sk)
{
	return &quic_sk(sk)->local;
}

static inline struct quic_transport_param *quic_remote(const struct sock *sk)
{
	return &quic_sk(sk)->remote;
}

static inline bool quic_is_serv(const struct sock *sk)
{
	return quic_outq(sk)->serv;
}

static inline bool quic_is_establishing(struct sock *sk)
{
	return sk->sk_state == QUIC_SS_ESTABLISHING;
}

static inline bool quic_is_established(struct sock *sk)
{
	return sk->sk_state == QUIC_SS_ESTABLISHED;
}

static inline bool quic_is_listen(struct sock *sk)
{
	return sk->sk_state == QUIC_SS_LISTENING;
}

static inline bool quic_is_closed(struct sock *sk)
{
	return sk->sk_state == QUIC_SS_CLOSED;
}

static inline void quic_set_state(struct sock *sk, int state)
{
	inet_sk_set_state(sk, state);
	sk->sk_state_change(sk);
}

struct sock *quic_sock_lookup(struct sk_buff *skb, union quic_addr *sa, union quic_addr *da);
int quic_request_sock_enqueue(struct sock *sk, struct quic_conn_id *odcid, u8 retry);
struct quic_request_sock *quic_request_sock_dequeue(struct sock *sk);
int quic_accept_sock_exists(struct sock *sk, struct sk_buff *skb);
bool quic_request_sock_exists(struct sock *sk);

int quic_sock_change_saddr(struct sock *sk, union quic_addr *addr, u32 len);
int quic_sock_change_daddr(struct sock *sk, union quic_addr *addr, u32 len);

#endif /* __net_quic_h__ */
