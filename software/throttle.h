#include <Arduino.h>
#include "src/ST.h"

/* Desscription, reusable throttle library to work with analog systems
    uses current sources from 4-20mA generating 1-5V 

    there are several modes to do with a varrying amount of potentiometers.

    With just 1 potentiometer, we can do little. Just speed. So the middle positon is speed 0, left and right is speed -127 to speed 127
    
    With 2 potentiometers, we can do 2 things
       speed can be done using 2, so either throttle/brake or  speed/acceleration
       or speed is absolute like with 1, and 2nd potentiometer is replaced for up x function buttons

    
    With 3 potentiometers, use double speed mode, mandatory and we use buttons for functions 

    So speedmodes are defined as 
        mode            // n pots
    - speed_accel        1, 2 or 3
    - throttle_brake     3

    speed_accel  AND 1 pot   -> just speed control with neutral middle 
    speed_accel  AND 2 pots  -> speed control with neutral middle and function buttons
    speed_accel  AND 3 pots  -> speed acceleration control with direction switch
    
    with just 1 pot, accel is default to max

    The function buttons: DIRECTION, F0,  F1 <> F8 (10 buttons)

*/
const int REVERSE  =    1 ; 
const int FORWARD  =    0 ; 
const int maxAccel =   23 ; //  3 second for  fastest acceleration
const int minAccel =   79 ; // 10 seconds for slowest acceleration
const int adcMin   =  204 ; // 1V ADC count
const int adcMax   = 1023 ; // 5V ADC count
const int noADC    =   10 ; // disconnected, discard throttle.
const int marge    =   50 ; // dead range
const int minSpeed =    0 ;
const int maxSpeed =  127 ;

const int throttle_eeAddress = 0x7000 ;  // 4096 bytes reserved

enum speedModes
{
    speed_accel,
    throttle_brake,
} ;



class Throttle
{
public:
    void begin( uint8_t pot1, uint8_t pot2, uint8_t pot3, uint8_t _buttons ) ; 
    void update() ;

private:
    uint8_t     mySlot ; 

    uint8_t     speedMode       : 1 ;
    uint8_t     hasFunctions    : 1 ;
    uint8_t     neutralMiddle   : 1 ;
    uint32_t    acceleration ;
    uint32_t    prevTime ;
    uint32_t    prevButtonTime ;
    uint8_t     accelerating    : 1 ;
    uint16_t    functions       : 9 ; //F0 - F8
    uint8_t     deccelerating   : 1 ;
    uint8_t     mode ;
    uint8_t     pot1 ;
    uint8_t     pot2 ;
    uint8_t     pot3 ;
    uint8_t     speed           : 7 ; // 0 - 127
    uint8_t     oldSpeed        : 7 ; // 0 - 127
    uint8_t     dir             : 1 ; // 1
    uint8_t     oldDir          : 1 ; // 1
    uint8_t     nPots           : 2 ; // 1-3
    uint16_t    address ;       // current address, comes from EEPROM
    uint8_t     index ;

    uint8_t     state ;

    Timer       timer ;
    Timer       funcTimer ;

    Trigger     trLong ;
    Trigger     trShort ;

    void        throttleBrakeControl() ;
    void        processFunctions() ;
    void        singlePotControl() ;
    void        acquireAddress( uint16_t acquireAddress) ;
} ;

extern void notifyThrottleFunction( uint8_t slot, uint8_t    Fn, uint8_t state ) __attribute__((weak)); // sets one function at the time
extern void notifyThrottleSpeed(    uint8_t slot, uint8_t speed, uint8_t   dir ) __attribute__((weak));
extern void notifySlotChange( uint8_t slot )                __attribute__((weak));