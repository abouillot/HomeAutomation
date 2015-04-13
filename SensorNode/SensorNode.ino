/*
 Author: Alexandre Bouillot
 Based on work from: Eric Tsai
 License: CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
 Date: 2014/04/13
 File: SensorNode.ino
 This sketch is for a wired Arduino w/ RFM69 wireless transceiver
 Sends sensor data (temp/humidity) back  to gateway.  
 Receive sensor messages from the gateway
 */


/* sensor
 node = 13
 device ID
 2 = 1222 = smoke or not
 3 = 1232 = flame detected or not
 4 = 1242 = human motion present or not
 5 = 1252 = barking or not
 6 = 1262, 1263 = temperature, humidity
 
 */



//general --------------------------------
#define SERIAL_BAUD   115200
#if 1
#define DEBUG1(expression)  Serial.print(expression)
#define DEBUG2(expression, arg)  Serial.print(expression, arg)
#define DEBUGLN1(expression)  Serial.println(expression)
#else
#define DEBUG1(expression)
#define DEBUG2(expression, arg)
#define DEBUGLN1(expression)
#endif
//RFM69  --------------------------------------------------------------------------------------------------
#include <RFM69.h>
#include <SPI.h>
#define NODEID        13    //unique for each node on same network
#define NETWORKID     101  //the same on all nodes that talk to each other
#define GATEWAYID     1
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY   RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "xxxxxxxxxxxxxxxx" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME      30 // max # of ms to wait for an ack
#define LED           9  // Moteinos have LEDs on D9
#define SERIAL_BAUD   115200  //must be 9600 for GPS, use whatever if no GPS

boolean debug = 0;

//struct for wireless data transmission
typedef struct {		
  int       nodeID; 		//node ID (1xx, 2xx, 3xx);  1xx = basement, 2xx = main floor, 3xx = outside
  int       deviceID;		//sensor ID (2, 3, 4, 5)
  unsigned long   var1_usl; 		//uptime in ms
  float     var2_float;   	//sensor data?
  float     var3_float;		//battery condition?
} 
Payload;
Payload theData;

char buff[20];
byte sendSize=0;
boolean requestACK = false;
RFM69 radio;

//end RFM69 ------------------------------------------


//temperature / humidity  =====================================
#include <dht.h>
#define DHTPIN 7     			// digital pin we're connected to
//#define DHTTYPE DHT11     // DHT 11  (AM2302) blue one
//#define TEMP_INTERVAL  6000
#define TEMP_INTERVAL  3000
#define DHTTYPE DHT22   // DHT 21 (AM2302) white one
//structure holding DHT data
dht DHT;


// RGB LED
#define REDPIN 6
#define BLUEPIN 4
#define GREENPIN 8

// 1 = 1211, 1212 = transmission ack count
// 6 = 1262, 1263 = temperature, humidity
// 7 = 1371, 1372, 1373 PWM for led


// timings
unsigned long temperature_time;

unsigned long frameSent = 0;
unsigned long ackMissed = 0;
unsigned long ackReceived = 0;
boolean statOut;

// Anarduino led is on pin D9
int led = 9;

