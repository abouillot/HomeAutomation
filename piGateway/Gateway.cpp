/*
RFM69 Gateway RFM69 pushing the data to the mosquitto server
by Alexandre Bouillot

License:  CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
Date:  2016/11/24
File: Gateway.c

This sketch receives RFM wireless data and forwards it to Mosquitto relay

The messages are published with the format RFM/<network number>/<node_id>/up/<sensor_id><var>
It also subscripe to Mosquitto Topics starting with RFM/<network_number>/<node_id>/down/<sensor_id>

The message is parsed and put back to the same payload structure as the one received from the nodes


Adjust network configuration to your setup in the file networkconfig.h
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
#include <signal.h>
#include <type_traits>

#include <wiringPi.h>
#include <mosquitto.h>

#include "../Common/include/rfm69/rfm69.h"
#include "../Common/include/RFM69NetworkCfg.h"
#include "../Common/include/RFM69HomeAutomationCfg.h"


//general --------------------------------

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


// RFM69  ----------------------------------
#define THIS_RFM69_IS_HIGH_POWER    // Uncomment for RFM69HW/RFM69HCW/etc., comment out if you have RFM69W/RFM69CW/etc.

const bool          Use_Promiscuous_Mode                      = true;
const int8_t        This_RFM69_Temperature_Calibration_Factor = -4;
const unsigned long Watchdog_Timeout_Delay                    = 1800000;
const unsigned long LED_On_Duration_Milliseconds              = 100;

static bool         Keep_Process_Alive                        = true;

Packet_Data         the_RFM69_Packet;
RFM69*              the_RFM69;


typedef struct {		
	unsigned long messageWatchdog;
	unsigned long messageSent;
	unsigned long messageReceived;
	unsigned long ackRequested;
	
	unsigned long ackReceived;
	unsigned long ackMissed;
	
	unsigned long ackCount;
} 
Stats;
Stats the_Stats;

static void initRFM69 ( void );
// ------------------------


// Mosquitto---------------
/* Hostname and port for the MQTT broker. */
const char*        MQTT_Broker_Hostname   = "localhost";
const char*        MQTT_Topic_Root        = "RFM69HomeAutomation";

const char         MQTT_Client_ID[]       = "RFM69HomePiGateway";
// Per MQTT specification v3.1.1, section 3.1.3.1, servers MUST accept client IDs of 1-23 characters (guaranteed
//  acceptable size), but MAY accept more (not guaranteed acceptable size); for other client ID restrictions, see
//  http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/csprd02/mqtt-v3.1.1-csprd02.html#_Toc385349767
static_assert( sizeof(MQTT_Client_ID) <= 24, "MQTT Client ID size not guaranteed to be acceptable for all servers" );

const int          Mosquitto_Timeout_Millis = 60;
const int          Mosquitto_Max_Packets    = 1;
const unsigned int MQTT_Broker_Port         = 1883;
/* How many seconds the broker should wait between sending out
 * keep-alive messages. */
const unsigned int MQTT_Keepalive_Seconds   = 60;
const std::size_t  MQTT_Max_Topic_Length    = 128;
const std::size_t  MQTT_Max_Message_Length  = 128;

static struct mosquitto* The_Mosquitto_Client;

static bool setMosquittoCallbacks   ( void );
static bool connectToMosquitto      ( void );

static void mqttFormOutputTopic     ( uint8_t node, uint8_t device, const char* subject, char* topic_buffer );
static void mosquittoPublishMessage ( const char* mqtt_topic, const char* mqtt_message );
// --------------------------


const unsigned int  Desired_Data_Vals_Per_Line = 16;
const std::size_t   Max_Data_Vals_Per_Line     = 16;
static_assert( Desired_Data_Vals_Per_Line <= Max_Data_Vals_Per_Line, "Requesting hexDump() to show more data per line than allowed" );

const unsigned long Temperature_Read_Interval  = 3000;


typedef enum {
    This_Node_Radio_Device_ID = 0
} Node_Device_ID;


