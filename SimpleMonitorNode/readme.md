This repo contain a sample node sketch. This code is trying to minimize the power consumption, allowing to use energy harvesting/battery supply.

The sketch is based on Anarduino Miniwireless board, with a DHT22 connected and a resistor bridge to measure the battery voltage. It shut down the peripherals when not used (DHT, Radio, Flash...) and goes to sleep when nothng to do.

The DHT22 is connected to the pin 7 for the data and pin 8 for the power supply. The GN is connected on GND pin.

The battery consist of 2 resistors connected in serial, from GND to VIN with the middle point being connected to A0. The values of the resistor must be changed in the sketch to have acurate conversion.

The Node can work in 3 modes, depending the usage/
* Class A, the rado is shut off just after the data transmission
* Class B, the radio is kept on few seconds in order to receive messages from gateway, after transmitting
* Class C, the radio is always on. More useful for actuator but will consume more energy
