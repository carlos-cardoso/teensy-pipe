/*
*  touchablePin.cpp
*  Version 1.0 - David Elvig on 1/2017
* based on Paul Stofregren's touchRead.c from ...
* ------------------------------------------------------------------------
* Teensyduino Core Library
* http://www.pjrc.com/teensy/
* Copyright (c) 2013 PJRC.COM, LLC.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* 1. The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* 2. If the Software is incorporated into a build system that allows
* selection among a list of target devices, then similar target
* devices manufactured by PJRC.COM must be included in the list of
* target devices and selectable in the same manner.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
* BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
* ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
* __________________________________________________________________________
*/

#include "touchablePin.h"

#include "core_pins.h"

#if defined(HAS_KINETIS_TSI) || defined(HAS_KINETIS_TSI_LITE)

#if defined(__MK20DX128__) || defined(__MK20DX256__)
// These settings give approx 0.02 pF sensitivity and 1200 pF range
// Lower current, higher number of scans, and higher prescaler
// increase sensitivity, but the trade-off is longer measurement
// time and decreased range.
#define CURRENT   2 // 0 to 15 - current to use, value is 2*(current+1)
#define NSCAN     9 // number of times to scan, 0 to 31, value is nscan+1
#define PRESCALE  2 // prescaler, 0 to 7 - value is 2^(prescaler+1)
static const uint8_t pin2tsi[] = {
    //0    1    2    3    4    5    6    7    8    9
    9,  10, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255,  13,   0,   6,   8,   7,
    255, 255,  14,  15, 255,  12, 255, 255, 255, 255,
    255, 255,  11,   5
};

#elif defined(__MK66FX1M0__)
#define NSCAN     9
#define PRESCALE  2
static const uint8_t pin2tsi[] = {
    //0    1    2    3    4    5    6    7    8    9
    9,  10, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255,  13,   0,   6,   8,   7,
    255, 255,  14,  15, 255, 255, 255, 255, 255,  11,
    12, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

#elif defined(__MKL26Z64__)
#define NSCAN     5//15 9
#define PRESCALE  6//2
static const uint8_t pin2tsi[] = {
    //0    1    2    3    4    5    6    7    8    9
    9,  10, 255,   2,   3, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255,  13,   0,   6,   8,   7,
    255, 255,  14,  15, 255, 255, 255
};

#endif // __MK....

touchablePin::touchablePin() {
    pinNumber = -1;
}

touchablePin::touchablePin(uint8_t pin) {
    pinNumber = pin;
    initUntouched();
}

touchablePin::touchablePin(uint8_t pin, float maxFactor) {
    pinNumber = pin;
    if (maxFactor > 0)    _maxFactor = maxFactor;
    initUntouched();
}

touchablePin::touchablePin(uint8_t pin, float maxFactor, int numSamples) {
    pinNumber = pin;
    if (maxFactor > 0)    _numSamples = numSamples;
    if (numSamples > 0)    _maxFactor = maxFactor;
    
    initUntouched();
}

int touchablePin::touchReadWithMax(uint8_t pin, bool useMax)
{
    unsigned long timeNow = micros(),
                  targetTime = timeNow + (untouchedTime * _maxFactor);
    
    uint32_t ch;
    
    if (pin >= NUM_DIGITAL_PINS) return 0;
    ch = pin2tsi[pin];
    if (ch == 255) return 0;
    
    *portConfigRegister(pin) = PORT_PCR_MUX(0);
    SIM_SCGC5 |= SIM_SCGC5_TSI;
#if defined(HAS_KINETIS_TSI)
    TSI0_GENCS = 0;
    TSI0_PEN = (1 << ch);
    TSI0_SCANC = TSI_SCANC_REFCHRG(3) | TSI_SCANC_EXTCHRG(CURRENT);
    TSI0_GENCS = TSI_GENCS_NSCN(NSCAN) | TSI_GENCS_PS(PRESCALE) | TSI_GENCS_TSIEN | TSI_GENCS_SWTS;
    delayMicroseconds(10);
    while ((TSI0_GENCS & TSI_GENCS_SCNIP) && (timeNow < targetTime)) {   //   wait
        if (useMax)  timeNow = micros();
    }
    delayMicroseconds(1);
    if (useMax && (timeNow >= targetTime))   return (-1);
    else                                     return *((volatile uint16_t *)(&TSI0_CNTR1) + ch);
#elif defined(HAS_KINETIS_TSI_LITE)
    TSI0_GENCS = TSI_GENCS_REFCHRG(4) | TSI_GENCS_EXTCHRG(3) | TSI_GENCS_PS(PRESCALE)
    | TSI_GENCS_NSCN(NSCAN) | TSI_GENCS_TSIEN | TSI_GENCS_EOSF;
    TSI0_DATA = TSI_DATA_TSICH(ch) | TSI_DATA_SWTS;
    delayMicroseconds(10);
    while ((TSI0_GENCS & TSI_GENCS_SCNIP) && (timeNow < targetTime)) {   //   wait
        if (useMax) timeNow = micros();
    }
    delayMicroseconds(1);
    if (useMax && (timeNow >= targetTime))  return (-1);
    else                                    return TSI0_DATA & 0xFFFF;
#endif
}

void touchablePin::init(void) {
    unsigned long timeBefore = micros();
    untouchedValue = touchReadWithMax(pinNumber, false);
    unsigned long timeAfter = micros();
    untouchedTime = timeAfter - timeBefore;
}

void touchablePin::initUntouched(void) {
    //    first call to touchReadWithMax() returns too quickly TODO: find out why.
    //    work-around - call it twice... second calling sets appropriate values
    if (pinNumber != -1) {
        init();
        init();
    }
}

void touchablePin::setPin(uint8_t pin) {
    pinNumber = pin;
    initUntouched();
}

int touchablePin::isTouched_orVal(void) {
    int readval = 0;
    int touchCount = 0;
    for (int x = 0; x < _numSamples; x++) {
        readval = touchReadWithMax(pinNumber, true);
        if (readval == -1) {
            touchCount++;
            if (touchCount >= (float)(_numSamples/2))  return(-1);
                // return true if at leasthalf of the attempts say "touched"
        }
    }
    return(readval);
}

bool touchablePin::isTouched(void) {
    int touchCount = 0;
    for (int x = 0; x < _numSamples; x++) {
        if (touchReadWithMax(pinNumber, true) == -1) {
            touchCount++;
            if (touchCount >= (float)(_numSamples/2))  return(true);
                // return true if at leasthalf of the attempts say "touched"
        }
    }
    return(false);
}

int touchablePin::touchRead(void) {
    if (pinNumber != -1) {
        return(touchReadWithMax(pinNumber, false));
    } else {
        return(0);
    }
}

#endif // HAS_KINETIS_TSI) || defined(HAS_KINETIS_TSI_LITE
       // NOTE: The final #else condition in touchRead.c has been eliminated here.
       //       In that #else case, I expect this code will throw compile time errors.


