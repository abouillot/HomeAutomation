install : Gatewayd
	cp Gatewayd /usr/local/bin

Gatewayd : Gateway.c rfm69.cpp
	g++ Gateway.c rfm69.cpp -o Gatewayd -lwiringPi -lmosquitto -DRASPBERRY -DDAEMON

Gateway : Gateway.c rfm69.cpp
	g++ Gateway.c rfm69.cpp -o Gateway -lwiringPi -lmosquitto -DRASPBERRY
