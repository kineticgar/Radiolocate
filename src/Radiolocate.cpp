//============================================================================
// Name        : Radiolocate.cpp
// Copyright   : Copyright (C) 2011 Garrett Brown <garbearucla@gmail.com>
// Description : Real-time locating system by radiation analysis
//               Comprehensive nl80211 interface reference:
//               http://dev.linuxfoundation.org/moblin-navigator/browse/interface.php?cmd=list-bylibrary&Lid=318&changever=2.0_proposed
//============================================================================

#include <time.h>
#ifdef ID_BY_IFNAME
	#include <net/if.h>
#else
	#include <fcntl.h>
#endif

// Install libnl-dev first
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>

#include "nl80211.h"

// Undefine this to lookup the device by physical name
#define ID_BY_IFNAME

using namespace std;

// Stores the result of the scan
int g_signal_strength = 0;

/**********************************
 *  libnl 2.0 compatibility code  *
 **********************************/
#ifndef CONFIG_LIBNL20

static inline struct nl_handle *nl_socket_alloc(void)
{
	return nl_handle_alloc();
}

static inline void nl_socket_free(struct nl_sock *h)
{
	nl_handle_destroy((nl_handle*) h);
}

static inline int __genl_ctrl_alloc_cache(struct nl_sock *h, struct nl_cache **cache)
{
	struct nl_cache *tmp = genl_ctrl_alloc_cache((nl_handle*) h);
	if (!tmp)
		return -1;
	*cache = tmp;
	return 0;
}
#define genl_ctrl_alloc_cache __genl_ctrl_alloc_cache
#endif // CONFIG_LIBNL20


/*******************
 *  nl80211 state  *
 *******************/
struct nl80211_state {
	struct nl_sock *nl_sock;
	struct nl_cache *nl_cache;
	struct genl_family *nl80211;
};

static bool nl80211_init(struct nl80211_state *state)
{
	// Allocate a new socket
	state->nl_sock = (nl_sock*) nl_socket_alloc();
	if (!state->nl_sock) {
		fprintf(stderr, "Failed to allocate netlink socket.\n");
		return false;
	}

	// Connect to the generic netlink
	if (genl_connect((nl_handle*) state->nl_sock)) {
		fprintf(stderr, "Failed to connect to generic netlink.\n");
		nl_socket_free(state->nl_sock);
		return false;
	}

	// Allocate the generic netlink cache
	if (genl_ctrl_alloc_cache(state->nl_sock, &state->nl_cache)) {
		fprintf(stderr, "Failed to allocate generic netlink cache.\n");
		nl_socket_free(state->nl_sock);
		return false;
	}

	// Look up generic netlink family by family name in the provided cache
	state->nl80211 = genl_ctrl_search_by_name(state->nl_cache, "nl80211");
	if (!state->nl80211) {
		fprintf(stderr, "nl80211 not found.\n");
		nl_cache_free(state->nl_cache);
		nl_socket_free(state->nl_sock);
		return false;
	}

	return true;
}

static void nl80211_cleanup(struct nl80211_state *state)
{
	genl_family_put(state->nl80211);
	nl_cache_free(state->nl_cache);
	nl_socket_free(state->nl_sock);
}

/*******************************
 *  nl80211 callback handlers  *
 *******************************/
static int print_sta_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = (genlmsghdr*) nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *sinfo[NL80211_STA_INFO_MAX + 1];
	static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1];
	memset(stats_policy, 0, sizeof(stats_policy));
	stats_policy[NL80211_STA_INFO_INACTIVE_TIME].type = NLA_U32;
	stats_policy[NL80211_STA_INFO_RX_BYTES].type = NLA_U32;
	stats_policy[NL80211_STA_INFO_TX_BYTES].type = NLA_U32;
	stats_policy[NL80211_STA_INFO_RX_PACKETS].type = NLA_U32;
	stats_policy[NL80211_STA_INFO_TX_PACKETS].type = NLA_U32;
	stats_policy[NL80211_STA_INFO_SIGNAL].type = NLA_U8;
	stats_policy[NL80211_STA_INFO_TX_BITRATE].type = NLA_NESTED;
	stats_policy[NL80211_STA_INFO_LLID].type = NLA_U16;
	stats_policy[NL80211_STA_INFO_PLID].type = NLA_U16;
	stats_policy[NL80211_STA_INFO_PLINK_STATE].type = NLA_U8;
	stats_policy[NL80211_STA_INFO_TX_RETRIES].type = NLA_U32;
	stats_policy[NL80211_STA_INFO_TX_FAILED].type = NLA_U32;

	// Create attribute index based on stream of attributes.
	// Iterates over the stream of attributes and stores a pointer to each
	// attribute in the index array using the attribute type as index to the
	// array. Attribute with a type greater than the maximum type specified
	// will be silently ignored in order to maintain backwards compatibility.
	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);

	/*
	 * TODO (from iw scan.c): validate the interface and mac address!
	 * Otherwise, there's a race condition as soon as
	 * the kernel starts sending station notifications.
	 */

	if (!tb[NL80211_ATTR_STA_INFO])
	{
		fprintf(stderr, "STA stats missing!\n");
		return NL_SKIP;
	}
	// Create attribute index based on nested attribute.
	// Feeds the stream of attributes nested into the specified attribute to nla_parse().
	if (nla_parse_nested(sinfo, NL80211_STA_INFO_MAX, tb[NL80211_ATTR_STA_INFO], stats_policy))
	{
		fprintf(stderr, "Failed to parse nested attributes!\n");
		return NL_SKIP;
	}

	// Print the station info
	/*
	char mac_addr[20], dev[20];
	unsigned char *byte = (unsigned char*)nla_data(tb[NL80211_ATTR_MAC]);
	snprintf(mac_addr, sizeof(mac_addr), "%hhX:%hhX:%hhX:%hhX:%hhX:%hhX", byte[0], byte[1], byte[2], byte[3], byte[4], byte[5]);
	if_indextoname(nla_get_u32(tb[NL80211_ATTR_IFINDEX]), dev);
	printf("Station %s (on %s)\n", mac_addr, dev);
	*/

	// Print the signal strength
	if (sinfo[NL80211_STA_INFO_SIGNAL])
	{
		//printf("SIGNAL STRENGTH: %d dBm\n", (int8_t)nla_get_u8(sinfo[NL80211_STA_INFO_SIGNAL]));
		g_signal_strength = (int8_t)nla_get_u8(sinfo[NL80211_STA_INFO_SIGNAL]);
	}

	return NL_SKIP;
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg)
{
	int *ret = (int*) arg;
	*ret = err->error;
	return NL_STOP;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
	int *ret = (int*) arg;
	*ret = 0;
	return NL_SKIP;
}

