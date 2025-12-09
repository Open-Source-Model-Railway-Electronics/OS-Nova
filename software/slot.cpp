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

#include "slot.h"

Slot slot[ maxSlot ]  ;

void clearValues( uint8_t number )
{
    slot[number].speed          = 0 ;
    slot[number].F0_F4          = 0 ;
    slot[number].F5_F8          = 0 ;
    slot[number].F9_F12         = 0 ;
    slot[number].F13_F20        = 0 ;
    slot[number].F21_F28        = 0 ;
    slot[number].F29_F36        = 0 ;
    slot[number].newInstruction = 0 ;
}

void purgeSlot( uint8_t number )
{
    slot[number].stat1      = SLOT_FREE ;           // TODO invoke callback that the slot became free
    slot[number].address    =         0 ;
    clearValues(number) ;

    notifySlotPurge( number ) ;
}


void updateSlots()
{
    REPEAT_MS( 60000 ) // every minute, update all slot's priorities
    {
        for( int i = 0 ; i < maxSlot ; i++ )
        {
            if( slot[i].set )
            {   slot[i].set = 0 ;

                if( slot[i].priority <  40 ) slot[i].priority ++ ;
            }
            else if( slot[i].speed == 0 && slot[i].stat1 != SLOT_IN_USE ) // if train is not moving and it's slot is not in use by a throttle, the train may be purged eventually
            {
                if( slot[i].priority >   0 ) slot[i].priority -- ;

                if( slot[i].priority == 0 )
                {
                    purgeSlot(i) ;
                }
            }
        }
    }
    END_REPEAT
}



uint8 getFreeSlot() // if new slot is needed, find a slot with 0 priority and otherwise pick the slot with the lowest priority
{
    uint8 lowestPrio =  40 ;
    uint8 slotToUse  =   0 ;

    for( int i = 1 ; i < maxSlot ; i++ ) // we do not use slot 0
    {
        if( slot[i].priority < lowestPrio )
        {
            lowestPrio = slot[i].priority ;
            slotToUse = i ;
        }
        if( lowestPrio == 0 ) break ; // we aren't going to find a lower value and this slot is free, so use it
    }

    return slotToUse ;
}

// find and initialize a slot for given address
uint8 commissionSlot( uint16 address )
{
    uint8 slotNumber = getFreeSlot() ;      // find either a free or the least used slot to commission

    slot[slotNumber].address  = address ;   // couple address
    slot[slotNumber].priority = 20 ;        // new trains will be active for atleast 20 minutes if not received new instructions.
    slot[slotNumber].stat1    = SLOT_IN_USE ;    

    clearValues( slotNumber ) ;             // ensure any old values for speed and functions are whiped. 

    return slotNumber ;
}

/* finds out which slot is attached to this address and return the slot's state
   if no slot can be found for the specified address. The slot is free to use

   
*/

uint8_t getSlotStat( uint16_t address )
{
    for (int i = 1 ; i < maxSlot; i++)  // we do not use slot 0
    {
        if( address == slot[i].address )
        {
            return i ;
        }
    }
    return  getFreeSlot() ;
}

/* finds out which slot is attached to this address and return the slot number
    if the slot is not found in the active list, a new slot will be allocated.
*/
uint8_t getSlot( uint16_t address )
{
    for (int i = 1 ; i < maxSlot; i++)  // we do not use slot 0
    {
        if( address == slot[i].address )
        {
            return i ;                  // match found, return slot number
        }
    }

    return commissionSlot( address ) ;  // no slot found -> commission new slot and return it
}


