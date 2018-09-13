COMMON_INCLUDE_DIRS    := ../Common/include

COMMON_INCLUDES        := ../Common/include/RFM69HomeAutomationCfg.h ../Common/include/RFM69NetworkCfg.h ../Common/include/rfm69/rfm69.h ../Common/include/rfm69/rfm69registers.h
COMMON_SOURCES         := ../Common/source/rfm69/rfm69.cpp
COMMON_LIBS            := wiringPi
COMMON_DEFINES         := RASPBERRY

GATEWAY_SOURCES        := Gateway.cpp $(COMMON_SOURCES)
SENDERRECEIVER_SOURCES := SenderReceiver.cpp $(COMMON_SOURCES)

GATEWAY_LIBS           := mosquitto

GATEWAY_DAEMON_DEFINES := DAEMON

CC                      = g++
CFLAGS                  = -std=c++11 -o $@ -I$(COMMON_INCLUDE_DIRS) -l$(COMMON_LIBS) -D$(COMMON_DEFINES) -Wall
#CFLAGS                  = -std=c++11 -E -I$(COMMON_INCLUDE_DIRS) -l$(COMMON_LIBS) -D$(COMMON_DEFINES) -Wall


systemd-install : Gatewayd
	cp Gatewayd /usr/local/bin
	chmod +x /usr/local/bin/Gatewayd
	cp Gateway.service /etc/systemd/system
	systemctl daemon-reload
	systemctl enable Gateway.service
	systemctl start Gateway.service


systemd-uninstall:
	systemctl stop Gateway.service
	systemctl disable Gateway.service
	rm /etc/systemd/system/Gateway.service
	rm /usr/local/bin/Gatewayd


Gatewayd : $(GATEWAY_SOURCES) $(COMMON_INCLUDES)
	$(CC) $(GATEWAY_SOURCES) $(CFLAGS) -l$(GATEWAY_LIBS) -D$(GATEWAY_DAEMON_DEFINES)

Gateway  : $(GATEWAY_SOURCES) $(COMMON_INCLUDES)
	$(CC) $(GATEWAY_SOURCES) $(CFLAGS) -l$(GATEWAY_LIBS)

SenderReceiver : $(SENDERRECEIVER_SOURCES) $(COMMON_INCLUDES)
	$(CC) $(SENDERRECEIVER_SOURCES) $(CFLAGS)

.PHONY: all
all: Gateway Gatewayd SenderReceiver

.PHONY: clean
clean:
	rm -f Gateway Gatewayd SenderReceiver
