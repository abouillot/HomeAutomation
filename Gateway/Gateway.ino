/*
 Based on work from author:  Eric Tsai
 Gateway ncorporating both the RFM69 and the ethernet part
 Revised by Alexandre Bouillot
 
 License:  CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
 Date:  10-23-2014
 File: Gateway.ino
 This sketch receives RFM wireless data and forwards it to Mosquitto relay
 
 Modifications Needed:
 1)  Update encryption string "ENCRYPTKEY"
 2)  Adjust SS - Chip Select - for RFM69
 3)  Adjust MQTT server address
 */

/*
RFM69 Pinout:
 MOSI = 11
 MISO = 12
 SCK = 13
 SS = 8
 */

/*
Ethernet Pinout:
 MOSI = 11
 MISO = 12
 SCK = 13
 SS = 10
 */

//general --------------------------------
#define SERIAL_BAUD   115200

//RFM69  ----------------------------------
#include <RFM69.h>
#include <SPI.h>
#define NODEID        1    //unique for each node on same network
#define NETWORKID     101  //the same on all nodes that talk to each other
#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY   RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "xxxxxxxxxxxxxxxx" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME      30 // max # of ms to wait for an ack
#define RFM69_SS  8
RFM69 radio(RFM69_SS);
bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network

#include <Ethernet.h>

//Ethernet
byte mac[]    = {  
  0x90, 0xA2, 0xDA, 0x0D, 0x11, 0x11 };
byte server[] = { 
  192, 168, 0, 50 };

IPAddress ip(192,168,0,61);
EthernetClient ethClient;
#define DHCP_RETRY 500

// Mosquitto---------------
#include <PubSubClient.h>
PubSubClient client(server, 1883, callback, ethClient);
#define MQTT_CLIENT_ID "arduinoClient"
#define MQTT_RETRY 500
int sendMQTT = 0;

void MQTTSendInt(PubSubClient* _client, int node, int sensor, int var, int val);
void MQTTSendLong(PubSubClient* _client, int node, int sensor, int var, long val);

//use LED for indicating MQTT connection status.
int led = 13;

typedef struct {		
  int                   nodeID; 
  int			sensorID;
  unsigned long         var1_usl; 
  float                 var2_float; 
  float			var3_float;	
} 
Payload;
Payload theData;

volatile struct 
{
  int                   nodeID;
  int			sensorID;		
  unsigned long         var1_usl;
  float                 var2_float;
  float			var3_float;		//
  int                   var4_int;
} 
SensorNode;

void setup() 
{
  Serial.begin(SERIAL_BAUD); 

  //Ethernet -------------------------
  //Ethernet.begin(mac, ip);

  //wait for IP address
  while (Ethernet.begin(mac) != 1) {
    Serial.println(F("Error getting IP address via DHCP, trying again..."));
    delay(DHCP_RETRY);
  }

  Serial.println(F("ethernet OK"));
  // print your local IP address:
  Serial.print(F("My IP address: "));
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(F(".")); 
  }
  Serial.println();

  // Mosquitto ------------------------------
  while (client.connect(MQTT_CLIENT_ID) != 1) {
    Serial.println(F("Error connecting to MQTT"));
    delay(MQTT_RETRY);
  }

  //RFM69 ---------------------------
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  radio.promiscuous(promiscuousMode);
  char buff[50];
  sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);

  Serial.println(F("setup complete"));
}  // end of setup

byte ackCount=0;
long watchdogInterval = 2000;
long watchdog = 0;

