/* This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Arduino.h"
#include "heltec.h"
#include <Wire.h>
#include <SPI.h>
#include <lmic.h>
#include <hal/hal.h>
#include <lmic/lmic_util.h>

#if(USE_MY_CREDENTIALS == 1)
    #include "credentials.h"
#else
    #include "credentials_template.h"
#endif

#include <SHT1x.h>

// Specify data and clock connections and instantiate SHT1x object
#define dataPin  21
#define clockPin 22

// default to 5.0v boards, e.g. Arduino UNO
SHT1x sht1x(dataPin, clockPin);


void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

static osjob_t sendjob;
static uint8_t payload[8];

// Schedule TX every this many seconds (might become longer due to duty cycle limitations).
const unsigned TX_INTERVAL = 60;

const lmic_pinmap lmic_pins = {
    .nss = 18,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 14,
    .dio = {26,35,34},
};

void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16)
        Serial.print('0');
    Serial.print(v, HEX);
}

// Sensor emulation
float temperature_ = 0;
uint16_t pressure_ = 0;
float humidity_ = 0;
uint16_t weight_ = 0;

void do_send(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
#if 1 
        temperature_ += 0.2;
        humidity_ += 0.2;
         
        if(temperature_ > 85.0)
            temperature_ = -40.0;

        if(humidity_ > 100)
            humidity_ = 0;
#else
        temperature_ = sht1x.readTemperatureC();
        humidity_ = sht1x.readHumidity();
#endif
        
        pressure_ += 10; 
        weight_ += 1;

        if(pressure_ > 2000)
            pressure_ = 0; 

        if(weight_ > 200)
            weight_ = 0;       

        Heltec.display->clear();
        Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
        Heltec.display->setFont(ArialMT_Plain_10);
        Heltec.display->drawString(2, 0, "Temperature: " + String(temperature_) + " °C");
        Heltec.display->drawString(2, 10, "Humidity: " + String(humidity_) + " %");
        Heltec.display->drawString(2, 20, "Pressure: " + String(pressure_) + " hPa");
        Heltec.display->drawString(2, 30, "Weight: " + String(weight_) + " Kg");
        Heltec.display->drawString(2, 40, "FFT: ");
        Heltec.display->display();         

        // warning 
        // adjust for the f2sflt16 range (-1 to 1)
        //temperature = temperature / 100; 

        // note: this uses the sflt16 datum (https://github.com/mcci-catena/arduino-lmic#sflt16)
        uint16_t payloadTemperature = temperature_ * 100;
        //uint16_t payloadTemperature = LMIC_f2sflt16(temperature_);

        uint16_t payloadHumidity = humidity_ * 100;
        //uint16_t payloadHumidity = LMIC_f2sflt16(humidity_);

        payload[0] = highByte(payloadTemperature);
        payload[1] = lowByte(payloadTemperature);
        payload[2] = highByte(payloadHumidity);
        payload[3] = lowByte(payloadHumidity);
        payload[4] = highByte(pressure_);
        payload[5] = lowByte(pressure_);
        payload[6] = highByte(weight_);
        payload[7] = lowByte(weight_);

        //check ttn decoder in tnn-decoder.js 
         
        // prepare upstream data transmission at the next possible time.
        // transmit on port 1 (the first parameter); you can use any value from 1 to 223 (others are reserved).
        // don't request an ack (the last parameter, if not zero, requests an ack from the network).
        // Remember, acks consume a lot of network resources; don't ask for an ack unless you really need it.     
        LMIC_setTxData2(1, payload, sizeof(payload), 0);
       
        Serial.println(F("Packet queued"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            
            {
              u4_t netid = 0;
              devaddr_t devaddr = 0;
              u1_t nwkKey[16];
              u1_t artKey[16];
              LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
              Serial.print("netid: ");
              Serial.println(netid, DEC);
              Serial.print("devaddr: ");
              Serial.println(devaddr, HEX);
              Serial.print("AppSKey: ");
              for (size_t i=0; i<sizeof(artKey); ++i) {
                if (i != 0)
                  Serial.print("-");
                printHex2(artKey[i]);
              }
              Serial.println("");
              Serial.print("NwkSKey: ");
              for (size_t i=0; i<sizeof(nwkKey); ++i) {
                      if (i != 0)
                              Serial.print("-");
                      printHex2(nwkKey[i]);
              }
              Serial.println();
            }

            // Disable link check validation (automatically enabled
            // during join, but because slow data rates change max TX
            // size, we don't use it in this example.
            LMIC_setLinkCheckMode(0);
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        /*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_SCAN_FOUND:
        ||    Serial.println(F("EV_SCAN_FOUND"));
        ||    break;
        */
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART"));
            break;
        case EV_TXCANCELED:
            Serial.println(F("EV_TXCANCELED"));
            break;
        case EV_RXSTART:
            /* do not print anything -- it wrecks timing */
            break;
        case EV_JOIN_TXCOMPLETE:
            Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
            break;
        
        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            break;
    }
}

void setup() {
  // heltec display and lora initiation
  Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Disable*/, false /*Serial Enable*/);

  // Lora connection initiation
  SPI.begin(5, 19, 27);
  
  //Heltec.display->flipScreenVertically();
  Heltec.display->clear();
  Heltec.display->setFont(ArialMT_Plain_24);
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  Heltec.display->drawString(20, 0, "Test Sensor");
  Heltec.display->display();

  // Serial monitor initiation
  Serial.begin(9600); // Sets the speed needed to read from the serial port when connected
  while (!Serial); // Loop that only continues once the serial port is active (true)
  Serial.println(F("entering Setup"));


  // LMIC init (TTN)
  // os_init_ex( (const void *) &lmic_pins ) );
  os_init();

  // Reset the MAC state. Session and pending data transfers will be discarded.  
  LMIC_reset(); 
  
  // Disable link-check mode and ADR, because ADR tends to complicate testing.
  LMIC_setLinkCheckMode(0);  

  // Set the data rate to Spreading Factor 7.  This is the fastest supported rate for 125 kHz channels, and it
  // minimizes air time and battery power. Set the transmission power to 14 dBi (25 mW).
  LMIC_setDrTxpow(DR_SF7, 14);  
  
  // Start job (sending automatically starts OTAA too)
  do_send(&sendjob);
}

void loop() {
  // we call the LMIC's runloop processor. This will cause things to happen based on events and time. One
  // of the things that will happen is callbacks for transmission complete or received messages. We also
  // use this loop to queue periodic data transmissions.  You can put other things here in the `loop()` routine,
  // but beware that LoRaWAN timing is pretty tight, so if you do more than a few milliseconds of work, you
  // will want to call `os_runloop_once()` every so often, to keep the radio running.
  os_runloop_once(); // Readings taken, sent and looped in the do_send() function called from setup

#if 0
    //Heltec.display->
    
    temperature_ = sht1x.readTemperatureC();
    humidity_ = sht1x.readHumidity();

    Heltec.display->clear();
    Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->drawString(2, 0, "Temperature: " + String(temperature_) + " °C");
    Heltec.display->drawString(2, 10, "Humidity: " + String(humidity_) + " %");
    Heltec.display->drawString(2, 20, "Pressure: " + String(pressure_) + " hPa");
    Heltec.display->drawString(2, 30, "Weight: " + String(weight_) + " Kg");
    Heltec.display->drawString(2, 40, "FFT: ");
    Heltec.display->display(); 
    delay(2000);
#endif        
}
