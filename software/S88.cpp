#include "S88.h"

S88::S88( uint8_t _CLOCK_PIN, uint8_t _DATA_PIN, uint8_t _LOAD_PIN, uint8_t _RESET_PIN )
{
    CLOCK_PIN   = _CLOCK_PIN ;  
    DATA_PIN    = _DATA_PIN ;  
    LOAD_PIN    = _LOAD_PIN ;  
    RESET_PIN   = _RESET_PIN ;  
}

void S88::setModules( uint8_t _nModules ) 
{
    nModules = _nModules ;
}

enum states
{
    S88_IDLE,
    S88_RESET,
    S88_CLOCK_IN,
    S88_DEBOUNCE,
} ;


#define StateFunction( x ) bool S88::x##F()

StateFunction( S88_IDLE )
{
    if( sm.entryState() )
    {
        digitalWrite( LOAD_PIN, HIGH ) ;
        sm.setTimeout_us( 10 ) ;
    }

    if( sm.onState() )
    {
        if( sm.timeout_us() )
        {
            sm.exit() ;
        }
    }

    if( sm.exitState() )
    {
        digitalWrite( LOAD_PIN, LOW ) ;
    }

    return sm.endState() ;
}



StateFunction( S88_CLOCK_IN )
{
    if( sm.entryState() )
    {
        moduleCounter = 0 ;
        bitCounter    = 0 ;

        for( int i = 0 ; i < maxModules ; i ++ )
        {
            newSamples[i] = 0 ; // while all old menu's
        }
    }

    if( sm.onState() )
    {
        if( sm.repeat_us( 30 ) )                                                             // REPEATS every 5us
        {
            digitalWrite( CLOCK_PIN, !digitalRead( CLOCK_PIN) ) ;                           // toggle the clock, clock is low the first run

            if( digitalRead( CLOCK_PIN ) )                                                  // if the clock is HIGH, we gonna read data  
            {
                newSamples[ moduleCounter ] |= ( digitalRead( DATA_PIN ) << bitCounter ) ;

                if( ++bitCounter == nBitsPerModule )
                {    bitCounter   = 0 ;

                    if( ++ moduleCounter == nModules )
                    {
                        sm.exit() ;
                    }
                }
            }
        }
    }

    if( sm.exitState() )
    {
        lastIteration = millis() ; // store timestamp in MILLISECONDS
        digitalWrite( CLOCK_PIN, LOW ) ;
    }

    return sm.endState() ;
}

// debounce and process all captured data!
StateFunction( S88_DEBOUNCE )
{
    for( uint8_t module = 0 ; module < nModules ; module ++ )
    {
        for( uint8_t bit = 0 ; bit < nBitsPerModule ; bit ++ )
        {
            uint8_t oldState  = bitRead( oldStates [module], bit ) ;
            uint8_t newSample = bitRead( newSamples[module], bit ) ;
            uint8_t oldSample = bitRead( oldSamples[module], bit ) ;

            if( newSample == oldSample )
            {
                if( newSample != oldState )
                {
                    if( newSample )  oldStates[module] |=  (1UL << bit) ; // update oldStates array with newSample
                    else             oldStates[module] &= ~(1UL << bit) ;

                    uint16_t address = (uint16_t)module * nBitsPerModule + bit ;

                    if( notifyS88sensor ) notifyS88sensor( address, newSample ) ;
                }
            }
        }

        oldSamples[module] = newSamples[module] ; // update old samples with newSamples
    }

    return 1 ;
}



// operate the reset line for 30us and wait for the next cycle to begin
StateFunction( S88_RESET )
{
    if( sm.entryState() )
    {
        digitalWrite( RESET_PIN, HIGH ) ;
        sm.setTimeout_us( 30 ) ; 
    }

    if( sm.onState() )
    {
        if( sm.timeout_us() )   // time has passed
        {
            digitalWrite( RESET_PIN, LOW ) ;   // after timeout, kill the reset pin
        }

        if( millis() - lastIteration >= pollInterval ) sm.exit() ;  // wait untill it is time
    }

    return sm.endState() ;
}


uint8_t S88::update()
{
    STATE_MACHINE_BEGIN( sm )
    {
        State( S88_IDLE ) {                     // wait for time to start a new cycle.
            sm.nextState( S88_CLOCK_IN, 0 ) ; }

        State( S88_CLOCK_IN ) {
            sm.nextState( S88_DEBOUNCE, 0 ) ; }

        State( S88_DEBOUNCE ) {
            sm.nextState( S88_RESET, 0 ) ; }


        State( S88_RESET ) {
            sm.nextState( S88_IDLE, 0 ) ; }

    }
    STATE_MACHINE_END( sm )
}