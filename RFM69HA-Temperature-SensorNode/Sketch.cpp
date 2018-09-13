/*
 Author: Erik Falk
 Based on work from: Alexandre Bouillot, Eric Tsai
 License: CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
 Date:
 File:

 This code is for an Arduino or a "stand-alone" AVR micro-controller
 connected via SPI to an RFM69 wireless transceiver module (the "sensor
 node").  It sends sensor data (temperature read from the RFM69 module)
 to a remote device which is functioning as an MQTT broker (the "gateway
 node").
 */


// *** IMPORTANT ***
//
// BE CERTAIN TO PAY ATTENTION TO THE FINAL "PROGRAM MEMORY" AND
// "DATA MEMORY" VALUES FOR THE COMPILED CODE TO BE SURE THEY DO
// NOT EXCEED THE CAPABILITIES OF THE TARGET AVR CHIP
//


/*Begining of Auto generated code by Atmel studio */
//#include <Arduino.h>

/*End of auto generated code by Atmel studio */


// *** NOTE ***
// Many "tinyAVR" chips (e.g. ATtiny85, ATtiny84, etc.) lack sufficient serial
// outputs and/or program/data memory to allow the use of serial output
//
#if not defined (NO_SERIAL_OUTPUT)
    #define SERIALPRINT1( expression )       Serial.print( expression )
    #define SERIALPRINT2( expression, arg )  Serial.print( expression, arg )
    #define SERIALPRINTLN1( expression )     Serial.println( expression )

    const unsigned long Serial_Baud = 115200;
#else
    #define SERIALPRINT1( expression )
    #define SERIALPRINT2( expression, arg )
    #define SERIALPRINTLN1( expression )
#endif

// Debugging output --------------------------------
#if defined (DEBUG)
    #if not defined (NO_SERIAL_OUTPUT)
        #define SERIAL_DEBUGING_OUTPUT
    #endif
#endif
// end Debugging output --------------------------------


//RFM69  --------------------------------------------------------------------------------------------------
#include <SPI/SPI.h>
#include <RFM69/RFM69.h>
#include <RFM69NetworkCfg.h>
#include <RFM69HomeAutomationCfg.h>
#include "RFM69NetworkNodeCfg.h"        // Change settings in this file for each node
#include "RFM69HomeAutomationNodeCfg.h" // Change settings in this file for each node

/*
#define THIS_RFM69_IS_HIGH_POWER    // Uncomment for RFM69HW/RFM69HCW/etc., comment out if you have RFM69W/RFM69CW/etc.

// **** CHANGE THIS FOR EACH ADDITIONAL SENSOR NODE OR CONTROL NODE ****
const uint8_t This_Node_ID    = 2;                  // ***THIS MUST BE UNIQUE FOR EACH SENSOR OR CONTROL NODE ON A NETWORK***

const uint8_t Temperature_Calibration_Factor = 2;
*/

RFM69       radio;
Packet_Data theData;
//end RFM69 ------------------------------------------


// timings (milliseconds)
const unsigned long Temperature_Read_Interval = 3000;
unsigned long       timeTemperatureLastRead;


typedef enum {
    This_Node_Radio_Device_ID = 0
} Node_Device_ID;



void setup()
{
#if defined SERIAL_DEBUGING_OUTPUT
  Serial.begin( Serial_Baud );          //  setup serial
  SERIALPRINT1( "starting" );
#endif


  //RFM69-------------------------------------------
  radio.initialize( Frequency_Band, This_Node_ID, The_Network_ID );
#if defined THIS_RFM69_IS_HIGH_POWER
  radio.setHighPower();
#endif

#if defined SERIAL_DEBUGING_OUTPUT
  char buff[50];
  sprintf( buff, "\nTransmitting at %d Mhz...", (Frequency_Band==RF69_433MHZ) ? 433 : ((Frequency_Band==RF69_868MHZ) ? 868 : 915) );
  SERIALPRINTLN1( buff );
#endif

  radio.encrypt( (char*)The_Encryption_Key );

  theData.protocol_Version  = The_Current_Protocol_Version;
  theData.source_Node_ID    = This_Node_ID;
  theData.target_Node_ID    = The_Gateway_Node_ID;
  //end RFM--------------------------------------------

  //initialize times
  timeTemperatureLastRead = millis();
}


void loop()
{
  static uint8_t sequence_number = 0;
  unsigned long  time_elapsed    = 0;

  //===================================================================
  time_elapsed = millis() - timeTemperatureLastRead;

  if( time_elapsed > Temperature_Read_Interval )
  {
    float rfm69_temperature = (float)radio.readTemperature( Temperature_Calibration_Factor );
    timeTemperatureLastRead = millis();

	SERIALPRINT1( "Temperature " );
	SERIALPRINTLN1( rfm69_temperature );
	SERIALPRINT1( "Packet sequence #" );
	SERIALPRINTLN1( sequence_number );

    //send data
	theData.data_Type									= Temperature_Data_Type;
    theData.the_Data.temperature.device_ID				= This_Node_Radio_Device_ID;
    theData.the_Data.temperature.packet_Sequence_Number = sequence_number++;
    theData.the_Data.temperature.temperature			= rfm69_temperature;
    radio.send( The_Gateway_Node_ID, (const void*)(&theData), sizeof(theData) );
  }
}//end loop
