#include "throttle.h"
#include "slot.h"


void Throttle::begin( uint8_t _pot1, uint8_t _pot2, uint8_t _pot3, uint8_t _mode ) 
{
    if( _pot1 != 0xFF ){ nPots ++ ; pot1 = _pot1 ; }
    if( _pot2 != 0xFF ){ nPots ++ ; pot2 = _pot2 ; }
    if( _pot3 != 0xFF ){ nPots ++ ; pot3 = _pot3 ; }

    if( _mode == speed_accel && nPots == 1 ) { neutralMiddle = 1 ; hasFunctions = 0 ; }
    if( _mode == speed_accel && nPots == 2 ) { neutralMiddle = 1 ; hasFunctions = 1 ; }
    if((_mode == speed_accel && nPots == 3 )
    || _mode == throttle_brake )
    {
        neutralMiddle = 0 ; 
        hasFunctions = 1 ; 
        hasFunctions = 1 ;
    }

    timer.set( TIMER_BLEEP, 50 ) ;
} 

void Throttle::throttleBrakeControl() 
{
    uint16_t sample1 = analogRead( pot1 ) ;
    if( sample1 < noADC ) return ;

    uint16_t    sample2 = analogRead( pot2 ) ; // brake, sample 1 is throttle
    if( sample2 > adcMin + marge )  // in case when braking is active, throttle is entirely discarded. Protects trains from humans
    {
        acceleration = map( sample2, adcMin + marge, adcMax, minAccel, maxAccel ) ; // negative acceleration using brake pot
        accelerating  = 0 ; deccelerating = 1 ;
    }
    else if( sample1 > adcMin + marge )  
    {
        acceleration = map( sample1, adcMin + marge, adcMax, minAccel, maxAccel ) ; // positive acceleration using throttle pot
        accelerating  = 1 ; deccelerating = 0 ;
    }
    else
    {
        acceleration  = 20 ; // just a random low value
        accelerating  = 0 ; deccelerating = 0 ;
    }

    if(  accelerating && speed < maxSpeed ) speed ++ ; // setPoint is not really needed, we can just inc or dec speed and perform callback
    if( deccelerating && speed > minSpeed ) speed -- ; 

    // if speed != oldSpeed || dir := 
    //notifyThrottleSpeed( address[index] , dir, speed ) ;
}

void Throttle::singlePotControl() 
{
    if( hasFunctions == 0 ) // meaning no direction switch is present
    {
        uint16_t sample1 = analogRead( pot1 ) ;
        if( sample1 < noADC ) return ;

        uint16_t  lowLimit = (adcMin + adcMax) / 2 - marge ;
        uint16_t highLimit = (adcMin + adcMax) / 2 + marge ;

        if( sample1 < lowLimit )
        {
            dir     = REVERSE ;
            speed   = map( sample1, lowLimit, adcMin, 0, 127 ) ;
        }
        if( sample1 > highLimit )
        {
            dir     = FORWARD ;
            speed   = map( sample1, highLimit, adcMax, 0, 127 ) ;
        }
        else
        {
            speed  = 0 ;
        }

        //notifyThrottle( address[index] , dir, speed ) ; // todo, implement speed change before callbacking

        if( pot2 == 0xFF )                                // 2nd pot is not in use
        {
            acceleration = maxAccel ;
            return ;
        }

        uint16_t sample2 = analogRead( pot2 ) ; 
        if( sample2 < adcMin + marge )
        {
            acceleration = minAccel ;
            return ;
        }

        acceleration = map( sample2, adcMin + marge, adcMax, minAccel, maxAccel ) ; 
    }
}

void Throttle::processFunctions() 
{
    if( millis() - prevButtonTime > 20 ) 
    {     prevButtonTime = millis() ;
        
        uint16_t sample3 = analogRead( pot3 ) ;
        uint8_t newState = 0 ;

        if( sample3 < 100                            )  newState =  1 ;// disconnected
        if( sample3 > 205 - 20 && sample3 < 205 + 20 )  newState =  2 ;// nothing pressed
        if( sample3 > 635 - 20 && sample3 < 635 + 20 )  newState =  3 ;// DIR
        if( sample3 > 659 - 20 && sample3 < 659 + 20 )  newState =  4 ;// F0
        if( sample3 > 686 - 20 && sample3 < 686 + 20 )  newState =  5 ;// F1
        if( sample3 > 716 - 20 && sample3 < 716 + 20 )  newState =  6 ;// F2
        if( sample3 > 750 - 20 && sample3 < 750 + 20 )  newState =  7 ;// F3
        if( sample3 > 789 - 20 && sample3 < 789 + 20 )  newState =  8 ;// F4
        if( sample3 > 834 - 20 && sample3 < 834 + 20 )  newState =  9 ;// F5
        if( sample3 > 887 - 20 && sample3 < 887 + 20 )  newState = 10 ;// F6
        if( sample3 > 949 - 20 && sample3 < 949 + 20 )  newState = 11 ;// F7
        if( sample3 > 1000                           )  newState = 12 ;// F8
        
        trShort.update( funcTimer.ET >= 20  &&  funcTimer.ET <= 1500 && newState == 2 ) ; // if timer's elapsed time is 20 <= ET <= 1500, and no button is press -> short time
        trLong.update(  funcTimer.ET >= 1500 ) ;

        if( newState > 2 )  { funcTimer.update( 1 ) ; } // if anything is pressed, run the timer
        else                { funcTimer.update( 0 ) ; }

        if( trShort.rose() )
        {
            if( newState == 3 ) { dir ^= 1 ; notifyThrottleSpeed( mySlot, slot[mySlot].speed, dir ) ; }   // direction change? special notify ?
            if( newState >= 4 )
            {
                uint8_t state ; // TODO, need a fix
                notifyThrottleFunction( address, newState-4, state ) ;  // request a toggle for this function
            }
        }
        if(  trLong.rose() ) // long used to set an address.
        {
            if( newState >= 3 ) { 
                index = newState - 3 ;
                
                slot[mySlot].stat1 = SLOT_IDLE ;        // slot of my old train is put in SLOT_IDLE
                notifySlotChange( mySlot ) ;

                mySlot = getSlot( address ) ;    // allocate a new slot for my other address, commision a new slot if needed
                slot[mySlot].stat1 = SLOT_IN_USE ;      // in case a slot used to be IDLE
                notifySlotChange( mySlot ) ;
            }
        }
    }
}

void Throttle::update()
{
    if( timer.update() ) 
    {        prevTime = millis() ;
     
        if( mode == throttle_brake ) throttleBrakeControl() ;
        else                         singlePotControl() ;
    }
    if( hasFunctions )               processFunctions() ;
} 


// void Throttle::dispatchAddress( uint16_t newAddress )
// {

// }

// void Throttle::acquireAddress( uint16_t newAddress )
// {
//     address[index] =  newAddress ;
//     // I need to do a slot move
//     notifySlotMove( 
// }