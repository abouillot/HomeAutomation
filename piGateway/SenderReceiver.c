/*
RFM69 Sender Reciever Test
by Alexandre Bouillot
adapted for a simple send/receive by Christian van der Leeden

License:  CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
Date:  2015-12-03
File: SenderReciever.c

Define the RFM Frequency of the chip used, your network id (pick one) and the encryption key 
*/

//general --------------------------------
#define LOG(...) do { printf(__VA_ARGS__); } while (0)

/* CONFIGURATION, please adapt */
#include "networkconfig.h"


//RFM69  ----------------------------------
#include "rfm69.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>



RFM69 *rfm69;

typedef struct {
	uint8_t networkId;
	uint8_t nodeId;
	uint8_t frequency; // RF69_433MHZ RF69_868MHZ RF69_915MHZ
	uint8_t keyLength; // set to 0 for no encryption
	char key[16];
	bool isRFM69HW;
	bool promiscuousMode;
}
Config;
Config theConfig;

typedef struct {		
	short           nodeID; 
	short			sensorID;
	unsigned long   var1_usl; 
	float           var2_float; 
	float			var3_float;	
} 
Payload;
Payload theData;


static void die(const char *msg);
static long millis(void);
static void hexDump (char *desc, void *addr, int len, int bloc);

static int initRfm(RFM69 *rfm);
static void send_message();
static int run_loop();

static int RECEIVER_ID = 10;
static int SENDER_ID = 11;
static int NODE_ID;
static int GATEWAY_ID;

enum Mode {
  sender,
  receiver
}; 
enum Mode mode;


static void uso(void) {
	fprintf(stderr, "Use:\n -s for sender, -r for receiver \n");
	exit(1);
}

int main(int argc, char* argv[]) {
	if (argc != 2) uso();
  if (strcmp(argv[1], "-r") == 0 ) {
    mode = receiver;
    NODE_ID = RECEIVER_ID;
    GATEWAY_ID = SENDER_ID; // not really needed...
  } else if (strcmp(argv[1], "-s") == 0) {
    mode = sender;
    NODE_ID = SENDER_ID;
    GATEWAY_ID = RECEIVER_ID;
  } else {
    fprintf(stderr, "invalid arguments");
    uso();
  }


	//RFM69 ---------------------------
	theConfig.networkId = NWC_NETWORK_ID;
	theConfig.nodeId = NODE_ID;
	theConfig.frequency = NWC_FREQUENCY;
	theConfig.keyLength = NWC_KEY_LENGTH;
	memcpy(theConfig.key, NWC_KEY, NWC_KEY_LENGTH);
	theConfig.isRFM69HW = NWC_RFM69H;
	theConfig.promiscuousMode = NWC_PROMISCUOUS_MODE;

	LOG("NETWORK %d NODE_ID %d FREQUENCY %d\n", theConfig.networkId, theConfig.nodeId, theConfig.frequency);
	
	rfm69 = new RFM69();
	rfm69->initialize(theConfig.frequency,theConfig.nodeId,theConfig.networkId);
	initRfm(rfm69);
	
	LOG("setup complete\n");
	return run_loop();
}

int counter = 0;

/* Loop until it is explicitly halted or the network is lost, then clean up. */
static int run_loop() {
	for (;;) {
		if (mode == receiver && rfm69->receiveDone()) {
			LOG("Received something...\n");
			// store the received data localy, so they can be overwited
			// This will allow to send ACK immediately after
			uint8_t data[RF69_MAX_DATA_LEN]; // recv/xmit buf, including header & crc bytes
			uint8_t dataLength = rfm69->DATALEN;
			memcpy(data, (void *)rfm69->DATA, dataLength);
			uint8_t theNodeID = rfm69->SENDERID;
			uint8_t targetID = rfm69->TARGETID; // should match _address
			uint8_t PAYLOADLEN = rfm69->PAYLOADLEN;
			uint8_t ACK_REQUESTED = rfm69->ACK_REQUESTED;
			uint8_t ACK_RECEIVED = rfm69->ACK_RECEIVED; // should be polled immediately after sending a packet with ACK request
			int16_t RSSI = rfm69->RSSI; // most accurate RSSI during reception (closest to the reception)
			LOG("ACK REQUESTED: %d, targetID %d, theConfig.nodeId %d\n", ACK_REQUESTED, targetID, theConfig.nodeId);
			if (ACK_REQUESTED  && targetID == theConfig.nodeId) {
				// When a node requests an ACK, respond to the ACK
				// but only if the Node ID is correct
				rfm69->sendACK();
			}//end if radio.ACK_REQESTED
	
			LOG("[%d] to [%d] ", theNodeID, targetID);

			if (dataLength != sizeof(Payload)) {
				LOG("Invalid payload received, not matching Payload struct! %d - %d\r\n", dataLength, sizeof(Payload));
				hexDump(NULL, data, dataLength, 16);		
			} else {
				theData = *(Payload*)data; //assume radio.DATA actually contains our struct and not something else

				LOG("Received Node ID = %d Device ID = %d Time = %d  RSSI = %d var2 = %f var3 = %f\n",
					theData.nodeID,
					theData.sensorID,
					theData.var1_usl,
					RSSI,
					theData.var2_float,
					theData.var3_float
				);
			}  
		} //end if radio.receive
		
    if (mode == sender) {
  		counter = counter + 1;
  		if (counter % 20 == 0) {
  			LOG("Sending test message\n");
  			send_message();
  		} else {
  			usleep(100*1000);
  		}
    } 
		
	}

}

