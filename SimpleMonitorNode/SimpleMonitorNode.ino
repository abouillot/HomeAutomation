// Simple Sensor node framework
// Minimize power consumption to run on battery
// copyright 2016 Alexandre Bouillot - FLTM @abouillot

#include <avr/sleep.h>
#include <avr/power.h>
#include <util/atomic.h>

// RTC
#include <Time.h>
#include <Wire.h>
#include <MCP7940RTC.h>

// Flash memory
#include <SPI.h>
#include <SPIFlash.h>       //get it here: https://www.github.com/lowpowerlab/spiflash

// Radio
#include <RFM69.h>          // get it here https://www.github.com/lowpowerlab/rfm69

// FOTA
//#include <WirelessHEX.h>  //get it here: https://github.com/LowPowerLab/WirelessProgramming/tree/master/WirelessHEX69

// DHT - temperature sensor
#include <dht.h>            //get it here: http://arduino.cc/playground/Main/DHTLib



//general --------------------------------
#define SERIAL_BAUD   115200
#if 1
#define DEBUGSTART()             Serial.begin(SERIAL_BAUD)
#define DEBUGSTOP()              Serial.end()
#define DEBUGFLUSH()             Serial.flush();
#define DEBUG1(expression)       Serial.print(expression)
#define DEBUG2(expression, arg)  Serial.print(expression, arg)
#define DEBUGLN1(expression)     Serial.println(expression)
#else
#define DEBUGSTART()
#define DEBUGSTOP()
#define DEBUGFLUSH()             Serial.flush();
#define DEBUG1(expression)
#define DEBUG2(expression, arg)
#define DEBUGLN1(expression)
#endif

#define ONESECOND      (1000L)
#define FIVESECONDS    (ONESECOND * 5)
#define FIFTEENSECONDS (ONESECOND * 15)
#define THIRTYSECONDS  (ONESECOND * 30)
#define HALFMINUTE     (ONESECOND * 30)
#define ONEMINUTE      (ONESECOND * 60)
#define TWOMINUTES     (ONEMINUTE * 2)
#define FIVEMINUTES    (ONEMINUTE * 5)
#define TENMINUTES     (ONEMINUTE * 10)

#define FOREVER      0xFFFFFFFF

// RFM69
#define NODEID        20    //unique for each node on same network
#define NETWORKID     101  //the same on all nodes that talk to each other
#define GATEWAYID     1
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY   RF69_868MHZ
//#define FREQUENCY   RF69_915MHZ
#define ENCRYPTKEY    "xxxxxxxxxxxxxxxx" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
//#define RFM_INT  2  //RTC Interrupt connected on pin D2 (INT0)
//#define DEVICE_CLASS_A  // Device wil lsend data and then put radio to sleep
#define DEVICE_CLASS_B  // Device will send data and then wait CLASS_B_DELAY second before putting rasio to sleep
//#define DEVICE_CLASS_C  // Device will keep it radio on all the tile
#define CLASS_B_DELAY  FIVESECONDS

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
byte sendSize = 0;
boolean requestACK = false;
RFM69 radio;
//end RFM69 ------------------------------------------

// RTC
MCP7940RTC *pRTC;
#define RTC_INT  3  //RTC Interrupt connected on pin D3 (INT1)

// SPI flash
// Anarduino: Flash SPI_CS = 5, ID = 0x.... (Winbond 4Mbit flash)
//  SPIFlash flash(5, 0x0120); // flash(SPI_CS, MANUFACTURER_ID)
SPIFlash flash(5); // Flash memory CS on pin 5

//LED
#define LED 9  // Pin 9 on Anarduino

//http://interface.khm.de/index.php/lab/interfaces-advanced/sleep_watchdog_battery/


/// Low-power utility code using the Watchdog Timer (WDT). Requires a WDT
/// interrupt handler, e.g. EMPTY_INTERRUPT(WDT_vect);
class Sleepy {
public:
  /// start the watchdog timer (or disable it if mode < 0)
  /// @param mode Enable watchdog trigger after "16 << mode" milliseconds
  ///             (mode 0..9), or disable it (mode < 0).
  /// @note If you use this function, you MUST included a definition of a WDT
  /// interrupt handler in your code. The simplest is to include this line:
  ///
  ///     ISR(WDT_vect) { Sleepy::watchdogEvent(); }
  ///
  /// This will get called when the watchdog fires.
  static void watchdogInterrupts (char mode);

