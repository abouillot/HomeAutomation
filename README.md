HomeAutomation
==============

Home Automation repository

Work initialy started with the Uber Home Automation Project found on:
http://www.instructables.com/id/Uber-Home-Automation
and various other places on the internet

Two way communication can be performed thru MQTT messages.

Basic setup for the sample sensorNode on network 101, node 14
Connect RGB pin on pins 8, 6 and 4.

in the openHab config items file, add: 
```
Switch LivingLedRedSwitch {mqtt=">[rfmPiMosquitto:RFM/101/14/7:command:OFF:0,0,0],>[rfmPiMosquitto:RFM/101/14/7:command:ON:0,1,0"}
Switch LivingLedBlueSwitch {mqtt=">[rfmPiMosquitto:RFM/101/14/7:command:OFF:0,0,0],>[rfmPiMosquitto:RFM/101/14/7:command:ON:1,0,0"}
Switch LivingLedGreenSwitch {mqtt=">[rfmPiMosquitto:RFM/101/14/7:command:OFF:0,0,0],>[rfmPiMosquitto:RFM/101/14/7:command:ON:0,0,1"}
```

in the appropriate frame section of the config sitemap file, add:
```
Frame label="Living temp" {
                Frame label="RGB Led" {
                        Switch item=LivingLedRedSwitch label="Toggle Red Switch"
                        Switch item=LivingLedBlueSwitch label="Toggle Blue Switch"
                        Switch item=LivingLedGreenSwitch label="Toggle Green Switch"
                }
```             

Actionning the switch from openHab web page will trigger the appropriate message to the node. 

For debug, look at the node serial output, where you should see the incoming messages and theirs decoding:
```
Received Device ID = 7
    Time = 1
    var2_float 1.00
    var3_float 1.00
```

In the `/var/log/messages` the outgoint messages are logged:
```
Jan 14 15:52:20 rfmPi Gatewayd[14115]: -- got message @ RFM/101/14/7: (5, QoS 0, !r) '0,1,0'
Jan 14 15:52:20 rfmPi Gatewayd[14115]: Received message for Node ID = 14 Device ID = 7 Time = 0  var2 = 1.000000 var3 = 0.000000
Jan 14 15:52:20 rfmPi Gatewayd[14115]: Message sent to node 14 ACK
```

You can send manualy the message with the command:
```
mosquitto_pub -m 1,0,0 -t RFM/101/14/7
```
