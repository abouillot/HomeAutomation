/*
RFM69 Sender Reciever Test
by Alexandre Bouillot
adapted for a simple send/receive by Christian van der Leeden

License:  CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
Date:  2015-12-03
File: SenderReciever.c

Define the RFM Frequency of the chip used, your network id (pick one) and the encryption key 
*/

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
#include <stdint.h>

#include <wiringPi.h>

#include "../Common/include/rfm69/rfm69.h"
#include "../Common/include/RFM69NetworkCfg.h"
#include "../Common/include/RFM69HomeAutomationCfg.h"


//general --------------------------------
#define DEBUG

#ifdef DAEMON
	#define LOG(...) do { syslog(LOG_INFO, __VA_ARGS__); } while (false)
	#define LOG_E(...) do { syslog(LOG_ERR, __VA_ARGS__); } while (false)
	#define LOG_MACRO_NOT_EMPTY
#else
	#ifdef DEBUG
		#define LOG(...) do { printf(__VA_ARGS__); } while (false)
		#define LOG_E(...) do { printf(__VA_ARGS__); } while (false)
		#define LOG_MACRO_NOT_EMPTY
	#else
		#define LOG(...)
		#define LOG_E(...)
	#endif
#endif


const unsigned int  Desired_Data_Vals_Per_Line = 16;
const std::size_t   Max_Data_Vals_Per_Line     = 16;
static_assert( Desired_Data_Vals_Per_Line <= Max_Data_Vals_Per_Line, "Requesting hexDump() to show more data per line than allowed" );


// RFM69  ----------------------------------
#define THIS_RFM69_IS_HIGH_POWER    // Uncomment for RFM69HW/RFM69HCW/etc., comment out if you have RFM69W/RFM69CW/etc.

const bool          Use_Promiscuous_Mode                      = true;
const int8_t        This_RFM69_Temperature_Calibration_Factor = -4;
const unsigned long Watchdog_Timeout_Delay                    = 1800000;

Packet_Data         the_RFM69_Packet;
RFM69*              the_RFM69;


static void initRFM69 ( void );
//static long millis(void);
static void hexDump  ( const char *desc, void *data_addr, unsigned int data_len, unsigned int max_data_vals_per_line );
static void send_message();
static int run_loop();


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
  } else if (strcmp(argv[1], "-s") == 0) {
    mode = sender;
  } else {
    fprintf(stderr, "invalid arguments");
    uso();
  }


	//RFM69 ---------------------------
	LOG("NETWORK %d NODE_ID %d FREQUENCY %d\n", The_Network_ID, The_Gateway_Node_ID, Frequency_Band);
	
	the_RFM69 = new RFM69();
	the_RFM69->initialize( Frequency_Band, The_Gateway_Node_ID, The_Network_ID );
	initRFM69();
	
	LOG("setup complete\n");
	return run_loop();
}

int counter = 0;

