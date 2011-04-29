//============================================================================
// Name        : Radiolocate.cpp
// Author      : 
// Version     :
// Copyright   : Copyright (C) 2011 Garrett Brown <gbruin@ucla.edu>
// Description : Real-time locating system by radiation analysis
//============================================================================



#include <sys/ioctl.h>
#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>

//#include <linux/if.h>
#include <linux/wireless.h>
//#include <linux/sockios.h>

#include <errno.h>
//#include <resolv.h>

//#include <net/if_arp.h>

#include <cstring>
#include <math.h>
#include <sys/time.h>

#include "NetworkAccessPoint.h"
#include "StdString.h"
#include "Radiolocate.h"

using namespace std;

int main()
{
	struct timeval start, end;

	int sock = socket(AF_INET, SOCK_DGRAM, 0);

	gettimeofday(&start, NULL);

	std::vector<NetworkAccessPoint> accessPoints = GetAccessPoints("wlan0", sock);

	gettimeofday(&end, NULL);

	long ms = (end.tv_sec  - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;

	printf("Scanning took %ldms\n", ms);
	/*
	for (unsigned int i = 0; i < accessPoints.size(); ++i)
	{
		if (!accessPoints[i].getEssId().Equals("Triangle Fraternity UCLA"))
			continue;
		printf(accessPoints[i].toJson().c_str());
		printf("\n");
	}
	/*
	CStdString essid("Triangle Fraternity UCLA");
	CStdString macAddress("00-00-00-00-00-00");
	NetworkAccessPoint ap(essid, macAddress, -10, ENC_WPA, 9);
	cout << sock << endl;
	/**/

	if (sock != -1)
		close(sock);
	return 0;
}

void SetScanParams(iwreq &iwr, const char interface[], void *pointer, __u16 length, __u16 flags)
{
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, interface, IFNAMSIZ);
	if (pointer)
	{
		iwr.u.data.pointer = pointer;
		iwr.u.data.length = length;
	}
	if (flags)
		iwr.u.data.flags = flags;
}

// Query the wireless extension's version number. It will help us when we
// parse the resulting event
unsigned char GetWextVersion(const char interfaceName[], int sock)
{
	struct iwreq iwr;
	char rangebuffer[sizeof(iw_range) * 2];  // Large enough
	memset(rangebuffer, 0, sizeof(rangebuffer));

	SetScanParams(iwr, interfaceName, (caddr_t) rangebuffer, sizeof(rangebuffer));
	if (ioctl(sock, SIOCGIWRANGE, &iwr) < 0)
	{
		fprintf(stderr, "ERROR: %-8.16s Driver has no Wireless Extension version information\n", interfaceName);
		return 0;
	}
	return ((struct iw_range*) rangebuffer)->we_version_compiled;
}


