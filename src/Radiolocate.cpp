//============================================================================
// Name        : Radiolocate.cpp
// Author      : 
// Version     :
// Copyright   : Copyright (C) 2011 Garrett Brown <gbruin@ucla.edu>
// Description : Real-time locating system by radiation analysis
//============================================================================

//#include <time.h>

// First install libnl-dev
//#include <netlink/genl/genl.h>
//#include <netlink/genl/family.h>
//#include <netlink/genl/ctrl.h>
//#include <netlink/msg.h>
//#include <netlink/attr.h>

#include "NetworkAccessPoint.h"
#include "nl80211.h"

using namespace std;

int main()
{
	struct timeval start, end;
	gettimeofday(&start, NULL);

	std::vector<NetworkAccessPoint> accessPoints;

	gettimeofday(&end, NULL);
	long ms = (end.tv_sec  - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;

	printf("Scanning took %ldms\n", ms);

	return 0;
}