void setup()
{
  Serial.begin(SERIAL_BAUD);          //  setup serial
  DEBUG1("starting");

  //RFM69-------------------------------------------
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  DEBUGLN1(buff);
  theData.nodeID = NODEID;  //this node id should be the same for all devices in this node
  //end RFM--------------------------------------------

  //initialize times
  temperature_time = millis();
  
  pinMode(led, OUTPUT);
  pinMode(REDPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
}

long blinkInterval = 1000;
long blinkNext = 0;

void loop()
{
  if (millis() > blinkNext)
  {
    digitalWrite(led, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(100);               // wait for a second
    digitalWrite(led, LOW);    // turn the LED off by making the voltage LOW
    blinkNext = millis() + blinkInterval;
  }
  
  //check for any received packets
  if (radio.receiveDone())
  {
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    if (radio.DATALEN == 8) { // ACK TEST
      for (byte i = 0; i < radio.DATALEN; i++)
        DEBUG1((char)radio.DATA[i]);
    }
    else if(radio.DATALEN == sizeof(Payload)) {
      for (byte i = 0; i < radio.DATALEN; i++) {
        DEBUG2((char)radio.DATA[i], HEX);
        DEBUG1(".");
      }
      DEBUGLN1();
      theData = *(Payload*)radio.DATA; //assume radio.DATA actually contains our struct and not something else

      DEBUG1("Received Device ID = ");
      DEBUGLN1(theData.deviceID);  
      DEBUG1 ("    Time = ");
      DEBUGLN1 (theData.var1_usl);
      DEBUG1 ("    var2_float ");
      DEBUGLN1 (theData.var2_float);
      DEBUG1 ("    var3_float ");
      DEBUGLN1 (theData.var3_float);
      
      if (theData.deviceID == 7) {
        digitalWrite(BLUEPIN, theData.var1_usl == 0 ? LOW : HIGH);
        digitalWrite(REDPIN, theData.var2_float == 0 ? LOW : HIGH);
        digitalWrite(GREENPIN, theData.var3_float == 0 ? LOW : HIGH);
      }
    }
    else {
      Serial.print("Invalid data ");
      for (byte i = 0; i < radio.DATALEN; i++) {
        DEBUG2((char)radio.DATA[i], HEX);
        DEBUG1(".");
      }
    }

    DEBUG1("   [RX_RSSI:");DEBUG1(radio.RSSI);DEBUG1("]");

    if (radio.ACKRequested())
    {
      radio.sendACK();
      DEBUG1(" - ACK sent");
    }
    DEBUGLN1();
  }
  
  if (frameSent%20 == 0) {
    //send data
    theData.deviceID = 1;
    theData.var1_usl = millis();
    theData.var2_float = frameSent;
    theData.var3_float = ackMissed;
    frameSent++;
    if (radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData))) {
      ackReceived++;
      DEBUGLN1("ACK received");
    } else {
      ackMissed++;
    }
  }

  if (frameSent%10 == 0) {
    if (statOut == 0) {
      statOut = 1;
      DEBUG1("Frames ");
      DEBUG1(frameSent);
      DEBUG1(" missed: ");
      DEBUG1(ackMissed);
      DEBUG1(" ACKnowledge: ");
      DEBUG1(ackReceived);
    }
  } else { statOut = 0; }
  
  unsigned long time_passed = 0;

  //===================================================================
  //device #6
  //temperature / humidity
  time_passed = millis() - temperature_time;
  if (time_passed < 0) {
    temperature_time = millis();
  }

  if (time_passed > TEMP_INTERVAL) {
    int chk = DHT.read21(DHTPIN);
    
  switch (chk)
    {
    case DHTLIB_OK:
        DEBUG1("OK,\t");
        break;
    case DHTLIB_ERROR_CHECKSUM:
        DEBUG1("Checksum error,\t");
        break;
    case DHTLIB_ERROR_TIMEOUT:
        DEBUG1("Time out error,\t");
        break;
    case DHTLIB_ERROR_CONNECT:
        Serial.print("Connect error,\t");
        break;
    case DHTLIB_ERROR_ACK_L:
        DEBUG1("Ack Low error,\t");
        break;
    case DHTLIB_ERROR_ACK_H:
        DEBUG1("Ack High error,\t");
        break;
    default:
        DEBUG1("Unknown error,\t");
        break;
    }
    
    double h = DHT.humidity;
    // Read temperature as Celsius
    double t = DHT.temperature;

    DEBUG1("Humidity=");
    DEBUG1(h);
    DEBUG1("   Temp=");
    DEBUG1(t);
    DEBUG1("°C");
    temperature_time = millis();

    //send data
    theData.deviceID = 6;
    theData.var1_usl = millis();
    theData.var2_float = t;
    theData.var3_float = h;
    frameSent++;
    if (radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData))) {
      ackReceived++;
      DEBUGLN1(" ACK received");
    } else {
      ackMissed++;
    }
    delay(100);
  }
}//end loop