/* Loop until it is explicitly halted or the network is lost, then clean up. */
static int run_loop() {
	for (;;)
	{
//		if (mode == receiver && the_RFM69->receiveDone())
		if (the_RFM69->receiveDone())
		{
			LOG("Received something...\n");
//			blinkLED( The_Rx_LED_Pin );
			// store the received data localy, so they can be overwited
			// This will allow to send ACK immediately after
			uint8_t data[RF69_MAX_DATA_LEN]; // recv/xmit buf, including header & crc bytes
			uint8_t dataLength = the_RFM69->DATALEN;
			memcpy(data, (void *)the_RFM69->DATA, dataLength);
#if defined (DEBUG)
			uint8_t theNodeID = the_RFM69->SENDERID;
#endif			
			uint8_t targetID = the_RFM69->TARGETID; // should match _address
//			uint8_t PAYLOADLEN = the_RFM69->PAYLOADLEN;
			uint8_t ACK_REQUESTED = the_RFM69->ACK_REQUESTED;
//			uint8_t ACK_RECEIVED = the_RFM69->ACK_RECEIVED; // should be polled immediately after sending a packet with ACK request
#if defined (DEBUG)
			int16_t RSSI = the_RFM69->RSSI; // most accurate RSSI during reception (closest to the reception)
#endif
			LOG("ACK REQUESTED: %d, targetID %d, theConfig.nodeId %d\n", ACK_REQUESTED, targetID, The_Gateway_Node_ID);
			if (ACK_REQUESTED  && targetID == The_Gateway_Node_ID)
			{
				// When a node requests an ACK, respond to the ACK
				// but only if the Node ID is correct
				the_RFM69->sendACK();
//				blinkLED( The_Tx_LED_Pin );
			}//end if radio.ACK_REQESTED
	
			LOG("[%d] to [%d] ", theNodeID, targetID);

			if (dataLength != sizeof(Packet_Data))
			{
				LOG("Invalid payload received, not matching Payload struct! %d - %d\r\n", dataLength, sizeof(Packet_Data));
				hexDump( NULL, data, dataLength, Desired_Data_Vals_Per_Line );		
			} else
			{
				the_RFM69_Packet = *(Packet_Data*)data; //assume radio.DATA actually contains our struct and not something else

				LOG("Received: Source Node ID=%03u - RSSI=%+04d - Data Type=%03u\n",
					the_RFM69_Packet.source_Node_ID,
					RSSI,
					the_RFM69_Packet.data_Type
				);

				switch( the_RFM69_Packet.data_Type )
				{
					case Test_Packet_Data_Type:
						LOG( "  Test packet> Sequence Number=%03d\n\n", the_RFM69_Packet.the_Data.test_Packet.packet_Sequence_Number );
						break;

						case Temperature_Data_Type:
						LOG( "  Temperature packet> Device ID=%03u - Sequence Number=%03d - Temperature=%+07.2f\n\n", the_RFM69_Packet.the_Data.temperature.device_ID,
						        the_RFM69_Packet.the_Data.temperature.packet_Sequence_Number, the_RFM69_Packet.the_Data.temperature.temperature );
						break;

					default:
						LOG( "  **Unknown packet data type**\n\n" );
						break;
				}
			}  
		} //end if radio.receive
		
	if (mode == sender)
	{
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


static void initRFM69( void )
{
	the_RFM69->restart( Frequency_Band, The_Gateway_Node_ID, The_Network_ID );
    
    #if defined (THIS_RFM69_IS_HIGH_POWER)
		the_RFM69->setHighPower();
    #endif
    
	#if defined (USE_ENCRYPTION)
		the_RFM69->encrypt( (char *)The_Encryption_Key );
    #endif
    
	the_RFM69->promiscuous( Use_Promiscuous_Mode );
    
	LOG( "RFM69: listening at %d Mhz...\n", Frequency_Band==RF69_433MHZ ? 433 : Frequency_Band==RF69_868MHZ ? 868 : 915 );
}


//static long millis(void) {
//	struct timeval tv;
//
//    gettimeofday(&tv, NULL);
//
//    return ((tv.tv_sec) * 1000 + tv.tv_usec/1000.0) + 0.5;
//	}

	
/* Binary Dump utility function */
static void hexDump( const char* desc, void* data_addr, unsigned int data_len_remaining, unsigned int num_data_vals_per_line )
{
#if defined (LOG_MACRO_NOT_EMPTY)
    const unsigned char Hex_Asc[]              = "0123456789ABCDEF";

    unsigned char hexbuf[(Max_Data_Vals_Per_Line * 3) + 1];	// Data as hex values (2 char + 1 space)
	unsigned char ascbuf[Max_Data_Vals_Per_Line + 1];	    // Data as ASCII characters
	unsigned int  num_data_vals_this_line;

	unsigned int  num_lines    = 0;
	

	// nothing to output
	if( data_len_remaining == 0 )
		return;

	// Limit the line length to Max
	if( num_data_vals_per_line > Max_Data_Vals_Per_Line ) 
		num_data_vals_per_line = Max_Data_Vals_Per_Line;
		
	// Output description if given.
    if( desc != NULL )
		LOG( "%s:\n", desc );
	
	do
	{
		unsigned int hexbuf_index = 0;
		unsigned int ascbuf_index = 0;

		num_data_vals_this_line = data_len_remaining - num_data_vals_per_line;
		if( num_data_vals_this_line > num_data_vals_per_line )
			num_data_vals_this_line = num_data_vals_per_line;

		data_len_remaining -= num_data_vals_this_line;

		unsigned int hex_vals_padding = (num_data_vals_per_line - num_data_vals_this_line) * 3;
	
		while( num_data_vals_this_line-- > 0 )
		{
			uint8_t data_val = *( static_cast<uint8_t *>(data_addr) );
			data_addr = static_cast<uint8_t*>(data_addr) + 1;

			hexbuf[hexbuf_index++] = Hex_Asc[((data_val) & 0xF0) >> 4];
			hexbuf[hexbuf_index++] = Hex_Asc[((data_val) & 0x0F)];
			hexbuf[hexbuf_index++] = ' ';
		
			ascbuf[ascbuf_index++] = ((data_val > 0x1F) && (data_val < 0x7F)) ? data_val : '.';
		}
	
		while( hex_vals_padding-- > 0 )
		{
			hexbuf[hexbuf_index++] = ' ';
		}

		// nul terminate both buffer
		hexbuf[hexbuf_index] = '\0';
		ascbuf[ascbuf_index] = '\0';
	
		// output buffers
		LOG("%04x: %s   %s\n", num_lines, hexbuf, ascbuf);
		
		num_lines++;
	} while( data_len_remaining > 0 );
#endif	
}


static void send_message() {
	uint8_t retries = 0;
	uint8_t wait_time = 255;
	Packet_Data data;
    data.target_Node_ID = 2;
	data.source_Node_ID = The_Gateway_Node_ID;
	data.the_Data.temperature.device_ID= 10;
	data.the_Data.temperature.packet_Sequence_Number = 100;
	data.the_Data.temperature.temperature = 99.0;
	
	LOG("Will Send message to Node ID = %d Device ID = %d Sequence # = %d  Temperature = %f\n",
		data.target_Node_ID,
		data.the_Data.temperature.device_ID,
		data.the_Data.temperature.packet_Sequence_Number,
		data.the_Data.temperature.temperature
	);

	
	if (the_RFM69->sendWithRetry(data.target_Node_ID,(const void*)(&data),sizeof(data), retries, wait_time)) {
			LOG("\n\nOK: Message sent to node %d ACK\n\n", data.target_Node_ID);
	}
	else {
		LOG("\n\nERROR: essage sent to node %d NAK \n\n", data.target_Node_ID);
	}
}
