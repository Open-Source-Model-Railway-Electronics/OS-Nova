#include "throttle.h"
#include "slot.h"

/* TODO 

    * rebuild this code to remove all analog signals all togather. Master plan is to provide the 16 bit data via extern methods.
    * We will use things like throttleLevel, brakeLevel and 

*/
#define DIR 0
#define F0  1 
#define F1  2 
#define F2  3 
#define F3  4 
#define F4  5 
#define F5  6 
#define F6  7 
#define F7  8 
#define F8  9 

void Throttle::begin( uint8_t _pot1,
                      uint8_t _pot2,
                      uint8_t _pot3,
                      uint8_t _isDigital,
                      uint8_t _hasBrake,
                      uint8_t _hasFunctions )
{
    pot1  = _pot1 ;
    pot2  = _pot2 ;
    pot3  = _pot3 ;

    hasBrake     = _hasBrake ;
    hasFunctions = _hasFunctions ;
    isDigital    = _isDigital ;

    if( isDigital ) { hasFunctions = hasBrake = 1 ; } // prevents illegitimate settings

    speed        = 0 ;
    oldSpeed     = 0 ;
    dir          = FORWARD ;
    oldDir       = FORWARD ;

    funcTimer.set( TIMER_ON, 10000 ) ; // just a high enough limit, completely random
    timer.set( TIMER_BLEEP, 50 ) ;
}

const uint32_t  sampleInterval = 20 ;
const int MARGE     =   30 ;
const int ONE_BIT   =  150 ;
const int ZERO_BIT  =  250 ;
const int START_BIT = 1000 ;

void Throttle::processDigitalSignal()
{
    uint32_t newMicros = micros() ;
    if( newMicros - lastMicros > sampleInterval )
    {    
        uint8_t newSample = digitalRead( pot1 ) ;
        if( newSample != oldSample )
        {
            uint32_t intervalTime = newMicros - lastSampleTime ;
            lastSampleTime = newMicros ;
            if( intervalTime >= START_BIT-MARGE && receiving == 0 )
            {
                receivedBits.raw = 0 ;
                bitCounter       = 0 ; 
                receiving        = 1 ;
            }
            else if( receiving == 1 && intervalTime >  (ONE_BIT-MARGE) && intervalTime < (ONE_BIT+MARGE))
            {
                receivedBits.raw |= ((uint32_t)1 << bitCounter) ;
                bitCounter ++ ;
            }
            else if( receiving == 1 &&  intervalTime > (ZERO_BIT-MARGE) && intervalTime <(ZERO_BIT+MARGE))
            {
                bitCounter ++ ;
            }
            else // neither start, one or zero
            {
                receiving = false ;
            }

            if( bitCounter == 32 && receiving == 1 )  // increment counter and check if frame is received
            {
                currentBits = receivedBits ;
                receiving   = 0 ;
            }
        }

        lastMicros = newMicros ;
        oldSample = newSample ;
    }
}


static uint16_t decodeFunctionsFromLadder( uint16_t sample3 )
{
    if( sample3 > 635 - 20 && sample3 < 635 + 20 ) return (1u << 0) ; // DIR
    if( sample3 > 659 - 20 && sample3 < 659 + 20 ) return (1u << 1) ; // F0  // DISPATCH GET
    if( sample3 > 686 - 20 && sample3 < 686 + 20 ) return (1u << 2) ; // F1  // SELECT ADDRESS 1
    if( sample3 > 716 - 20 && sample3 < 716 + 20 ) return (1u << 3) ; // F2
    if( sample3 > 750 - 20 && sample3 < 750 + 20 ) return (1u << 4) ; // F3
    if( sample3 > 789 - 20 && sample3 < 789 + 20 ) return (1u << 5) ; // F4
    if( sample3 > 834 - 20 && sample3 < 834 + 20 ) return (1u << 6) ; // F5
    if( sample3 > 887 - 20 && sample3 < 887 + 20 ) return (1u << 7) ; // F6
    if( sample3 > 949 - 20 && sample3 < 949 + 20 ) return (1u << 8) ; // F7
    if( sample3 > 1000                           ) return (1u << 9) ; // F8  // SELECT ADDRESS 8
    return 0 ;
}


#define lastMillis lastMicros

