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

#include "src/macros.h"
#include <Arduino.h>


const int NA              = 0xFF ;
const int maxSlot         =   25 ; // max amount of active trains.

const int SLOT_FREE        = 0x00 ;   // 0000 0000
const int SLOT_IDLE        = 0x01 ;   // 0000 0001
const int SLOT_IN_USE      = 0x02 ;   // 0000 0010
const int SLOT_COMMON      = 0x03 ;   // 0000 0011
const int SLOT_DISPATCH    = 0x04 ;   // 0000 0100



typedef struct Slots
{
    uint8_t     speed : 7;
    uint8_t     dir   : 1;
    uint16_t    address ;
    uint8_t     newInstruction ;            
    uint8_t     F0_F4 ;
    uint8_t     F5_F8 ;
    uint8_t     F9_F12 ;
    uint8_t     F13_F20 ;
    uint8_t     F21_F28 ;
    uint8_t     F29_F36 ;
    uint8_t     priority : 7 ;
    uint8_t     set      : 1 ;

    uint8_t     stat1  ; //   ( IN_USE, COMMON, IDLE, FREE, DISPATCH).
} Slot ;

extern Slot     slot[maxSlot]  ;
  
extern uint8_t  getSlotStat( uint16_t ) ;
extern uint8_t  getSlot(     uint16_t ) ;
extern void     updateSlots() ;
extern void     notifySlotPurge( uint8_t ) __attribute__((weak)) ;