#ifndef ID_BY_IFNAME
// Converts a physical interface name (e.g. phy0) into an index
static int phy_lookup(const char *name)
{
	char buf[200];
	int fd, pos;

	snprintf(buf, sizeof(buf), "/sys/class/ieee80211/%s/index", name);

	fd = open(buf, O_RDONLY);
	if (fd < 0)
		return -1;
	pos = read(fd, buf, sizeof(buf) - 1);
	if (pos < 0) {
		close(fd);
		return -1;
	}
	buf[pos] = '\0';
	close(fd);
	return atoi(buf);
}
#endif // ID_BY_IFNAME


static int do_scan(void)
{
	struct nl80211_state nlstate;
	struct nl_cb *cb;
	struct nl_cb *s_cb;
	struct nl_msg *msg;
	const bool debug = false;

	if (!nl80211_init(&nlstate))
		return -1;

	// Allocate a new netlink message with the default maximum payload size.
	// Allocates a new netlink message without any further payload. The maximum
	// payload size defaults to PAGESIZE or as otherwise specified with
	// nlmsg_set_default_size()
	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "Failed to allocate netlink message.\n");
		return -1;
	}

	// Allocate new callback handles
	cb = nl_cb_alloc(debug ? NL_CB_DEBUG : NL_CB_DEFAULT);
	s_cb = nl_cb_alloc(debug ? NL_CB_DEBUG : NL_CB_DEFAULT);
	if (!cb || !s_cb) {
		fprintf(stderr, "Failed to allocate netlink callbacks.\n");
		// Release the reference from the netlink message
		nlmsg_free(msg);
		return 2;
	}

	// Add generic netlink header to the netlink message
	genlmsg_put(msg, 0, 0, genl_family_get_id(nlstate.nl80211), 0, NLM_F_DUMP, NL80211_CMD_GET_STATION, 0);

	// Add 32 bit integer attribute to the netlink message
#ifdef ID_BY_IFNAME
	if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_nametoindex("wlan0")) != 0)
#else
	if (nla_put_u32(msg, NL80211_ATTR_WIPHY, phy_lookup("phy0")) != 0)
#endif
	{
		fprintf(stderr, "Building message failed.\n");
		return 2;
	}

	// Set up the callbacks
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_sta_handler, NULL);
	nl_socket_set_cb((nl_handle*) nlstate.nl_sock, s_cb);

	// Automatically complete and send the netlink message
	if (nl_send_auto_complete((nl_handle*) nlstate.nl_sock, msg) < 0)
	{
		fprintf(stderr, "Sending netlink message failed.\n");
		nl_cb_put(cb);
		nlmsg_free(msg);
		return -1;
	}

	int err = 1;
	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);

	// Receive a set of messages from the netlink socket
	while (err > 0)
		nl_recvmsgs((nl_handle*) nlstate.nl_sock, cb);

	nl_cb_put(cb);
	nlmsg_free(msg);

	nl80211_cleanup(&nlstate);
	return err;
}

int main()
{
	const int sleep_interval = 1000; // microseconds
	const int duration = 5; // seconds
	int prev_signal_strength;
	g_signal_strength = 0;
	struct timeval init, cur_time, last;
	gettimeofday(&init, NULL);

	// Get an initial signal strength value
	if (do_scan() != 0 || g_signal_strength == 0)
	{
		printf("Initial scan failed, aborting.\n");
		return -1;
	}
	printf("Signal strength: %d dBm\n", g_signal_strength);
	prev_signal_strength = g_signal_strength;
	gettimeofday(&last, NULL);

	do
	{
		usleep(sleep_interval);
		if (do_scan() != 0)
		{
			printf("Scan failed, aborting.\n");
			return -1;
		}

		gettimeofday(&cur_time, NULL);
		if (prev_signal_strength != g_signal_strength)
		{
			int ms = (cur_time.tv_sec - last.tv_sec) * 1000 + (cur_time.tv_usec - last.tv_usec) / 1000;
			printf("Signal strength: %d dBm (Scan: %d ms)\n", g_signal_strength, ms);
			gettimeofday(&last, NULL);
			prev_signal_strength = g_signal_strength;
		}
	} while (cur_time.tv_sec <= init.tv_sec + duration || cur_time.tv_usec < init.tv_usec);

	// Result: On my Asus, the driver refreshes the signal strength value every 100ms

	return 0;
}

