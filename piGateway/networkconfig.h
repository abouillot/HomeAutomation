/*
RFM69 Gateway RFM69 pushing the data to the mosquitto server
by Alexandre Bouillot

License:  CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
Date:  2015-06-12
File: Gateway.c

This file hold the network configuration
*/

#define NWC_NETWORK_ID 101
#define NWC_NODE_ID 1
// Frequency should be one of RF69_433MHZ RF69_868MHZ RF69_915MHZ
#define NWC_FREQUENCY RF69_433MHZ
// Set to 0 to disable encryption
#define NWC_KEY_LENGTH 16
// Must contain 16 characters
#define NWC_KEY "xxxxxxxxxxxxxxxx"
// Set to true is the RFM69 is high power, false otherwise
#define NWC_RFM69H true
// Set to true if you want to listen to all messages on the network, even if for  different nodes
#define NWC_PROMISCUOUS_MODE true
// Set the delay before reinitializing the RFM69 module if no  message received in the interval
#define NWC_WATCHDOG_DELAY 1800000