std::vector<NetworkAccessPoint> GetAccessPoints(const char interface[], int sock)
{
	struct iwreq iwr;                   // Wireless scan structure
	unsigned char* res_buf = NULL;      // Buffer to hold the results of the scan
	int res_buf_len = IW_SCAN_MAX_DATA; // Min for compat WE<17
	int timeout = 15 * 1000 * 1000;     // 15s
	struct timeval tv;                  // Select timeout

	std::vector<NetworkAccessPoint> result;

	/********************************/
	long ms;
	struct timeval t_start, t_end;
	gettimeofday(&t_start, NULL);
	/********************************/

	unsigned char wireless_extension_version = GetWextVersion(interface, sock);
	if (!wireless_extension_version)
		return result;

	// Scan for wireless access points
	SetScanParams(iwr, interface); // Set scan options (SSID, channel, etc) here
	if (ioctl(sock, SIOCSIWSCAN, &iwr) < 0)
	{
		// Triggering scanning is a privileged operation (root only)
		if (errno == EPERM)
			fprintf(stderr, "ERROR: Cannot initiate wireless scan: ioctl[SIOCSIWSCAN]: %s. Try running with sudo\n", strerror(errno));
		else
			fprintf(stderr, "ERROR: Cannot initiate wireless scan: ioctl[SIOCSIWSCAN]: %s\n", strerror(errno));
		return result;
	}

	/********************************/
	gettimeofday(&t_end, NULL);
	ms = (t_end.tv_sec  - t_start.tv_sec) * 1000 + (t_end.tv_usec - t_start.tv_usec) / 1000;
	printf("%ldms - Initial scan request\n", ms);
	/********************************/

	// Init timeout value -> 250ms between set and first get
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	while (1)
	{
		if (tv.tv_sec > 0 || tv.tv_usec > 0)
		{
			// We must regenerate rfds (file descriptors for select) each time
			fd_set rfds;
			FD_ZERO(&rfds);
			FD_SET(sock, &rfds); // Add the rtnetlink fd in the list

			// Wait until something happens
			int ret = select(sock + 1, &rfds, NULL, NULL, &tv);

			// Check if there was an error
			if(ret < 0)
			{
				/********************************/
				gettimeofday(&t_end, NULL);
				ms = (t_end.tv_sec  - t_start.tv_sec) * 1000 + (t_end.tv_usec - t_start.tv_usec) / 1000;
				printf("%ldms - select() interrupted\n", ms);
				/********************************/

				// TODO: same tv.tv_usec?
				if(errno == EAGAIN || errno == EINTR)
					continue;
				fprintf(stderr, "ERROR: Unhandled signal in select() - exiting...\n");
				if(res_buf)
					free(res_buf);
				return result;
			}
			else if (ret > 0)
			{
				/********************************/
				gettimeofday(&t_end, NULL);
				ms = (t_end.tv_sec  - t_start.tv_sec) * 1000 + (t_end.tv_usec - t_start.tv_usec) / 1000;
				printf("%ldms - select() returned %d?\n", ms, ret);
				/********************************/

				//TODO
				//continue;

				if(res_buf)
					free(res_buf);
				return result;
			}
			else
			{
				/********************************/
				gettimeofday(&t_end, NULL);
				ms = (t_end.tv_sec  - t_start.tv_sec) * 1000 + (t_end.tv_usec - t_start.tv_usec) / 1000;
				printf("%ldms - select() timed out\n", ms);
				/********************************/
			}
		}

		// (Re)allocate the buffer - realloc(NULL, len) == malloc(len)
		unsigned char* new_buf = (unsigned char*)realloc(res_buf, res_buf_len);
		if (new_buf == NULL)
		{
			if(res_buf)
				free(res_buf);
			fprintf(stderr, "ERROR: Cannot alloc memory for wireless scanning (%lu bytes)\n", (unsigned long)res_buf_len);
			return result;
		}
		res_buf = new_buf;

		// Try to read the scan results
		SetScanParams(iwr, interface, res_buf, res_buf_len);
		int x = ioctl(sock, SIOCGIWSCAN, &iwr);

		/********************************/
		gettimeofday(&t_end, NULL);
		ms = (t_end.tv_sec  - t_start.tv_sec) * 1000 + (t_end.tv_usec - t_start.tv_usec) / 1000;
		printf("%ldms - Subsequent scan\n", ms);
		/********************************/


		// Get the results of the scanning. Three scenarios:
		//    1. We have the results, go to process them [ioctl() returned 0 (success)]
		//    2. We need to use a bigger result buffer (E2BIG)
		//    3. The scanning is not complete (EAGAIN) and we need to try again
		if (x == 0)
			break; // We have the results, go to process them

		if (errno == E2BIG)
		{
			if (res_buf_len < 100000)
			{
				// Check if the driver gave us any hints
				if (iwr.u.data.length > res_buf_len)
					res_buf_len = iwr.u.data.length;
				else
					res_buf_len *= 2;
				fprintf(stderr, "DEBUG: Scan results did not fit - trying larger buffer (%lu bytes)\n", (unsigned long)res_buf_len);
				tv.tv_sec = tv.tv_usec = 0; // Use the same results, don't try to get new ones
				continue;
			}
			else
				fprintf(stderr, "ERROR: Scan results exceeded buffer length (%lu bytes)\n", (unsigned long)res_buf_len);
		}
		else if (errno == EAGAIN)
		{
			// Restart timer for only 100ms
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			timeout -= tv.tv_usec;
			if (timeout >= 0)
				continue;
			else
				fprintf(stderr, "ERROR: Wireless scan results timed out after %d seconds\n", timeout / 1000 / 1000);
		}
		else // Bad error
			fprintf(stderr, "ERROR: Cannot get wireless scan results: ioctl[SIOCGIWSCAN]: %d - %s\n", errno, strerror(errno));

		if (res_buf)
			free(res_buf);
		return result;
	}

	size_t len = iwr.u.data.length;           // total length of the wireless events from the scan results
	char* pos = (char *) res_buf;             // pointer to the current event (about 12 per wireless network)
	char* end = (char *) res_buf + len;       // marks the end of the scan results
	char* custom;                             // pointer to the event payload
	struct iw_event iwe_buf, *iwe = &iwe_buf; // buffer to hold individual events

	CStdString essId;
	CStdString macAddress;
	int signalLevel = 0;
	EncMode encryption = ENC_NONE;
	int channel = 0;

	while (pos + IW_EV_LCP_LEN <= end)
	{
		/* Event data may be unaligned, so make a local, aligned copy
		* before processing. */

		// copy event prefix (size of event minus IOCTL fixed payload)
		memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
		if (iwe->len <= IW_EV_LCP_LEN)
			break;

		// if the payload is nontrivial (i.e. > 16 octets) assume it comes after a pointer
		custom = pos + IW_EV_POINT_LEN;
		if (wireless_extension_version > 18 &&
		    (iwe->cmd == SIOCGIWESSID ||
		     iwe->cmd == SIOCGIWENCODE ||
		     iwe->cmd == IWEVGENIE ||
		     iwe->cmd == IWEVCUSTOM))
		{
			/* Wireless extensions v19 removed the pointer from struct iw_point */
			char *data_pos = (char *) &iwe_buf.u.data.length;
			int data_len = data_pos - (char *) &iwe_buf;
			memcpy(data_pos, pos + IW_EV_LCP_LEN, sizeof(struct iw_event) - data_len);
		}
		else
		{
			// copy the rest of the event and point custom toward the payload offset
			memcpy(&iwe_buf, pos, sizeof(struct iw_event));
			custom += IW_EV_POINT_OFF;
		}

		// Interpret the payload based on event type. Each access point generates ~12 different events
		switch (iwe->cmd)
		{
			// Get access point MAC addresses
			case SIOCGIWAP:
			{
				// This even marks a new access point, so push back the old information
				if (!macAddress.IsEmpty())
					result.push_back(NetworkAccessPoint(essId, macAddress, signalLevel, encryption, channel));
				unsigned char* mac = (unsigned char*)iwe->u.ap_addr.sa_data;
				// macAddress is big-endian, write in byte chunks
				macAddress.Format("%02x-%02x-%02x-%02x-%02x-%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
				// Reset the remaining fields
				essId = "";
				encryption = ENC_NONE;
				signalLevel = 0;
				channel = 0;
				break;
			}

			// Get operation mode
			case SIOCGIWMODE:
				// Ignore Ad-Hoc networks (1 is the magic number for this)
				if (iwe->u.mode == 1)
					macAddress = "";
				break;

			// Get ESSID
			case SIOCGIWESSID:
			{
				char essid[IW_ESSID_MAX_SIZE+1];
				memset(essid, '\0', sizeof(essid));
				if ((custom) && (iwe->u.essid.length))
				{
					memcpy(essid, custom, iwe->u.essid.length);
					essId = essid;
				}
				break;
			}

			// Quality part of statistics
			case IWEVQUAL:
				// u.qual.qual is scaled to a vendor-specific RSSI_Max, so use u.qual.level
				signalLevel = iwe->u.qual.level - 0x100; // and remember we use 8-bit arithmetic
				break;

			// Get channel/frequency (Hz)
			case SIOCGIWFREQ: // This gets called twice per network -- need to find the difference between the two!
			{
				float freq = ((float)iwe->u.freq.m) * pow(10, iwe->u.freq.e);
				if (freq > 1000)
					channel = NetworkAccessPoint::FreqToChannel(freq);
				else
					channel = (int)freq; // Some drivers report channel instead of frequency
				break;
			}

			// Get encoding token & mode
			case SIOCGIWENCODE:
				if (!(iwe->u.data.flags & IW_ENCODE_DISABLED) && encryption == ENC_NONE)
					encryption = ENC_WEP;
				break;

			// Generic IEEE 802.11 information element (IE) for WPA, RSN, WMM, ...
			case IWEVGENIE:
			{
				int offset = 0;

				// Loop on each IE, each IE is minimum 2 bytes
				while (offset <= (iwe_buf.u.data.length - 2))
				{
					switch ((unsigned char)custom[offset])
					{
						case 0xdd: /* WPA1 */
							if (encryption != ENC_WPA2)
								encryption = ENC_WPA;
							break;
						case 0x30: /* WPA2 */
							encryption = ENC_WPA2;
					}
					// Skip over this IE to the next one in the list
					offset += (unsigned char)custom[offset+1] + 2;
				}
			}
		}

		pos += iwe->len;
	}

	if (!essId.IsEmpty())
		result.push_back(NetworkAccessPoint(essId, macAddress, signalLevel, encryption, channel));

	free(res_buf);
	res_buf = NULL;

	return result;
}