  /// enter low-power mode, wake up with watchdog, INT0/1, or pin-change
  static void powerDown ();

  /// Spend some time in low-power mode, the timing is only approximate.
  /// @param msecs Number of milliseconds to sleep, in range 0..65535.
  /// @returns 1 if all went normally, or 0 if some other interrupt occurred
  /// @note If you use this function, you MUST included a definition of a WDT
  /// interrupt handler in your code. The simplest is to include this line:
  ///
  ///     ISR(WDT_vect) { Sleepy::watchdogEvent(); }
  ///
  /// This will get called when the watchdog fires.
  static byte loseSomeTime (word msecs);

  /// This must be called from your watchdog interrupt code.
  static void watchdogEvent();
};

// Interupt routine called by watchdog
ISR(WDT_vect) {
  Sleepy::watchdogEvent();
}


static volatile byte watchdogCounter;

void Sleepy::watchdogEvent() {
  ++watchdogCounter;
}
// chk
void Sleepy::watchdogInterrupts (char mode) {
  /* In order to change WDE or the prescaler, we need to
   * set WDCE (This will allow updates for 4 clock cycles).
   */
  word wdp = mode;
  // correct for the fact that WDP3 is *not* in bit position 3!
  if (wdp & bit(3))
    wdp ^= bit(3) | bit(WDP3);
  //  Serial.print("wdp ");Serial.print(wdp, BIN); Serial.print(" mode ");Serial.print(mode,BIN); Serial.println(); Serial.flush();
  // if mode == -1, set WDT in reset mode, otherwise, clear the RESET and enable the interrupt
  byte wdtcsr = wdp >= 0 ? bit(WDIE) | wdp : 0;

  // Clear the Watchdog Reset flag
  MCUSR &= ~(1 << WDRF);

  // Ensure the following cannot be interrupted by other int
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
#ifndef WDTCSR    // This is probably a XXX chip, where this doesn't exist under this name
#define WDTCSR WDTCR  // substitute the right namr
#endif
    WDTCSR |= (1 << WDCE) | (1 << WDE); // timed sequence
    // Set the new watchdog timeout prescaler value
    WDTCSR = wdtcsr;
  }
}

/// @see http://www.nongnu.org/avr-libc/user-manual/group__avr__sleep.html
void Sleepy::powerDown () {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  /* 318 µA */

  // disable ADC  // save ~200 µA
  ADCSRA = 0;  

  noInterrupts();
  sleep_enable();

  // turn off brown-out enable in software    // save ~25 µA
  MCUCR = bit (BODS) | bit (BODSE);  // turn on brown-out enable select
  MCUCR = bit (BODS);        // this must be done within 4 clock cycles of above
  interrupts ();             // guarantees next instruction executed

  sleep_cpu ();              // sleep within 3 clock cycles of above

  /* The program will continue from here after the WDT timeout*/
  sleep_disable(); /* First thing to do is disable sleep. */

  /* Re-enable the peripherals. */
  power_all_enable();
}