void loop() {
  
  // calling client.loop too often block the system at some point quite early (~up to 5 loop)
  // Here is a temporized call to it on a regular interval
  // This need to be as fast as the fastest sensor received
  if (millis() > watchdog) {
//    Serial.print("loop "); 
//    Serial.println(millis());
    watchdog += watchdogInterval;
    //client.loop needs to run every iteration.  Previous version did not.  Big opps.
    client.loop();
  }

  if (radio.receiveDone()) {
    Serial.print('[');
    Serial.print(radio.SENDERID, DEC);
    Serial.print("] ");
    if (promiscuousMode) {
      Serial.print("to [");
      Serial.print(radio.TARGETID, DEC);
      Serial.print("] ");
    }

    if (radio.DATALEN != sizeof(Payload))
      Serial.println(F("Invalid payload received, not matching Payload struct!"));
    else {
      theData = *(Payload*)radio.DATA; //assume radio.DATA actually contains our struct and not something else

      //      Serial.print(theData.sensorID);
      //      Serial.print(", ");
      Serial.print(theData.var1_usl);
      //      Serial.print(", ");
      //      Serial.print(theData.var2_float);
      //      Serial.print(", ");
      //Serial.print(" var2(temperature)=");
      //Serial.print(", ");
      //Serial.print(theData.var3_float);

      //printFloat(theData.var2_float, 5); Serial.print(", "); printFloat(theData.var3_float, 5);

      Serial.print(F(", RSSI= "));
      Serial.println(radio.RSSI);

      //save it for i2c:
      SensorNode.nodeID = theData.nodeID;
      SensorNode.sensorID = theData.sensorID;
      SensorNode.var1_usl = theData.var1_usl;
      SensorNode.var2_float = theData.var2_float;
      SensorNode.var3_float = theData.var3_float;
      SensorNode.var4_int = radio.RSSI;

      /*
      Serial.print ("Received Device ID = ");
       Serial.println (SensorNode.sensorID);  
       Serial.print ("    Time = ");
       Serial.println (SensorNode.var1_usl);
       Serial.print ("    var2_float ");
       Serial.println (SensorNode.var2_float);
       */
      sendMQTT = 1;
    }


    if (radio.ACK_REQUESTED)
    {
      byte theNodeID = radio.SENDERID;
      radio.sendACK();

      // When a node requests an ACK, respond to the ACK
      // and also send a packet requesting an ACK (every 3rd one only)
      // This way both TX/RX NODE functions are tested on 1 end at the GATEWAY
      if (ackCount++%3==0)
      {
        //Serial.print(" Pinging node ");
        //Serial.print(theNodeID);
        //Serial.print(" - ACK...");
        //delay(3); //need this when sending right after reception .. ?
        //if (radio.sendWithRetry(theNodeID, "ACK TEST", 8, 0))  // 0 = only 1 attempt, no retries
        //  Serial.print("ok!");
        //else Serial.print("nothing");
      }
    }//end if radio.ACK_REQESTED
  } //end if radio.receive

  if (sendMQTT == 1) {
    Serial.println(F("starting MQTT send"));

    if (!client.connected()) {
      while (client.connect(MQTT_CLIENT_ID) != 1)
      {
        digitalWrite(led, LOW);
        Serial.println("Error connecting to MQTT");
        delay(500);
        digitalWrite(led, HIGH);
      }
      client.publish("outTopic","hello world");
    } 

    digitalWrite(led, HIGH);
    
    int varnum;
    char buff_topic[6];
    char buff_message[12];      

    /*
      //send var1_usl
     varnum = 2;
     buff_topic[6];
     buff_message[12];
     sprintf(buff_topic, "%02d%01d%01d", SensorNode.nodeID, SensorNode.sensorID, varnum);
     Serial.println(buff_topic);
     dtostrf (SensorNode.var1_usl, 10, 1, buff_message);
     client.publish(buff_topic, buff_message);
     */

    //send var2_float
    MQTTSendLong(&client, SensorNode.nodeID, SensorNode.sensorID, 2, SensorNode.var2_float);

    //send var3_float
    MQTTSendInt(&client, SensorNode.nodeID, SensorNode.sensorID, 3, SensorNode.var3_float);

    //send var4_int, RSSI
    MQTTSendInt(&client, SensorNode.nodeID, SensorNode.sensorID, 4, SensorNode.var4_int);

    sendMQTT = 0;
    Serial.println(F("finished MQTT send"));
    digitalWrite(led, LOW);
  }//end if sendMQTT
}//end loop

void MQTTSendInt(PubSubClient* _client, int node, int sensor, int var, int val) {
    char buff_topic[6];
    char buff_message[7];

    sprintf(buff_topic, "%02d%01d%01d", node, sensor, var);
    sprintf(buff_message, "%04d%", val);
    _client->publish(buff_topic, buff_message);
}
void MQTTSendLong(PubSubClient* _client, int node, int sensor, int var, long val) {
    char buff_topic[6];
    char buff_message[7];

    sprintf(buff_topic, "%02d%01d%01d", node, sensor, var);
    dtostrf (val, 2, 1, buff_message);
    _client->publish(buff_topic, buff_message);
}

// Handing of Mosquitto messages
void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  Serial.println(F("Mosquitto Callback"));
}




