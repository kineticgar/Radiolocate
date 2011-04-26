/*
 * NetworkAccessPoint.cpp
 *
 *  Created on: April 26, 2011
 *      Author: Garrett Brown
 */

#include "NetworkAccessPoint.h"


/* Returns the quality, normalized as a percentage, of the network access point */
int NetworkAccessPoint::getQuality()
{
  // Cisco dBm lookup table (partially nonlinear)
  // Source: Converting Signal Strength Percentage to dBm Values, 2002
  int quality;
  if (m_dBm >= -10) quality = 100;
  else if (m_dBm >= -20) quality = 85 + (m_dBm + 20);
  else if (m_dBm >= -30) quality = 77 + (m_dBm + 30);
  else if (m_dBm >= -60) quality = 48 + (m_dBm + 60);
  else if (m_dBm >= -98) quality = 13 + (m_dBm + 98);
  else if (m_dBm >= -112) quality = 1 + (m_dBm + 112);
  else quality = 0;
  return quality;
}

/* Returns a Google Gears specific JSON string */
CStdString NetworkAccessPoint::toJson()
{
  CStdString jsonBuffer;
  if (m_macAddress.IsEmpty())
      return "{}";
  jsonBuffer.Format("{\"mac_address\":\"%s\"", m_macAddress.c_str());
  if (!m_essId.IsEmpty())
    jsonBuffer.AppendFormat(",\"ssid\":\"%s\"", m_essId.c_str());
  if (m_dBm < 0)
    jsonBuffer.AppendFormat(",\"signal_strength\":%d", m_dBm);
  if (m_channel != 0)
    jsonBuffer.AppendFormat(",\"channel\":%d", m_channel);
  jsonBuffer.append("}");
  return jsonBuffer;
}

/* Translates a 802.11a+b frequency into corresponding channel */
int NetworkAccessPoint::FreqToChannel(float frequency)
{
  int IEEE80211Freq[] = {2412, 2417, 2422, 2427, 2432,
                          2437, 2442, 2447, 2452, 2457,
                          2462, 2467, 2472, 2484,
                          5180, 5200, 5210, 5220, 5240, 5250,
                          5260, 5280, 5290, 5300, 5320,
                          5745, 5760, 5765, 5785, 5800, 5805, 5825};
  int IEEE80211Ch[] = {     1,    2,    3,    4,    5,
                            6,    7,    8,    9,   10,
                            11,   12,   13,   14,
                            36,   40,   42,   44,   48,   50,
                            52,   56,   58,   60,   64,
                          149,  152,  153,  157,  160,  161,  165};

  int mod_chan = (int)(frequency / 1000000 + 0.5f);
  for (unsigned int i = 0; i < sizeof(IEEE80211Freq) / sizeof(int); ++i)
  {
      if (IEEE80211Freq[i] == mod_chan)
        return IEEE80211Ch[i];
  }
  return 0; // unknown
}
