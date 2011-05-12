//============================================================================
// Name        : Radiolocate.cpp
// Author      : 
// Version     :
// Copyright   : Copyright (C) 2011 Garrett Brown <gbruin@ucla.edu>
// Description : Real-time locating system by radiation analysis
//============================================================================

// Undefine this to lookup the device by physical name
#define ID_BY_IFNAME

//#include <time.h>
#ifdef ID_BY_IFNAME
	#include <net/if.h>
#else
	#include <fcntl.h>
#endif

// First install libnl-dev
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>

#include "NetworkAccessPoint.h"
#include "nl80211.h"

using namespace std;

/* libnl 2.0 compatibility code */
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

static int print_sta_handler(struct nl_msg *msg, void *arg)
{
	fprintf(stderr, "Print handler\n");
	return NL_SKIP;
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg)
{
	fprintf(stderr, "Error handler\n");
	int *ret = (int*) arg;
	*ret = err->error;
	return NL_STOP;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
	fprintf(stderr, "Finish handler\n");
	int *ret = (int*) arg;
	*ret = 0;
	return NL_SKIP;
}

static int ack_handler(struct nl_msg *msg, void *arg)
{
	fprintf(stderr, "Ack handler\n");
	int *ret = (int*) arg;
	*ret = 0;
	return NL_STOP;
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

int main()
{
	struct nl80211_state nlstate;
	struct nl_cb *cb;
	struct nl_cb *s_cb;
	struct nl_msg *msg;
	const bool debug = true;

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
		return -1;
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
		return -1;
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
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

	// Receive a set of messages from the netlink socket
	while (err > 0)
		nl_recvmsgs((nl_handle*) nlstate.nl_sock, cb);

	nl_cb_put(cb);
	nlmsg_free(msg);

	nl80211_cleanup(&nlstate);
/*
	struct timeval start, end;
	gettimeofday(&start, NULL);

	std::vector<NetworkAccessPoint> accessPoints;

	gettimeofday(&end, NULL);
	long ms = (end.tv_sec  - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;

	printf("Scanning took %ldms\n", ms);
*/
	return 0;
}
