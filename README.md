RFM69HomeAutomation
====================
![](./Media/RFM69HomeAutomation.jpg)

## RFM69 Home Automation repository

The initial project is described by Eric Tsai at [Uber Home Automation](https://www.instructables.com/id/Uber-Home-Automation-w-Arduino-Pi/) and various other places on the internet. The code in this repository was adapted from work by [Eric Tsai](https://github.com/tsaitsai/OpenHab-RFM69) and [Alexandre Bouillot](https://github.com/abouillot/HomeAutomation), and is licensed under [Creative Commons CC-BY-SA](https://creativecommons.org/licenses/by-sa/2.0/).

This system described here is intended to provide an inexpensive, reasonably secure, and easily extensible "DIY" home automation network based on the relatively affordable (in 2017, about $5 U.S. for single-unit quantities) RFM69 radio transceiver modules from Hope Microelectronics co., Ltd (info at http://www.hoperf.cn [Chinese only] or http://www.hoperf.com [worldwide, English only]), and which operate in the unlicensed [ISM radio frequency bands](https://en.wikipedia.org/wiki/ISM_band).

It should be noted that, while these transceiver modules provide the ability to encrypt their signals and are capable of sending and recieving over significant distances, the small size of their data packets (66 bytes maximum) limit their overall bandwidth, making them unsuitable for high-banwidth tasks such as video surveillance; for high-bandwidth needs, other communications means such as WiFi or hard-wiring is preferable. RFM69 modules are, however, perfectly suited for most other home automation tasks such as controlling lights, reporting weather data, monitoring water levels, remotely operating door locks, etc.

## RFM69 Nodes

The combination of an RFM69 module and a microcontroller or computer is referred to as a "node." The RFM69 module is connected to a microcontroller or computer via the module's [SPI](https://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus) interface, with the RFM69 module acting as a SPI "slave" device.

![](./Media/node.jpg)

An "end node" consists of an RFM69 module, a microcontroller or computer, and typically one or more sensor and/or "effector" devices, such as a relative-humidity sensor, a relay acting as a light switch, a door-lock mechanism, etc.

A "gateway node" simply consists of an RFM69 module and a computer or sufficiently powerful microcontroller which translates commands-to and data-from an RFM69 end node into another communications protocol, such as [MQTT](https://en.wikipedia.org/wiki/MQTT), [AllJoyn](https://en.wikipedia.org/wiki/AllJoyn), [CoAP](https://en.wikipedia.org/wiki/Constrained_Application_Protocol), [DDS](https://en.wikipedia.org/wiki/Data_Distribution_Service), etc. The gateway node may also include sensor/effector devices, but this is strictly optional.

## RFM69 Home Automation Network



![](./Media/RFM69HomeAutomationNetwork.jpg)


