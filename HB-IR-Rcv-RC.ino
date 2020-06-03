//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2018-10-10 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2019-01-16 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

// Steuerung mittels SUBMIT-Commands: dom.GetObject("BidCos-RF.<Device Serial>:<Ch#>.SUBMIT").State("<command>");
// <command> Typen:
// Löschen einer Chip ID:             0xcc

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER


#define EI_NOTEXTERNAL

#include <EnableInterrupt.h>
#include <SPI.h>

#include <AskSinPP.h>

#include <Register.h>
#include <Device.h>
#include <MultiChannelDevice.h>
#include <IRremote.h>

#include "IRRecv.h"

#define IR_PIN               A4

#define LED1_PIN              5
#define LED2_PIN              4

#define CONFIG_BUTTON_PIN     8


#define NUM_CHANNELS          8
// number of available peers per channel
#define PEERS_PER_CHANNEL     10

// all library classes are placed in the namespace 'as'
using namespace as;

IRrecv irrecv(IR_PIN);
decode_results results;


// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
  {0xf3, 0x3d, 0x00},     // Device ID
  "JPIRRC0001",           // Device Serial
  {0xf3, 0x3d},           // Device Model
  0x10,                   // Firmware Version
  as::DeviceType::Remote, // Device Type
  {0x00, 0x00}            // Info Bytes
};

/**
   Configure the used hardware
*/
typedef LibSPI<10> RadioSPI;
typedef DualStatusLed<LED2_PIN, LED1_PIN> LedType;
typedef AskSin<LedType, NoBattery, Radio<RadioSPI, 2> > Hal;
Hal hal;

DEFREGISTER(IRRECVReg0, MASTERID_REGS, DREG_TRANSMITTRYMAX)
class IRRECVList0: public RegList0<IRRECVReg0> {
public:
  IRRECVList0(uint16_t addr) :
    RegList0<IRRECVReg0>(addr) {
  }

    void defaults () {
      clear();
      transmitDevTryMax(2);
    }
};

typedef IRRECVChannel<Hal, PEERS_PER_CHANNEL, IRRECVList0> IrRecvChannel;

class IRRECVDev : public MultiChannelDevice<Hal, IrRecvChannel, NUM_CHANNELS, IRRECVList0> {
public:
  typedef MultiChannelDevice<Hal, IrRecvChannel, NUM_CHANNELS, IRRECVList0> DevType;
  IRRECVDev(const DeviceInfo& i, uint16_t addr) : DevType(i, addr) {}
  virtual ~IRRECVDev() { }

    // return ir receiver channel from 0 - n-1
  IrRecvChannel& irRecvChannel (uint8_t num) {
      return channel(num + 1);
    }
    // return number of ir channels
    uint8_t irRecvCount () const {
      return channels();
    }

    void configChanged() {
      DPRINTLN("Config Changed List0");
    }


    bool init(Hal& hal) {
      DevType::init(hal);
      return true;
    }
};

IRRECVDev sdev(devinfo, 0x20);
ConfigButton<IRRECVDev> cfgBtn(sdev);
IRRECVScanner<IRRECVDev, IrRecvChannel, irrecv, LED2_PIN, LED1_PIN> scanner(sdev);

void setup () {
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
  sdev.init(hal);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  sdev.initDone();
  irrecv.enableIRIn(); // Start the receiver
  sysclock.add(scanner);
}

void loop() {
  hal.runready();
  sdev.pollRadio();
  if (irrecv.decode(&results)) {
    scanner.setIrCode(results);
    //DHEX((uint32_t)results.value);
    irrecv.resume();
  }
}