static int initRfm(RFM69 *rfm) {
	rfm->restart(theConfig.frequency,theConfig.nodeId,theConfig.networkId);
	if (theConfig.isRFM69HW)
		rfm->setHighPower(); //uncomment only for RFM69HW!
	if (theConfig.keyLength)
		rfm->encrypt(theConfig.key);
	rfm->promiscuous(theConfig.promiscuousMode);
	LOG("Listening at %d Mhz...\n", theConfig.frequency==RF69_433MHZ ? 433 : theConfig.frequency==RF69_868MHZ ? 868 : 915);
}

/* Fail with an error message. */
static void die(const char *msg) {
	fprintf(stderr, "%s", msg);
	exit(1);
}

static long millis(void) {
	struct timeval tv;

    gettimeofday(&tv, NULL);

    return ((tv.tv_sec) * 1000 + tv.tv_usec/1000.0) + 0.5;
	}

	
/* Binary Dump utility function */
#define MAX_BLOC 16
const unsigned char hex_asc[] = "0123456789abcdef";
static void hexDump (char *desc, void *addr, int len, int bloc) {
    int i, lx, la, l, line;
	long offset = 0;
    unsigned char hexbuf[MAX_BLOC * 3 + 1];	// Hex part of the data (2 char + 1 space)
	unsigned char ascbuf[MAX_BLOC + 1];	// ASCII part of the data
    unsigned char *pc = (unsigned char*)addr;
	unsigned char ch;
	
	// nothing to output
	if (!len)
		return;

	// Limit the line length to MAX_BLOC
	if (bloc > MAX_BLOC) 
		bloc = MAX_BLOC;
		
	// Output description if given.
    if (desc != NULL)
		LOG("%s:\n", desc);
	
	line = 0;
	do
		{
		l = len - (line * bloc);
		if (l > bloc)
			l = bloc;
	
		for (i=0, lx = 0, la = 0; i < l; i++) {
			ch = pc[i];
			hexbuf[lx++] = hex_asc[((ch) & 0xF0) >> 4];
			hexbuf[lx++] = hex_asc[((ch) & 0xF)];
			hexbuf[lx++] = ' ';
		
			ascbuf[la++]  = (ch > 0x20 && ch < 0x7F) ? ch : '.';
			}
	
		for (; i < bloc; i++) {
			hexbuf[lx++] = ' ';
			hexbuf[lx++] = ' ';
			hexbuf[lx++] = ' ';
		}	
		// nul terminate both buffer
		hexbuf[lx++] = 0;
		ascbuf[la++] = 0;
	
		// output buffers
		LOG("%04x %s %s\n", line * bloc, hexbuf, ascbuf);
		
		line++;
		pc += bloc;
		}
	while (line * bloc < len);
}



static void send_message() {
	uint8_t retries = 0;
	uint8_t wait_time = 255;
	Payload data;
	uint8_t network;
	data.nodeID = GATEWAY_ID;
	data.sensorID = 10;
	data.var1_usl = 1000;
	data.var2_float = 99.0;
	data.var3_float = 101.0;
	
	LOG("Will Send message to Node ID = %d Device ID = %d Time = %d  var2 = %f var3 = %f\n",
		data.nodeID,
		data.sensorID,
		data.var1_usl,
		data.var2_float,
		data.var3_float
	);

	if (rfm69->sendWithRetry(data.nodeID,(const void*)(&data),sizeof(data), retries, wait_time)) {
		LOG("\n\nOK: Message sent to node %d ACK\n\n", data.nodeID);
	}
	else {
		LOG("\n\nERROR: essage sent to node %d NAK \n\n", data.nodeID);
	}
}

