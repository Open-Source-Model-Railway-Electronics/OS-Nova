/*
 * Copyright (C) 2024 Sebastiaan Knippels, Train-Science
 *
 * To the extent possible under law, the person who associated CC0 with this work
 * has waived all copyright and related or neighboring rights to this work.
 *
 * This work is published from: The Netherlands
 *
 * You can copy, modify, distribute and perform the work, even for commercial purposes,
 * all without asking permission. See the full license for details at:
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

#include <Arduino.h>
#include "macros.h"


#ifndef DEBUG
#define State(x) break; case x: if(x##F())
#else   
#define State(x) break; case x: if(sm.runOnce) Serial.println(#x); if(x##F())   // if debug is defined, all states are stringerized and printed when entry state is run
#endif
#define STATE_MACHINE_BEGIN(x) if( x.run() ) switch( x.getState() ) {
#define STATE_MACHINE_END(x) break ; } return x.getState() ;

class StateMachine {
public:
    StateMachine() ;
    
    void  setState( uint8_t ) ;
    uint8 getState() ;
    void  nextState( uint8_t, uint32_t ) ;
    uint8 entryState() ;
    uint8 onState() ;
    uint8 exitState() ;
    uint8 run() ;
    void  setTimeout( uint32_t ) ;
    void  setTimeout_us( uint32_t ) ;
    uint8 timeout() ;
    uint8 timeout_us() ;
    uint8 timeoutError() ;

    void  exit() ;
    void  reboot( uint32_t ) ;
    uint8 endState() ;
    uint8 repeat( uint32_t ) ;
    uint8 repeat_us( uint32_t ) ;
    #ifdef DEBUG
    uint8  runOnce ;  // if debug is active, this must be public in order to print the state names
    #endif
    
private:
    #ifndef DEBUG
    uint8  runOnce ;  // is usually private
    #endif
    uint8  enabled ;
    uint8  exitFlag ;
    uint32_t prevTime ;
    uint32_t interval ;
    uint8  state;
    uint8  timeOutSet ;
} ;


