/*
 * NetworkAccessPoint.h
 *
 *  Created on: April 26, 2011
 *      Author: Garrett
 */

#ifndef NETWORKACCESSPOINT_H_
#define NETWORKACCESSPOINT_H_

#include "StdString.h"

enum EncMode { ENC_NONE = 0, ENC_WEP = 1, ENC_WPA = 2, ENC_WPA2 = 3 };
enum NetworkAssignment { NETWORK_DASH = 0, NETWORK_DHCP = 1, NETWORK_STATIC = 2, NETWORK_DISABLED = 3 };

class NetworkAccessPoint
{
public:
   NetworkAccessPoint(CStdString& essId, CStdString& macAddress, int signalStrength, EncMode encryption, int channel = 0)
   {
      m_essId = essId;
      m_macAddress = macAddress;
      m_dBm = signalStrength;
      m_encryptionMode = encryption;
      m_channel = channel;
   }

   CStdString getEssId() { return m_essId; }
   CStdString getMacAddress() { return m_macAddress; }
   int getSignalStrength() { return m_dBm; }
   EncMode getEncryptionMode() { return m_encryptionMode; }
   int getChannel() { return m_channel; }

   /* Returns the quality, normalized as a percentage, of the network access point */
   int getQuality();

   /* Returns a Google Gears specific JSON string */
   CStdString toJson();

   /* Translates a 802.11a+b frequency into corresponding channel */
   static int FreqToChannel(float frequency);

private:
   CStdString   m_essId;
   CStdString   m_macAddress;
   int          m_dBm;
   EncMode      m_encryptionMode;
   int          m_channel;
};


#endif /* NETWORKACCESSPOINT_H_ */
