//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2018-05-03 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2019-01-14 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef __IRRECV_H__
#define __IRRECV_H__

#include "MultiChannelDevice.h"
#include "Register.h"
#include <IRremote.h>


#define   ID_ADDR_SIZE 8

namespace as {

DEFREGISTER(IRRECVReg1,CREG_AES_ACTIVE,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7)
class IRRECVList1 : public RegList1<IRRECVReg1> {
public:
  IRRECVList1 (uint16_t addr) : RegList1<IRRECVReg1>(addr) {}
  void defaults () {
    clear();
  }
};

class ChipIdMsg : public Message {
  public:

  bool free(uint8_t*addr) {
   bool f = true;
   for (uint8_t n = 0; n < ID_ADDR_SIZE; n++) {
    if (addr[n] != 0x00) {
      f = false;
      break;
    }
   }
   return f;
  }

    void init(uint8_t msgcnt, uint8_t ch, uint8_t*addr) {
      char hexstr[ID_ADDR_SIZE * 2];
      if (free(addr)) {
        for (uint8_t n = 0; n < (ID_ADDR_SIZE * 2); n++)
            hexstr[n] = 0x20;
      } else {
        //convert address to hex-string - from https://stackoverflow.com/questions/6357031/how-do-you-convert-a-byte-array-to-a-hexadecimal-string-in-c
        unsigned char * pin = addr;
        const char * hex = "0123456789ABCDEF";
        char * pout = hexstr;
        uint8_t i = 0;
        for(; i < ID_ADDR_SIZE-1; ++i){
          *pout++ = hex[(*pin>>4)&0xF];
          *pout++ = hex[(*pin++)&0xF];
        }
        *pout++ = hex[(*pin>>4)&0xF];
        *pout++ = hex[(*pin)&0xF];
        *pout = 0;

      }
      //DPRINT("hexstr=");DPRINTLN(hexstr);
      Message::init(0x1a, msgcnt, 0x53, BIDI , ch , hexstr[0]);
      for (uint8_t i = 1; i < (ID_ADDR_SIZE * 2); i++) {
        pload[i-1] = hexstr[i];
      }
    }
};

template<class HALTYPE,int PEERCOUNT,class List0Type=List0>
class IRRECVChannel : public Channel<HALTYPE,IRRECVList1,EmptyList,DefList4,PEERCOUNT,List0Type>, Alarm {

  enum { none=0, released, longpressed, longreleased };

  uint8_t state, matches, repeatcnt;

public:
  typedef Channel<HALTYPE,IRRECVList1,EmptyList,DefList4,PEERCOUNT,List0Type> BaseChannel;

  IRRECVChannel () : BaseChannel(), Alarm(0), state(0), matches(0),repeatcnt(0) {}
  virtual ~IRRECVChannel () {}

  virtual void trigger (__attribute__((unused)) AlarmClock& clock) {
    state = 0;
    this->changed(true);
  }

  uint8_t status () const {
    return state;
  }

  uint8_t flags () const {
    return 0;
  }

  void start () {
    matches <<= 1;
  }

  bool check (uint8_t* addr) {
    if( free() == false && isID(addr) == true ) {
      matches |= 0b00000001;
      return true;
    }
    return false;
  }

  void finish () {
      uint8_t s = none;
      // 3 or 6 matches are longpress and longlongpress
      if( (matches & 0b00111111) == 0b00000111 || (matches & 0b00111111) == 0b00111111 ) {
        s = longpressed;
        DPRINTLN(F("longpressed"));
        // clear longlong
        matches &= 0b11000111;
      }
      // check for long release
      else if( (matches & 0b00001111) == 0b00001110 ) {
        s = longreleased;
        DPRINTLN(F("longreleased"));
      }
      // check for release
      else if( (matches & 0b00000011) == 0b00000010 ) {
        s = released;
        DPRINTLN(F("released"));
      }
      if( s != none ) {
        RemoteEventMsg& msg = (RemoteEventMsg&)this->device().message();
        msg.init(this->device().nextcount(),this->number(),repeatcnt,(s==longreleased || s==longpressed),this->device().battery().low());
        if( s == released || s == longreleased) {
          // send the message to every peer
          this->device().sendPeerEvent(msg,*this);
          repeatcnt++;
        }
        else if (s == longpressed) {
          // broadcast the message
          this->device().broadcastPeerEvent(msg,*this);
        }
      }
  }

  bool match (uint8_t* addr) {
    start();
    bool res = check(addr);
    finish();
    return res;
  }

  bool isID (uint8_t* buf) {
    IRRECVList1 l = this->getList1();
    for( uint8_t n=0; n< ID_ADDR_SIZE; ++n ) {
      if( l.readRegister(0xe0+n) != buf[n] ) {
        return false;
      }
    }
    return true;
  }

  void storeID (uint8_t* buf) {
    if( learn() == true ) {
      for( uint8_t n=0; n < ID_ADDR_SIZE; ++n ) {
        this->getList1().writeRegister(0xe0+n,buf[n]);
      }
      state = 0;
      this->changed(true);
      sysclock.cancel(*this);
    }
  }

  bool free () {
    return {
      this->getList1().readRegister(0xe0) == 0x00 &&
      this->getList1().readRegister(0xe1) == 0x00 &&
      this->getList1().readRegister(0xe2) == 0x00 &&
      this->getList1().readRegister(0xe3) == 0x00 &&
      this->getList1().readRegister(0xe4) == 0x00 &&
      this->getList1().readRegister(0xe5) == 0x00 &&
      this->getList1().readRegister(0xe6) == 0x00 &&
      this->getList1().readRegister(0xe7) == 0x00
    };
  }

  bool learn () const {
    return state == 200;
  }

