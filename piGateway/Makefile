install : Gatewayd
	cp Gatewayd /usr/local/bin
	cp Gatewayd /etc/init.d/Gatewayd
	chmod +x /etc/init.d/Gatewayd
	update-rc.d Gatewayd defaults
	service Gatewayd start
	
	
uninstall:
	service Gatewayd stop
	update-rc.d -f Gatewayd remove
	rm /etc/init.d/Gatewayd
	rm /usr/local/bin/Gatewayd

Gatewayd : Gateway.cpp ../Common/source/rfm69/rfm69.cpp ../Common/include/RFM69HomeAutomation.h ../Common/include/rfm69/rfm69.h ../Common/include/rfm69/rfm69registers.h ../Common/include/RFM69Network.h
	g++ -std=c++11 Gateway.cpp ../Common/source/rfm69/rfm69.cpp -o Gatewayd -I../Common/include -lwiringPi -lmosquitto -DRASPBERRY -DDAEMON

Gateway : Gateway.cpp ../Common/source/rfm69/rfm69.cpp ../Common/include/RFM69HomeAutomation.h ../Common/include/rfm69/rfm69.h ../Common/include/rfm69/rfm69registers.h ../Common/include/RFM69Network.h
	g++ -std=c++11 Gateway.cpp ../Common/source/rfm69/rfm69.cpp -o Gateway -I../Common/include -lwiringPi -lmosquitto -DRASPBERRY

SenderReceiver : SenderReceiver.cpp ../Common/source/rfm69/rfm69.cpp ../Common/include/rfm69/rfm69.h ../Common/include/rfm69/rfm69registers.h ../Common/include/RFM69Network.h
	g++ -std=c++11 SenderReceiver.cpp ../Common/source/rfm69/rfm69.cpp -o SenderReceiver -I../Common/include -lwiringPi -DRASPBERRY

