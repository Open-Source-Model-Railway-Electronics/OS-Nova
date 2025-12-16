#include <Arduino.h>
#include "src/ST.h"

#ifndef CONTROL_PANEL_H
#define CONTROL_PANEL_H

/*
Meesterplan:   
Alles gaat per 8... behalve terugmelders want die gaan per 16..  
Anyways. Het zou kunnen dat een decoder niet gevuld is 100%. Dat lossen we op door een knopje gewoon weg niet te gebruiken
Maar aan de andere kant, zit je met route knopjes, als je er nu net 1 mist... is jammer. 



*/

const int route_eeAddress   = 512 ;
const int maxPanelModules   = 16  ;  // 32 times 8 makes 256 buttons,
const int switchesPerModule = 8 ;
const int maxIO             = maxPanelModules * switchesPerModule ;
const int pointsPerRoute    = 12 ;
const int nRoutes           = 52 ; // TO DETERMEN, 2 is plain random

enum
{
    AccessoryBtn ,
    OccupancyBtn ,
    RouteBtn     ,
} ;

const int noButton = 0xFF ;


typedef struct
{
    uint8_t first ;
    uint8_t second ;
} ButtonPair ;

typedef struct Routes 
{
    ButtonPair button ;

    struct 
    {
        uint16_t address : 14 ;
        uint16_t state   :  1 ;
        uint16_t inUse   :  1 ;
    } point [pointsPerRoute] ;
} Route; 

// usage: route.button.first
//        route.button.second
//        route.point[i].address
//        route.point[i].state

// Route route[pointsPerRoute] ;  // routes are not yet implemented. Will be stored in I2C EEPROM.

typedef struct IOthings 
{
    uint16_t    inPrev      : 1 ;
    uint16_t    state       : 1 ;
    uint16_t    type        : 2 ;
    uint16_t    invertedLed : 1 ;
    uint16_t    address     : 10 ;  // 1..1024
} IOunit ;

class ControlPanel 
{
public:
    void begin( uint8_t, uint8_t, uint8_t, uint8_t ) ; 
    void update() ;

    void setAccessory( uint16_t address, uint8_t state ) ;   // exterm command to update an LED for a point 
    void setOccupancy( uint16_t address, uint8_t state ) ;   // extern command to update an LED for an occupancy
    void allowUpdate() ;                                    // extern command to allow an update


private:
    uint8_t nModules ; 
    Timer   timer ;
    Route   route2set ;
    
    uint8_t pointIndex ;
    uint8_t setRoute      : 1 ;
    uint8_t updateAllowed : 1 ; // will be set extern evenutally via S88 routine, to be implemented.
    uint8_t routeState    : 3 ;

    uint8_t latch ;
    uint8_t clock ;
    uint8_t outNext ;
    uint8_t inNext ;

    uint8_t firstButton, secondButton ;
    uint8_t routeIndex ;

    IOunit  IO[maxIO] ;

    void    routeManager() ;
    void    sendRouteButton( uint8_t ID, uint8_t state ) ;
} ;

extern void notifyPanelSetOccupancy( uint16_t address, uint8_t state ) __attribute__((weak)); 
extern void notifyPanelSetAccessory( uint16_t address, uint8_t state ) __attribute__((weak)); 

#endif