// chk
byte Sleepy::loseSomeTime (word msecs) {
  byte ok = 1;
  // msleft is updated thru the loop to keep track of ms required under 'deep sleep'
  // initialy, this is equal to the time requested
  word msleft = msecs;

  // only slow down for periods longer than the watchdog granularity
  while (msleft >= 16) {
    char wdp = 0; // wdp 0..9 corresponds to roughly 16..8192 ms
    // calc wdp as log2(msleft/16), i.e. loop & inc while next value is ok
    for (word m = msleft; m >= 32; m >>= 1) {
      // limit is the factor 9 (~8s)
      if (++wdp >= 9)
        break;
    }

    // reset the counter, so we will know if we exit thru the watchdog, or thru other means
    watchdogCounter = 0;

    // Prepare the watchdog registers
    watchdogInterrupts(wdp);

    // sleep for good
    powerDown();

    // Stop the watchdog interrupts
    watchdogInterrupts(-1); // off

    // watchdogCounter hasn't been set by the watchdog interrupt - other mean has been used for waking up
    if (watchdogCounter == 0) {
      // when interrupted, our best guess is that half the time has passed
      msleft -= 8 << wdp;
      ok = 0; // lost some time, but got interrupted
      break;
    }
    else {
      msleft -= 0x10 << wdp;
    }
  }

  // adjust the milli ticks, since we will have missed several
#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__) || defined (__AVR_ATtiny44__) || defined (__AVR_ATtiny45__)
  extern volatile unsigned long millis_timer_millis;
  millis_timer_millis += msecs - msleft;
#else
  extern volatile unsigned long timer0_millis;
  timer0_millis += msecs - msleft;
#endif
  return ok;
}


class Active {
public:
  virtual unsigned long Run() = 0;

protected:
  typedef enum {
    Uninitialized = -1, Init, Standby, PowerOn, Stabilize, Read, Transmit, PowerOff, LastCoreState
  }
  ActiveState;
  ActiveState State;
  unsigned long NextTime;
};

class RadioActive :
public Active {
public:
  void SendData(const uint8_t node, const void *data, const uint8_t length);
  int ReceiveData();
  unsigned long Run();

private:
  boolean active;
};

class Led :
public Active {
public:
  void RequestBlink(int blinkRequested);
  unsigned long Run();

private:
  int remainingBlink;
  static const int blinkPeriod = 16;  // set to a full cycle of power off
};

class DHT :
public Active {
public:
  unsigned long Run();
private:
#define DHTTYPE DHT22   // DHT 22 (AM2302) white one
#define TEMP_READ_DELAY  ONEMINUTE
#define TEMP_STABILIZE_DELAY  ONESECOND
#define DHTPIN 7     			// digital pin we're connected to
#define DHTPOWERPIN 8                  // Pin powering DHT Sensor
  dht DHT;

  double h;
  double t;
  int chk;
};

class Battery :
public Active {
public:
  unsigned long Run();
private:
  #define BATT_READ_DELAY  FIVEMINUTES
  #define BATTPIN A0     			// digital pin we're connected to
  float volts;
};


Led led;
DHT dht;
Battery battery;
RadioActive radioSend;


/////////////////////////////
// RadioActive
/////////////////////////////
void RadioActive::SendData(const uint8_t node, const void* data, const uint8_t length) {
  DEBUG1("Send Data ");
  DEBUG1(node);
  DEBUG1(" ");
  DEBUG1(length);
  int i;
  unsigned char *p = (unsigned char *)data;
  if (radio.sendWithRetry(node, (const void*)(data), length/*, 2, 100*/)) {
    // ackReceived++;
    DEBUGLN1(" ACK received");
  }
  else {
    // ackMissed++;
    DEBUGLN1(" ACK missed");
  }
  radio.receiveDone();
  State = Transmit;

#if defined (DEVICE_CLASS_A)
  NextTime = millis();
#elif defined (DEVICE_CLASS_B)
  NextTime = millis() + CLASS_B_DELAY;
#elif defined (DEVICE_CLASS_C)
  NextTime = FOREVER;
#else
#error "No radio Class Defined"
#endif
  led.RequestBlink(1);
}

int RadioActive::ReceiveData() {
  //check for any received packets
  if (radio.receiveDone()) {
    // CheckForWirelessHEX(radio, flash);

    DEBUG1('[');
    DEBUG2(radio.SENDERID, DEC);
    DEBUG1("] to [");
    DEBUG2(radio.TARGETID, DEC);
    DEBUG1("] ");
    for (byte i = 0; i < radio.DATALEN; i++) {
      DEBUG2((unsigned char)radio.DATA[i], HEX);
      DEBUG1('.');
    }
    DEBUG1("   [RX_RSSI:");
    DEBUG1(radio.RSSI);
    DEBUG1("]");
    DEBUGLN1();

    if (radio.ACKRequested()
      && radio.TARGETID == NODEID
      ) {
      delay(3); // Pause needed when sending right after receiving
      radio.sendACK();
      DEBUGLN1(" - ACK sent");
    }
    led.RequestBlink(1);
  }
}