  bool process (const ActionSetMsg& msg) {
    state = msg.value();
    this->changed(true);
    if( state != 0 ) {
      sysclock.cancel(*this);
      set(seconds2ticks(60));
      sysclock.add(*this);
    }
    return true;
  }

  bool process (const ActionCommandMsg& msg) {
    if ( (msg.len() == ID_ADDR_SIZE) || (msg.len() == 1 && msg.value(0) == 0xcc) ) {
      for( uint8_t n=0; n < ID_ADDR_SIZE; ++n ) {
      uint8_t val =  msg.len() == 1 ? 0x00:msg.value(n);
        this->getList1().writeRegister(0xe0+n,val);
      }
      state = 0;
      this->changed(true);
    }

    return true;
  }

  bool process (__attribute__((unused)) const RemoteEventMsg& msg)   {return false; }
  bool process (__attribute__((unused)) const SensorEventMsg& msg)   {return false; }
};


template <class IRRECVDev,class IRRECVChannel,IRrecv& rdrDev,int LED_GREEN,int LED_RED>
class IRRECVScanner : public Alarm {
  IRRECVDev& dev;
  DualStatusLed<LED_GREEN,LED_RED> led;
  uint8_t cnt;
  bool gotNewIRCode;
  uint8_t irAddr[ID_ADDR_SIZE];
public:
  IRRECVScanner (IRRECVDev& d) : Alarm(millis2ticks(500)), dev(d), cnt(0), gotNewIRCode(false) {
    led.init();
  }
  virtual ~IRRECVScanner () {}

  IRRECVChannel* learning () {
    for( uint8_t i=0; i<dev.irRecvCount(); ++i ) {
      IRRECVChannel& rc = dev.irRecvChannel(i);
      if( rc.learn() == true ) {
        return &rc;
      }
    }
    return 0;
  }

  IRRECVChannel* matches (uint8_t* addr) {
    for( uint8_t i=0; i<dev.irRecvCount(); ++i ) {
      IRRECVChannel& rc = dev.irRecvChannel(i);
      if( rc.match(addr) == true ) {
        return &rc;
      }
    }
    return 0;
  }

  IRRECVChannel* find (uint8_t* addr) {
    for( uint8_t i=0; i<dev.irRecvCount(); ++i ) {
      IRRECVChannel& rc = dev.irRecvChannel(i);
      if( rc.isID(addr) == true ) {
        return &rc;
      }
    }
    return 0;
  }

  void DADDR(uint8_t * addr) {
    for (uint8_t i = 0; i < ID_ADDR_SIZE; i++)
      DHEX(addr[i]);
    DPRINTLN(F(""));
  }

  void start () {
    for( uint8_t i=0; i<dev.irRecvCount(); ++i ) {
      IRRECVChannel& rc = dev.irRecvChannel(i);
      rc.start();
    }
  }

  void finish () {
    for( uint8_t i=0; i<dev.irRecvCount(); ++i ) {
      IRRECVChannel& rc = dev.irRecvChannel(i);
      rc.finish();
    }
  }

  bool check (uint8_t* addr) {
    bool res = false;
    for( uint8_t i=0; i<dev.irRecvCount(); ++i ) {
      IRRECVChannel& rc = dev.irRecvChannel(i);
      res |= rc.check(addr);
    }
    return res;
  }

  void setIrCode(decode_results results) {
    gotNewIRCode = true;

    uint8_t t = (int)results.decode_type > -1 ? results.decode_type : 0xff;
    uint8_t rl = results.rawlen;

    static uint32_t last_v = 0;
    uint32_t v = results.value;
    if (v == 0xFFFFFFFF) v = last_v;
    last_v = v;

    memset(irAddr,0x00, ID_ADDR_SIZE);
    byte addrArr[8];
    addrArr[0] = t;
    addrArr[1] = rl;
    for (uint8_t i = 0; i < sizeof(uint32_t); i++)
      addrArr[i+2] = v >> (i*8) & 0xff;

    memcpy(irAddr, addrArr, ID_ADDR_SIZE);
  }

  bool readIrCode(uint8_t *addr) {
   static uint8_t last_addr[ID_ADDR_SIZE];

   bool success = false;

   memset(addr,0,ID_ADDR_SIZE);

   if (gotNewIRCode) {
     gotNewIRCode = false;
     memcpy(addr, irAddr, ID_ADDR_SIZE);
     DADDR(addr);
     if (memcmp(addr, last_addr, ID_ADDR_SIZE) != 0) {
         //turn a OK-Led on
     }
     success = true;
   }

   memcpy(last_addr,addr,ID_ADDR_SIZE);

   return success;
  }

  void scan () {
    uint8_t addr[ID_ADDR_SIZE];

    start();
    readIrCode(addr);

    if( check(addr) == true ) {
      led.ledOn(millis2ticks(500),0);
    }
    finish();
  }

  bool learn (IRRECVChannel* lc) {
    uint8_t addr[ID_ADDR_SIZE];
    while( readIrCode(addr) == true ) {
      if( find(addr) == 0 ) {
        lc->storeID(addr);
        return true;
      }
    }
    return false;
  }

  void trigger (AlarmClock& clock) {
  // reactivate
  set(millis2ticks(500));
  clock.add(*this);
    ++cnt;
    // check if we have a learning channel
    IRRECVChannel* lc = learning();
    if( lc != 0 ) {
      uint8_t cycle = cnt & 0x01;
      led.ledOn(cycle == 0 ? tick : 0, cycle == 0 ? 0 : tick);
      // if we have learned a new ID
      if( learn(lc) == true ) {
        clock.cancel(*this);
        set(seconds2ticks(5));
        led.ledOff();
        led.ledOn(tick);
        clock.add(*this);
      }
    }
    else {
      // scan the bus now
      scan();
    }
  }
};

}

#endif