static void die      ( const char *msg );
//static long millis   ( void );
static void hexDump  ( const char *desc, void *data_addr, unsigned int data_len, unsigned int max_data_vals_per_line );
static int  runLoop  ( void );


#if not defined(DAEMON)
static void uso( void )
{
	fprintf( stderr, "Use:\n Simply use it without args :D\n" );
	exit( 1 );
}
#endif


#ifdef DAEMON
/* Signal handler -- break out of the main loop */
void ShutdownSignalHandler( int sig )
{
        Keep_Process_Alive = false;
}
#endif


int main( int argc, char* argv[] )
{
	#ifdef DAEMON
		//Adapted from http://www.netzmafia.de/skripten/unix/linux-daemon-howto.html
		pid_t pid, sid;

		openlog("Gatewayd", LOG_PID, LOG_USER);

		pid = fork();
		if (pid < 0) {
			LOG_E("fork failed");
			exit(EXIT_FAILURE);
		}
		/* If we got a good PID, then
			 we can exit the parent process. */
		if (pid > 0) {
			LOG("Child spawned, pid %d\n", pid);
			exit(EXIT_SUCCESS);
		}

		/* Change the file mode mask */
		umask(0);

		/* Create a new SID for the child process */
		sid = setsid();
		if (sid < 0) {
			LOG_E("setsid failed");
			exit(EXIT_FAILURE);
		}
			
		/* Change the current working directory */
		if ((chdir("/")) < 0) {
		  LOG_E("chdir failed");
		  exit(EXIT_FAILURE);
		}
			
		/* Close out the standard file descriptors */
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

        /* Catch these signals for proper terminaton and clean up */
        {
            struct sigaction sig_action;

            memset( &sig_action, 0, sizeof(sig_action) );

            sig_action.sa_handler = ShutdownSignalHandler;
            sig_action.sa_flags   = 0; /* We block on usleep; don't use SA_RESTART */

            sigemptyset( &sig_action.sa_mask );

            sigaction( SIGHUP,  &sig_action, NULL );
            sigaction( SIGINT,  &sig_action, NULL );
            sigaction( SIGTERM, &sig_action, NULL );
        }
    #else
    	if( argc != 1 )
            uso();
	#endif //DAEMON


	// Mosquitto ----------------------
	The_Mosquitto_Client = mosquitto_new( MQTT_Client_ID, true, nullptr );
	if( The_Mosquitto_Client == NULL )
	{
		die( "mosquitto client create failure\n" );
	}

	if( !setMosquittoCallbacks() )
	{
		die( "set mosquitto callbacks failure\n" );
	}
	
	if( !connectToMosquitto() )
	{
		die( "mosquitto connect failure\n" );
	}


	//RFM69 ---------------------------
	the_RFM69 = new RFM69();

	the_RFM69->initialize( Frequency_Band, The_Gateway_Node_ID, The_Network_ID );
	initRFM69();


	// Mosquitto subscription ---------
	char subsciptionMask[128];

    sprintf(             subsciptionMask, "%s/INPUT/Network%03d/#", MQTT_Topic_Root, The_Network_ID );
	LOG(                 "Subscribing to MQTT topic: %s\n", subsciptionMask );
	mosquitto_subscribe( The_Mosquitto_Client, NULL, subsciptionMask, 0 );


	LOG( "*** setup complete\n" );
	
	return runLoop();
}  // end of setup


