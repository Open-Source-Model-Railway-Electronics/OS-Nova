#include <Arduino.h>
#include "src/stateMachineClass.h"

const uint8_t   maxModules      = 31 ;
const uint32_t  pollInterval    = 50 ;
const uint8_t   nBitsPerModule  = 16 ;

class S88
{
public:
    S88( uint8_t, uint8_t, uint8_t, uint8_t ) ;
    
    uint8_t         update() ;
    void            setModules( uint8_t ) ;

private:
    StateMachine sm ;

    uint8_t         CLOCK_PIN ;
    uint8_t         DATA_PIN ;
    uint8_t         LOAD_PIN ;
    uint8_t         RESET_PIN ;

    uint8_t         nModules ;
    uint8_t         bitCounter ;
    uint8_t         moduleCounter ;

    uint16_t        oldStates [maxModules] ;
    uint16_t        newSamples[maxModules] ;
    uint16_t        oldSamples[maxModules] ;

    uint32_t        lastIteration ;

    bool            S88_IDLEF() ;
    bool            S88_RESETF() ;
    bool            S88_CLOCK_INF() ;
    bool            S88_DEBOUNCEF() ;

} ;

extern void notifyS88sensor( uint16_t, uint8_t )__attribute__((weak));