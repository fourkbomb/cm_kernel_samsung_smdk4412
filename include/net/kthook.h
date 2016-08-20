#ifndef KTHOOK_H
#define KTHOOK_H
#include <linux/types.h>
#include <net/sock.h>

/* Functions to store information about transmission */

#ifdef CONFIG_KTGRABBER
extern void kt_recv_hook(uint32_t classid, int bytes, int ifindex);
extern void kt_send_hook(uint32_t classid, int bytes, int ifindex);

extern bool kt_recv_permitted(uint32_t classid);
extern bool kt_send_permitted(uint32_t classid);
#else
static inline void kt_recv_hook(uint32_t classid, int bytes, int ifindex)
{
}

static inline void kt_send_hook(uint32_t classid, int bytes, int ifindex)
{
}

static inline bool kt_recv_permitted(uint32_t classid)
{
	return false;
}

static inline bool kt_send_permitted(uint32_t classid)
{
	return false;
}
#endif
static inline uint32_t get_classid_from_skb(const struct sk_buff *skb)
{
	return (skb && skb->sk) ? skb->sk->sk_classid : 0;
}

static inline int get_ifindex_from_skb(const struct sk_buff *skb)
{
	int ifindex = 0;
	if (skb)
		ifindex = skb->skb_iif;
	return ifindex;
}

#endif