/* Loop until it is explicitly halted or the network is lost, then clean up. */
static int runLoop( void )
{
 	unsigned long last_message_time          = millis();

	int           mosquitto_loop_result;

	while( Keep_Process_Alive )                                          // Run until killed
	{
        char mqtt_topic  [MQTT_Max_Topic_Length   + 1];
        char mqtt_message[MQTT_Max_Message_Length + 1];

		mosquitto_loop_result = mosquitto_loop( The_Mosquitto_Client, Mosquitto_Timeout_Millis, Mosquitto_Max_Packets );

		// No messages have been received withing MESSAGE_WATCHDOG interval
		if( millis() > (last_message_time + Watchdog_Timeout_Delay) )
		{
			LOG("=== Message WatchDog ===\n");
			the_Stats.messageWatchdog++;
			// re-initialise the radio
			initRFM69();
			// reset watchdog
			last_message_time = millis();
		}
		
		if( the_RFM69->receiveDone() ) 
        {
            bool published_mqtt_message = false;

			// record last message received time - to compute radio watchdog
			last_message_time = millis();
			the_Stats.messageReceived++;
			
			// store the received data localy, so they can be overwited
			// This will allow to send ACK immediately after
			uint8_t data[RF69_MAX_DATA_LEN]; // recv/xmit buf, including header & crc bytes

			uint8_t data_length        = the_RFM69->DATALEN;
			uint8_t the_node_id        = the_RFM69->SENDERID;
			uint8_t target_id          = the_RFM69->TARGETID;     // should match _address
			uint8_t ack_requested      = the_RFM69->ACK_REQUESTED;
			int16_t rx_signal_strength = the_RFM69->RSSI;         // most accurate rx_signal_strength during reception (closest to the reception)

			memcpy(data, (void *)the_RFM69->DATA, data_length);

			if( ack_requested  && (target_id == The_Gateway_Node_ID) )
            {
				// When a node requests an ACK, respond to the ACK
				// but only if the Node ID is correct
				the_Stats.ackRequested++;
				the_RFM69->sendACK();
				
				if( (the_Stats.ackCount++ % 3) == 0 )
                {
					// and also send a packet requesting an ACK (every 3rd one only)
					// This way both TX/RX NODE functions are tested on 1 end at the GATEWAY

					usleep(3000);  //need this when sending right after reception .. ?
					the_Stats.messageSent++;
					if( the_RFM69->sendWithRetry(the_node_id, "ACK TEST", 8) )
                    { // 3 retry, over 200ms delay each
						the_Stats.ackReceived++;
						LOG( "Pinging node %d - ACK - ok!\n", the_node_id );
					}
					else 
                    {
						the_Stats.ackMissed++;
						LOG( "Pinging node %d - ACK - nothing!\n", the_node_id );
					}
				}
			}//end if radio.ACK_REQESTED
	
			LOG("[%03d] to [%03d] ", the_node_id, target_id);

			if( data_length != sizeof(Packet_Data) )
			{
				LOG( "Invalid payload received, not matching Payload struct! %d - %d\r\n", data_length, sizeof(Packet_Data) );
				hexDump( "", data, data_length, Desired_Data_Vals_Per_Line );		
			}
			else
			{
				the_RFM69_Packet = *(Packet_Data*)data; //assume radio.DATA actually contains our struct and not something else

				switch( the_RFM69_Packet.protocol_Version )
				{
					case First_Protocol_Version:
						{
							switch( the_RFM69_Packet.data_Type )
							{
								case Temperature_Data_Type:
									{
										LOG( "Received Temperature, Node ID = %03d Device ID = %03d Seq = %03d  RSSI = %+04d Temp = %+07.2fC\n",
											the_RFM69_Packet.source_Node_ID,
											the_RFM69_Packet.the_Data.temperature.device_ID,
											the_RFM69_Packet.the_Data.temperature.packet_Sequence_Number,
											rx_signal_strength,
											the_RFM69_Packet.the_Data.temperature.temperature
										);
										
                                        
										mqttFormOutputTopic    ( the_RFM69_Packet.source_Node_ID, the_RFM69_Packet.the_Data.temperature.device_ID, "RSSI", mqtt_topic );
                                        sprintf                ( mqtt_message, "%+04d", rx_signal_strength );
                                        mosquittoPublishMessage( mqtt_topic, mqtt_message );

										mqttFormOutputTopic    ( the_RFM69_Packet.source_Node_ID, the_RFM69_Packet.the_Data.temperature.device_ID, "Sequence", mqtt_topic );
                                        sprintf                ( mqtt_message, "%03d", the_RFM69_Packet.the_Data.temperature.packet_Sequence_Number );
                                        mosquittoPublishMessage( mqtt_topic, mqtt_message );

                                        mqttFormOutputTopic    ( the_RFM69_Packet.source_Node_ID, the_RFM69_Packet.the_Data.temperature.device_ID, "TemperatureC", mqtt_topic );
                                        sprintf                ( mqtt_message, "%+07.2f", the_RFM69_Packet.the_Data.temperature.temperature );
                                        mosquittoPublishMessage( mqtt_topic, mqtt_message );

										published_mqtt_message = true;
										break;
									}
									
								default:
									{
										LOG( "Unhandled Data Type = %03d\n", the_RFM69_Packet.data_Type );
										break;
									}
							}
									
							break;
						}
						
					default:
						{
							LOG( "Unhandled Protocol Version = %03d\n", the_RFM69_Packet.protocol_Version );
							break;
						}
				}

				if( published_mqtt_message == false )
					hexDump( NULL, data, data_length, Desired_Data_Vals_Per_Line );
			} // end if data_length 
		} //end if radio.receive


        {
            static uint8_t sequence_number                  = 0;
            static unsigned long time_temperature_last_read = millis();

            if( millis() > (time_temperature_last_read + Temperature_Read_Interval) )
            {
                float rfm69_temperature = (float)the_RFM69->readTemperature( This_RFM69_Temperature_Calibration_Factor );
                time_temperature_last_read = millis();

                LOG( "Read gateway temperature #%03d: %+07.2fC\n", sequence_number, rfm69_temperature );
 
				mqttFormOutputTopic    ( The_Gateway_Node_ID, This_Node_Radio_Device_ID, "Sequence", mqtt_topic );
                sprintf                ( mqtt_message, "%03d", sequence_number++ );
                mosquittoPublishMessage( mqtt_topic, mqtt_message );

                mqttFormOutputTopic    ( The_Gateway_Node_ID, This_Node_Radio_Device_ID, "TemperatureC", mqtt_topic );
                sprintf                ( mqtt_message, "%+07.2f", rfm69_temperature );
                mosquittoPublishMessage( mqtt_topic, mqtt_message );
            }
        }

	} // end for( ;; )

		
	mosquitto_destroy( The_Mosquitto_Client );
	(void)mosquitto_lib_cleanup();

	
	if( mosquitto_loop_result == MOSQ_ERR_SUCCESS )
    {
		return 0;
	} 
    else 
    {
		return 1;
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


/* Fail with an error message. */
static void die( const char *msg )
{
	fprintf( stderr, "%s", msg );
	exit( 1 );
}


/* static long millis(void) {
	struct timeval tv;

    gettimeofday(&tv, NULL);

    return ((tv.tv_sec) * 1000 + tv.tv_usec/1000.0) + 0.5;
	}
 */


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


static void mqttFormOutputTopic( uint8_t node, uint8_t device, const char* subject, char* topic_buffer )
{
	sprintf( topic_buffer, "%s/OUTPUT/Network%03d/Node%03d/Device%03d/%s", MQTT_Topic_Root, The_Network_ID, node, device, subject );

    if( strlen(topic_buffer) > MQTT_Max_Topic_Length )
        die( "MQTT topic too large, probable buffer overflow" );
}


static void mosquittoPublishMessage( const char* mqtt_topic, const char *mqtt_message )
{
//    LOG( "MQTT: publish topic=%s, message=%s\n", mqtt_topic, mqtt_message );

    if( strlen(mqtt_topic) > MQTT_Max_Topic_Length )
	{
        die( "MQTT topic too large at publish, probable buffer overflow" );
	}

    if( strlen(mqtt_message) > MQTT_Max_Message_Length )
	{
        die( "MQTT message too large at publish, probable buffer overflow" );
	}

	mosquitto_publish( The_Mosquitto_Client, 0, &mqtt_topic[0], strlen(mqtt_message), mqtt_message, 0, false);
}


// Handing of Mosquitto messages
void mosquittoCallback( char* topic, uint8_t* payload, unsigned int length )
{
	// handle message arrived
	LOG( "Mosquitto Callback\n" );
}


/* Connect to the network. */
static bool connectToMosquitto( void )
{
	int res = mosquitto_connect( The_Mosquitto_Client, MQTT_Broker_Hostname, MQTT_Broker_Port, MQTT_Keepalive_Seconds );
	LOG( "Connect return %d\n", res );
	return (res == MOSQ_ERR_SUCCESS);
}


/* Callback for successful connection: add subscriptions. */
static void onMosquittoConnect( struct mosquitto* the_mosquitto_client, void* udata, int result )
{
	if( result == 0 ) 
    {   /* success */
		LOG( "Mosquitto connect succeed\n" );
	}
    else
    {
		die( "Mosquitto connection refused\n" );
	}
}


/* Handle a message that just arrived via one of the subscriptions. */
/*
static void on_message(struct mosquitto *m, void *udata,
const struct mosquitto_message *msg) {
	if (msg == NULL) { return; }
	LOG("-- got message @ %s: (%d, QoS %d, %s) '%s'\n",
		msg->topic, msg->payloadlen, msg->qos, msg->retain ? "R" : "!r",
		msg->payload);
		
	if (strlen((const char *)msg->topic) < strlen(MQTT_Topic_Root) + 2 + 3 + 1) {return; }	// message is smaller than "RFM/xxx/x" so likey invalid

//	Payload data;
	Packet_Data data;
	uint8_t network;

	sscanf(msg->topic, "RFM/%d/%d/%d", &network, &data.nodeID, &data.deviceID);
	if (strncmp(msg->topic, MQTT_Topic_Root, strlen(MQTT_Topic_Root)) == 0 && network == The_Network_ID ) {
		
		// extract the target network and the target node from the topic
		sscanf(msg->topic, "RFM/%d/%d/%d", &network, &data.nodeID, &data.deviceID);
		
		if (network == The_Network_ID ) {
			// only process the messages to our network
		
			sscanf((const char *)msg->payload, "%ld,%f,%f", &data.var1_usl, &data.sensor_data.var2_float, &data.sensor_data.var3_float);
			
			LOG("Received message for Node ID = %d Device ID = %d Time = %d  var2 = %f var3 = %f\n",
				data.nodeID,
				data.deviceID,
				data.var1_usl,
				data.sensor_data.var2_float,
				data.sensor_data.var3_float
			);

			theStats.messageSent++;
			if (the_RFM69->sendWithRetry(data.nodeID,(const void*)(&data),sizeof(data))) {
				LOG("Message sent to node %d ACK", data.nodeID);
				theStats.ackReceived++;
				}
			else {
				LOG("Message sent to node %d NAK", data.nodeID);
				theStats.ackMissed++;
			}
		}
	}
}
*/
static void onMosquittoMessage( struct mosquitto *the_mosquitto_client, void *udata,
                                const struct mosquitto_message *msg)
{}


/* A message was successfully published. */
static void onMosquittoPublish( struct mosquitto *the_mosquitto_client, void *udata, int m_id )
{
//	LOG(" -- published successfully\n");
}


/* Successful subscription hook. */
static void onMosquittoSubscribe( struct mosquitto *the_mosquitto_client, void *udata, int mid,
		                          int qos_count, const int *granted_qos)
{
//	LOG(" -- subscribed successfully\n");
}


/* Register the callbacks that the mosquitto connection will use. */
static bool setMosquittoCallbacks( void )
{
	mosquitto_connect_callback_set  ( The_Mosquitto_Client, onMosquittoConnect );
	mosquitto_publish_callback_set  ( The_Mosquitto_Client, onMosquittoPublish );
	mosquitto_subscribe_callback_set( The_Mosquitto_Client, onMosquittoSubscribe );
	mosquitto_message_callback_set  ( The_Mosquitto_Client, onMosquittoMessage );

	return true;
}
