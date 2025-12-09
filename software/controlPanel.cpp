#include "controlPanel.h"

// note brainwave about timing: I was thinking to let the S88 code and this code share a flag. The S88 already has timing built in, we could let it allow this code to run an update cycle

/* TODO
 fix I2C EEPROM memory
 button memory 128 buttons -> 2 bytes per item -> 256 bytes 

 routes: 2 bytes elk
 pointsPerRoute = 12 ; 24 bytes per route
 max routes.. undefined, groot EEPROM you know.

*/

void ControlPanel::begin( uint8_t clockPin, uint8_t latchPin, uint8_t dataInPin, uint8_t dataOutPin )
{
    latch   = latchPin ;
    clock   = clockPin ;
    outNext = dataOutPin ;
    inNext  = dataInPin ;

    timer.set( TIMER_ON, 20 ) ;
    for( int i = 0 ; i < maxIO ; i ++ )
    {
        IO[i].inPrev = 0 ;
    }
}

void ControlPanel::allowUpdate()
{
    updateAllowed = true ;
}

void ControlPanel::update()
{
    if( !updateAllowed ) return ;
    updateAllowed = false ;
    
    digitalWrite( latch, LOW ) ;            // update outputs first, this will operate the latch line which is needed for the shift in registers. As soon as we release, we can continue with clocking in the inputs
    for (int i = 0; i < nModules; i++)
    {
        uint8_t output = 0;

        for( int j = 0 ; j < 8 ; j++ )
        {
            uint8_t idx = j + i*8 ;
            output |= ((IO[idx].state ^ IO[idx].invertedLed) << j) ;    // prepare byte to send
        }
        shiftOut( outNext, clock, MSBFIRST, output ) ;      // Note may become LSB first
    }
    digitalWrite( latch, HIGH ) ;   // releasing the latch will allow us to read imputs during the next iteration
    

    for (int i = 0; i < nModules; i++)
    {
        uint8_t input = shiftIn( inNext, clock, MSBFIRST ) ; // Note may become LSB first

        for( int j = 0 ; j < 8 ; j++ )
        {
            uint8_t idx       = j + i*8 ;
            uint8_t newState  = ((input >> j) & 1) ;

            if( newState       != IO[idx].inPrev )
            {   IO[idx].inPrev  = newState ;
                IO[idx].state   = newState ; // update the corresponding LED

                if( IO[idx].type == AccessoryBtn ) { notifyPanelSetAccessory( IO[idx].address, IO[idx].state ) ; }  
                if( IO[idx].type == OccupancyBtn ) { notifyPanelSetOccupancy( IO[idx].address, IO[idx].state ) ; }
                if( IO[idx].type == RouteBtn     ) { sendRouteButton(                     idx,      newState ) ; }
            }
        }
    }
}

// external process can send information about occupancy, we can use this to set our LED correctly
void ControlPanel::setOccupancy( uint16_t address, uint8_t state )
{
    for( int i = 0 ; i < maxIO ; i ++ ) 
    {
        if( IO[i].type == OccupancyBtn && address == IO[i].address )
        {
            IO[i].state = state ;
        }
    }
}

void ControlPanel::setAccessory( uint16_t address, uint8_t state )
{
    for( int i = 0 ; i < maxIO ; i ++ ) 
    {
        if( IO[i].type == AccessoryBtn && address == IO[i].address )
        {
            IO[i].state = state ;
        }
    }
}

/*
void ControlPanel::processIO()
{
    for( int j = 0 ; j < maxIO ; j++ )
    {
        if( IO[j].in     != IO[j].inPrev )
        {   IO[j].inPrev  = IO[j].in ;
            IO[j].state   = IO[j].in ;   // update the corresponding LED

            if( IO[j].isAccessory ) (IO[j].address, IO[j].state ) ;
            if( IO[j].isOccupancy )  ; // todo callback to occupancy function that does not yet exist
            if( IO[j].isRouteBtn  )  ; // todo, send button to route manager, not yet implemented
        }
    }
}*/

/* TODO

    figure out first and second button.


*/

void ControlPanel::sendRouteButton( uint8_t ID, uint8_t state )
{
}

void ControlPanel::routeManager() // find the route buttons and fetch the correct array from i2c eeprom.
{

}