unsigned long RadioActive::Run() {
  // do not check radio when it is in standby state. This in order to save battery, as any call to radio, when asleep, will power it back again
  if (State != Standby) {
    ReceiveData();
  }

  if (millis() < NextTime) {
    // we haven't yet reached a time where we have some state transition to happen, return imediately
    return NextTime;
  }

  switch (State) {
  case Transmit:

#if defined (DEVICE_CLASS_A) || defined (DEVICE_CLASS_B)
    DEBUGLN1("Switch off radio");
    NextTime = FOREVER;
    radio.sleep();
#elif defined(DEVICE_CLASS_C)
#else
#error "No radio Class Defined"
#endif
    State = Standby;
    break;
  }
  return NextTime;
}

/////////////////////////////
// LED
/////////////////////////////
void Led::RequestBlink(int blinkRequested) {
  remainingBlink = blinkRequested;
  State = PowerOn;
  NextTime = millis() + 1;
}

unsigned long Led::Run() {
  if (millis() < NextTime) {
    // we haven't yet reached a time where we have some state transition to happen, return imediately
    return NextTime;
  }
  switch (State) {
  case Init:
    pinMode(LED, OUTPUT);
    NextTime = FOREVER;
    break;
  case Standby:
    NextTime = FOREVER;
    break;
  case PowerOn:
    digitalWrite(LED, 1);
    NextTime = millis() + blinkPeriod;
    State = PowerOff;
    break;
  case PowerOff:
    digitalWrite(LED, 0);
    NextTime = millis() + blinkPeriod;
    if (--remainingBlink) {
      State = PowerOn;
    }
    else {
      State = Standby;
    }
    break;
  }
  return NextTime;
}