void Throttle::processAnalogSignal()
{
    uint32_t newMillis = millis() ;
    if( newMillis - lastMillis > sampleInterval )
    {           lastMillis = newMillis ;

        // Defaults
        currentBits.f.throttleLevel = 0 ;
        currentBits.f.brakeLevel    = 0 ;
        currentBits.f.functions     = 0 ;

        uint16_t a1 = analogRead( pot1 ) ;
        if( a1 < noADC ) return ; // throttle disconnected → ignore update. This condition always is enough to check whether the throttle is attached or not

        // Scenario 1:
        // hasBrake = 1 and hasFunctions = 1  => 3 pots: throttle, brake, functions
        if( hasBrake && hasFunctions )
        {
            uint16_t a2 = analogRead( pot2 ) ;
            uint16_t a3 = analogRead( pot3 ) ;

            const uint16_t lowLimit = adcMin + marge ;

            if( a1 >= lowLimit )                  currentBits.f.throttleLevel = map( a1, lowLimit, adcMax, 0, 127 ) ;
            if( a1 >= adcMin && a1 < lowLimit )   currentBits.f.throttleLevel = 0 ;

            if( a2 >= lowLimit )                  currentBits.f.brakeLevel    = map( a2, lowLimit, adcMax, 0, 127 ) ;
            if( a2 >= adcMin && a2 < lowLimit )   currentBits.f.brakeLevel    = 0 ;

            if( a3 >= noADC )                     currentBits.f.functions  = decodeFunctionsFromLadder( a3 ) ;

            return ;
        }

        // Scenario 2:
        // hasBrake = 0 and hasFunctions = 0 => 1 pot speed with neutral middle, no functions
        if( !hasBrake && !hasFunctions )
        {
            const uint16_t mid       = (adcMin + adcMax) / 2 ;
            const uint16_t lowLimit  = mid - marge ;
            const uint16_t highLimit = mid + marge ;

            if(      a1 <  lowLimit ) { currentBits.f.throttleLevel = map( a1,  lowLimit, adcMin, 0, 127 ) ; dir = REVERSE ; }
            else if( a1 > highLimit ) { currentBits.f.throttleLevel = map( a1, highLimit, adcMax, 0, 127 ) ; dir = FORWARD ; }
            else                      { currentBits.f.throttleLevel = 0 ; }
            return ;
        }

        // Scenario 3:
        // hasBrake = 0 and hasFunctions = 1 => 1 pot speed without middle + functions
        // (no brake pot)
        if( !hasBrake && hasFunctions )
        {
            const uint16_t lowLimit  = adcMin + marge ;
            if( a1 >= lowLimit )    currentBits.f.throttleLevel = map( a1, lowLimit, adcMax, 0, 127 ) ;
            else                    currentBits.f.throttleLevel = 0 ;

            uint16_t a3 = analogRead( pot3 ) ;
            if( a3 >= noADC ) currentBits.f.functions = decodeFunctionsFromLadder( a3 ) ;
        }

        speed = constrain( speed, 0, 127 ) ;
    }
}

/*
 single POT NO function control ->  middlePosition                          targetSpeed comes from   currentBits.f.throttleLevel
 single POT + function  -> no middle positon, functions toggle direciton    targetSpeed comes from   currentBits.f.throttleLevel
 double POT + function  ->  separate throttle and brake                     speed is ramping

 Note minimum potentiometer marges are processed in above function ^ ^
*/

void Throttle::updateSpeed() 
{
    uint8_t throttle = currentBits.f.throttleLevel ;
    uint8_t    brake = currentBits.f.brakeLevel ;

    if( hasBrake )
    {
        uint8_t desiredSpeed = 0 ;
        uint8_t delta        = 0 ;
        uint8_t drag         = 0 ;

        /* --- throttle -> desired cruising speed --- */
        if( throttle > 0 )
        {
            // Linear mapping: throttle -> speed
            desiredSpeed = ( (uint16_t)throttle * maxSpeed ) >> 8 ;

            // Minimum cruise speed = 5% of maxSpeed
            uint8_t minCruise = maxSpeed / 20 ;
            if( desiredSpeed < minCruise )
                desiredSpeed = minCruise ;
        }

        /* --- brake overrides throttle --- */
        if( brake > 0 )
        {
            desiredSpeed = 0 ;
        }

        /* --- determine acceleration direction --- */
        if( speed < desiredSpeed )
        {
            accelerating  = 1 ;
            deccelerating = 0 ;
            delta = desiredSpeed - speed ;
        }
        else if( speed > desiredSpeed )
        {
            accelerating  = 0 ;
            deccelerating = 1 ;
            delta = speed - desiredSpeed ;
        }
        else
        {
            accelerating  = 0 ;
            deccelerating = 0 ;
            delta = 0 ;
        }

        /* --- dynamic interval based on difference --- */
        timer.PT = maxAccel - ( delta >> 1 ) ;
        if( timer.PT < minAccel )
            timer.PT = minAccel ;

        /* --- speed dependent air resistance when coasting --- */
        if( throttle == 0 && brake == 0 && speed > 0 )
        {
            if(      speed > ( maxSpeed /  2 ) ) drag = 2 ;
            else if( speed > ( maxSpeed / 10 ) ) drag = 1 ;
            else                                 drag = 0 ;
        }

        /* --- apply speed change --- */
        if( accelerating && speed < maxSpeed )
        {
            speed ++ ;
        }

        if( deccelerating && speed > minSpeed )
        {
            speed -- ;

            if( drag && speed > minSpeed ) speed -- ;
        }
    }

    else if( hasFunctions == 0 ) //  middle pos -> No functions
    {
        const uint16_t  lowLimit = maxSpeed / 2 - 5 ;
        const uint16_t highLimit = maxSpeed / 2 + 5 ;

        timer.PT = maxAccel ;

        if(     throttle <  lowLimit )  {
                                            dir     = REVERSE ;
                                            speed   = map( throttle, lowLimit, 0 , 0, 127 ) ;
                                        }
        else if( throttle > highLimit ) {
                                            dir     = FORWARD ;
                                            speed   = map( throttle, highLimit, maxSpeed, 0, 127 ) ;
                                        }
        else                            {
                                            speed  = 0 ;
                                        }
    }
    else    // single potentiometer speed control but with function
    {
        speed    = throttle ;
        timer.PT = maxAccel ;
    }

    if( speed != oldSpeed || dir != oldDir ) notifyThrottleSpeed( mySlot, speed, dir ) ;
    oldDir   =   dir ;
    oldSpeed = speed ;
}


