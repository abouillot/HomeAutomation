install : Gatewayd
	cp Gatewayd /usr/local/bin
	cp GatewaydService /etc/init.d/Gatewayd
	chmod +x /etc/init.d/Gatewayd
	update-rc.d Gatewayd enable
	service Gatewayd start
	
	
uninstall: Gatewayd
	service Gatewayd stop
	update-rc.d -f Gatewayd disable
	rm /etc/init.d/Gatewayd
	rm /usr/local/bin/Gatewayd

Gatewayd : Gateway.c rfm69.cpp
	g++ Gateway.c rfm69.cpp -o Gatewayd -lwiringPi -lmosquitto -DRASPBERRY -DDAEMON

Gateway : Gateway.c rfm69.cpp
	g++ Gateway.c rfm69.cpp -o Gateway -lwiringPi -lmosquitto -DRASPBERRY