/////////////////////////////
// TEMP Sensor
/////////////////////////////
unsigned long DHT::Run() {
  if (millis() < NextTime) {
    // we haven't yet reached a time where we have some state transition to happen, return imediately
    return NextTime;
  }
  switch (State) {
  case Init:
    NextTime = millis() + TEMP_READ_DELAY;
    State = Standby;
    break;
  case Standby:
    State = PowerOn;
    break;
  case PowerOn:
    DEBUGLN1("Power on DHT");
    pinMode(DHTPIN, INPUT_PULLUP);
    pinMode(DHTPOWERPIN, OUTPUT);
    digitalWrite(DHTPOWERPIN, 1);
    State = Stabilize;
    NextTime = millis() + TEMP_STABILIZE_DELAY;
    break;
  case Stabilize:
    //      if (millis() > temperatureTime) {
    State = Read;
    //      }
    break;
  case Read:
    chk = DHT.read21(DHTPIN);

    DEBUGLN1("Power off DHT");
    pinMode(DHTPOWERPIN, INPUT);
    digitalWrite(DHTPOWERPIN, 0);
    pinMode(DHTPIN, INPUT);
    digitalWrite(DHTPIN, 0);  //ensure no consumption on 
 
    State = PowerOff;
    switch (chk)
    {
    case DHTLIB_OK:
      DEBUG1("OK,\t");
      State = Transmit;
      break;
    case DHTLIB_ERROR_CHECKSUM:
      DEBUG1("Checksum error,\t");
      break;
    case DHTLIB_ERROR_TIMEOUT:
      DEBUG1("Time out error,\t");
      break;
    case DHTLIB_ERROR_CONNECT:
      DEBUG1("Connect error,\t");
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

    h = DHT.humidity;
    // Read temperature as Celsius
    t = DHT.temperature;

    DEBUG1("Humidity=");
    DEBUG1(h);
    DEBUG1("   Temp=");
    DEBUG1(t);
    DEBUG1("°C");
    DEBUGLN1();
    break;
  case Transmit:
    //send data
    theData.deviceID = 6;
    theData.var1_usl = millis();
    theData.var2_float = t;
    theData.var3_float = h;
    //      frameSent++;
    radioSend.SendData(GATEWAYID, (const void*)(&theData), sizeof(theData));

    State = PowerOff;
    break;
  case PowerOff:
    State = Standby;
    NextTime = millis() + TEMP_READ_DELAY;
    break;
  }
  return NextTime;
}

/////////////////////////
//
// Battery monitoring
//
/////////////////////////
static const float referenceVolts = 3.3;
static const float R1 = 926;  // 926K for a 1M - from V+ to point of measure
static const float R2 = 989;  // 989K for a 1M - from GND to point of measure
static const float resistorFactor = 255 / (R2 / (R1 + R2));
static const int batteryPin = A0; // where battery is connected

unsigned long Battery::Run() {
  if (millis() < NextTime) {
    // we haven't yet reached a time where we have some state transition to happen, return imediately
    return NextTime;
  }
  int val;
  switch (State) {
  case Init:
    pinMode(BATTPIN, INPUT);
    NextTime = millis();    // immediate read
    State = Standby;
    break;
  case Standby:
    State = Stabilize;
    break;
  case Stabilize:
    State = Read;
    analogRead(0);  // consume first read
    break;
  case Read:
    volts = ((analogRead(0) / resistorFactor) * referenceVolts);
    DEBUGLN1(volts);
    State = Transmit;
    break;
  case Transmit:
    //send data
    theData.deviceID = 7;
    theData.var1_usl = millis();
    theData.var2_float = volts;
    theData.var3_float = 0;
    radioSend.SendData(GATEWAYID, (const void*)(&theData), sizeof(theData));
    State = PowerOff;
    break;
  case PowerOff:
    State = Standby;
    NextTime = millis() + BATT_READ_DELAY;
    break;
  }
  return NextTime;
}

void setup() {
  DEBUGSTART();
  Serial.println(__FILE__);
  Serial.print("Network "); 
  Serial.println(NETWORKID);
  Serial.print("Node "); 
  Serial.println(NODEID);
  Serial.print("Gateway "); 
  Serial.println(GATEWAYID);
  Serial.println();

  // Set all pin as output and low to minimize consumption.
  for (byte i = 0; i <= A7; i++) {
    pinMode (i, INPUT);
    digitalWrite (i, LOW);
  }

  // disable the LED
  digitalWrite(LED, 0);

  // configure RTC chip
  pinMode(RTC_INT, INPUT);

  pRTC = new MCP7940RTC();
  pRTC->set(1387798395);
  pRTC->setTimeRTC(1388534400); // 2014-01-01 00:00:00

  // configure Flash memory chip
  // put flash memory to sleep
  // Anarduino: Flash SPI_CS = 5, ID = 0xEF30 (Winbond 4Mbit flash)
  //  SPIFlash flash(5, 0x0120); // flash(SPI_CS, MANUFACTURER_ID)
  //  SPIFlash flash(5); // flash(SPI_CS, MANUFACTURER_ID)
  if (flash.initialize()) {
    DEBUGLN1("Flash Init OK!");
    flash.sleep();          // put flash (if it exists) into low power mode
    DEBUGLN1("Flash sleep");
  }
  else {
    DEBUGLN1("Flash Init FAIL!");
  }
  // Configure Radio module
  radio.initialize(FREQUENCY, NODEID, NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.setPowerLevel(31);
  radio.encrypt(ENCRYPTKEY);
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY == RF69_433MHZ ? 433 : FREQUENCY == RF69_868MHZ ? 868 : 915);
  DEBUGLN1(buff);
  theData.nodeID = NODEID;  //this node id should be the same for all devices in this node
  //  radio.promiscuous(true);
  radio.promiscuous(false);
  radio.sleep();
}

void loop() {
  uint32_t nextRun = FOREVER;
  nextRun = min(nextRun, led.Run());
  nextRun = min(nextRun, dht.Run());
  nextRun = min(nextRun, battery.Run());
  nextRun = min(nextRun, radioSend.Run());

  long suspend = nextRun - millis();
  if (suspend > 0 ) {
    DEBUGFLUSH();
    Sleepy::loseSomeTime(suspend);
  }
}