void Throttle::processFunctions()
{
    if( millis() - prevButtonTime < 20 ) return ;
    prevButtonTime = millis() ;

    uint16_t fnBits = currentBits.f.functions ;
    // Determine which function is currently active (one-hot expected)
    uint8_t newActive = 0xFF ;
    for( uint8_t i = 0 ; i < 9 ; i++ )
    {
        if( fnBits & (1u << i) )
        {
            newActive = i ;
            break ;
        }
    }

    // If timer elapsed time is between 20 and 1500ms and the button is longer pressed. we have a short press
    trShort.update(
        funcTimer.ET    >=   20
    &&  funcTimer.ET    <= 1500
    &&  newActive       == 0xFF  
    &&  activeFunction  != 0xFF
    ) ;
    
    /* -------- SHORT PRESS ACTIONS -------- */
    if( trShort.rose() )
    {
        if( activeFunction == DIR )   // DIR (example: activeFunction == 9 if you reserve it)
        {
            dir ^= 1 ;
            // notifyThrottleSpeed( mySlot, slot[mySlot].speed, dir ) ; THis is performed by upadate speed, therfor toggling dir is enough
        }
        // F0..F8
        else if( activeFunction >= F0 && activeFunction <= F8 )
        {
            uint8_t state = 1 ; // toggle request
            notifyThrottleFunction( mySlot, activeFunction, state ) ;
        }
    }
    
    trLong.update( funcTimer.ET >= 1500 ) ;
    // if this is true, the button must still be pressed, so we have a long press
    // long press is used to change address. DIR button is a DISPATCH PUT command, F0 is a DISPATCH GET command.
    // F1 - F8 is new address selection.

    /* -------- LONG PRESS ACTIONS -------- */
    if( trLong.rose() )
    {
        if(      activeFunction == DIR ) ; // dispatch PUT
        else if( activeFunction ==  F0 ) ; // dispatch GET 
        else                          // F1-F8 are used to swap addresses internally
        {
            index = activeFunction - F1; // ensures that index is between 0 and 7

            slot[mySlot].stat1 = SLOT_IDLE ; // put my current slot in IDLE
            notifySlotChange( mySlot ) ;

            mySlot = getSlot( address ) ;   // commision a new slot, this will set the state to IN_USE
            notifySlotChange( mySlot ) ;
        }
    }

    funcTimer.update( newActive != 0xFF ) ; // if any button is pressed

    activeFunction = newActive ;
}


void Throttle::update()
{
    if( isDigital ) processDigitalSignal() ;
    else            processAnalogSignal() ;

    if( timer.update() ) 
    {
        updateSpeed() ;
    }
    
    if( hasFunctions )
    {
        processFunctions() ;
    }
} 


 // TODO intent is to flag our current address as dispatch address
// void Throttle::dispatchAddress( uint8_t slot )
// {

// }


/* acuiring an address

    somewhat tricky, the throttle has no feedback because it is dumb
    the command center however knows when a throttle calls this method

    therefor the command center can assign an address to my current slot.
    I may also want to introduce dispatch mode only in which a throttle can only obtain an address via dispatching
    So I eliminate the F1-F8 options to change address. But this really may only be fun for our digital throttles.
*/
// void Throttle::acquireAddress(  ) // TODO, issue 
// {
//     address[index] =  newAddress ;
//     // I need to do a slot move
//     notifySlotMove( 
// }