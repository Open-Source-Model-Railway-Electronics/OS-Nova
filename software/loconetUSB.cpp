#include "loconetUSB.h"
#include "slot.h"



enum states
{
    getOPC,
    getLength,
    getPayload,
    getChecksum,
} ;

uint8_t LocoNetUSB::update()
{
    if( Serial.available() )
    {
        uint8_t b = Serial.read() ;

        if( b & 0x80 ) state = getOPC ; 

        switch( state )
        {
        case getOPC: 
            message.OPCODE = b ;

            byteCounter = 0 ;
            message.length = (b & 0b01100000) >> 5 ;

            if(      message.length == 0b00 ) { message.length = 2 ; state = getPayload ; }
            else if( message.length == 0b01 ) { message.length = 4 ; state = getPayload ; }
            else if( message.length == 0b10 ) { message.length = 6 ; state = getPayload ; }
            else if( message.length == 0b11 ) {                      state = getLength ;  }
            break ;
        
        case getLength:
            message.length = b ; 
            message.payload[++byteCounter] = b ; // length is part of payload so we store it in the array as well
            state = getPayload ;
            break ;

        case getPayload:
            message.payload[byteCounter] = b ;    // first iteration byteCounter = 1,

            if( ++ byteCounter == message.length-1 ) // if all bytes are received -> process
            {
                uint8_t checksum = message.OPCODE ^ 0xFF ;                 // start making checksum
                for( int i = 0 ; i < message.length - 2 ; i ++ )    // except for last bytes, XOR the payload  message.payload[byteCounter] should hold the checksum
                {
                    checksum ^= message.payload[i] ;
                }
                state = getOPC ;
                if( checksum == message.payload[byteCounter-1] ) return 1 ; // check if last byte matches checksum and return to application that message is received
            }
            break ;
        }
    }
    return 0 ;
}

void LocoNetUSB::acceptProgrammingCommand()
{
    lnMessage   message2send ;
    uint8_t     len = 0 ;

    message2send.OPCODE = OPC_LONG_ACK ;
    message2send.payload[len++] = OPC_WR_SL_DATA & 0x7F ;
    message2send.payload[len++] = 0x01 ;    // meaning -> program task accepted.

    sendMessage( &message2send, len ) ;
}

void LocoNetUSB::processOpcode()
{
    switch( message.OPCODE )
    {   // ----------------------------------------------------
        //   1) BASIC ACCESSORY COMMANDS  
        // ----------------------------------------------------
        case OPC_SW_REQ:            processSwitchRequest();     return 1 ; 
        case OPC_IMM_PACKET:        processDccExtRequest() ;    return 1 ; 
        // ----------------------------------------------------
        //   2) LOCOMOTIVE/THROTTLE
        // ----------------------------------------------------
        case OPC_LOCO_SPD:          processLocoSpeed() ;        return 1 ; 
        case OPC_LOCO_DIRF:         processLocoDirF0toF4() ;    return 1 ; 
        case OPC_LOCO_SND:          processLocoF5toF8() ;       return 1 ; 
        case OPC_LOCO_F9_F12:       processLocoF9toF12() ;      return 1 ; 
        case OPC_LOCO_F13_F19:      processLocoF13toF19() ;     return 1 ; 
        // ----------------------------------------------------
        // 3) slot commands
        // ----------------------------------------------------
        // case OPC_RQ_SL_DATA:        processSlotRequest() ;      return 1 ;
        // case OPC_SL_RD_DATA:        processSlotReadData() ;     return 1 ;
        // case OPC_SL_RD_DATA_EXP:    processSlotReadDataExp() ;  return 1 ;
        case OPC_WR_SL_DATA:        processWriteSlotData() ;    return 1 ;
        case OPC_WR_SL_DATA_EXP:    processWriteSlotDataExp() ; return 1 ;
        // case OPC_SLOT_STAT1:        processSlotStatus1() ;      return 1 ;
        case OPC_LOCO_ADR:          processLocoAddressRequest() ;   return 1 ;  // retreive slot ID and data of this address and respond with E7 OPC_SL_RD_DATA
        case OPC_MOVE_SLOTS:        processMoveSlots() ;            return 1 ;  // initiated by a throttle (PC)
        // case OPC_IDLE:              processIdleEverywhere() ;   return 1 ;

        case OPC_PEER_XFER:         processConfiguration() ;       return 1 ;
        

    }
    return 0 ; // No OPCODE recognized
}

void LocoNetUSB::processProgrammingCommand()
{
    uint8_t PCMD  = message.payload[1];
    uint8_t CVH   = message.payload[5];
    uint8_t CVL   = message.payload[6];
    uint8_t DATA7 = message.payload[7];

    uint16_t CV =
        (  (CVH >> 5) & 0x01) << 9   // CV9
        | ((CVH >> 4) & 0x01) << 8   // CV8
        | ((CVH >> 0) & 0x01) << 7   // CV7
        |  (CVL & 0x7F);             // CV6..0

    uint16_t value =
        ((CVH >> 1) & 0x01) << 7     // D7 (MSB)
        |  (DATA7 & 0x7F);           // D6..0

    if( PCMD & 0x04 )               // POM WRITE
    {
        uint16_t address = ((uint16_t)message.payload[2] << 7) | (uint16_t)message.payload[3] ; // HOPSA & LOPSA
        notifyPomWrite( address, CV, value );
        return ;
    }

    notifyCvWrite( CV, value ) ;  // Service mode CV WRITE
    acceptProgrammingCommand() ;
}

void LocoNetUSB::processWriteSlotData()
{
    uint8_t slotNr = message.payload[0];
    if( slotNr == 0x7C )                // programming
    {
        processProgrammingCommand() ;
        return;
    }
    // normal processing
    Slot *s = &slot[slotNr];

    s->stat1 = message.payload[1];
    s->address = 
        ( uint16_t)( message.payload[2] & 0x7F ) |
        ((uint16_t)( message.payload[3] & 0x7F ) << 7 ) ;

    s->speed = message.payload[4] & 0x7F;

    uint8_t dirf = message.payload[5];
    s->F0_F4  =  dirf & 0x1F;     // F0..F4
    s->speed |= (dirf & 0x20) << 2; // direction bit

    uint8_t snd = message.payload[6];
    s->F5_F8  =  snd       & 0x0F;
    s->F9_F12 = (snd >> 4) & 0x0F;

   // notifySlotChange(slotNr);
}

void LocoNetUSB::processWriteSlotDataExp()
{
    Serial.println(F("processWriteSlotDataEXP"));

    uint8_t slotNr = message.payload[1];
    Slot *s = &slot[slotNr];

    s->stat1 = message.payload[2];

    uint16_t addr =
        (message.payload[3] & 0x7F) |
        ((uint16_t)(message.payload[4] & 0x7F) << 7);
    s->address = addr;

    s->speed = message.payload[6] & 0x7F;

    uint8_t w5 = message.payload[7];
    uint8_t w6 = message.payload[8];
    uint8_t w7 = message.payload[9];

    s->F0_F4     = w6 & 0x1F;
    s->F5_F8     = w7 & 0x0F;
    s->F9_F12    = (w7 >> 4) & 0x0F;

    s->F13_F20   = message.payload[10];
    s->F21_F28   = message.payload[11];

    //notifySlotChange(slotNr);
}


// first checks if a slot is attached to this address and return that slot number.
// otherwise return the first free availble slot.
// then transmitt the slot data to the computer.

void  LocoNetUSB::processLocoAddressRequest()
{
    Serial.println(F("processLocoAddressRequest"));

    uint16_t reqAddress = message.payload[0] << 7 | message.payload[1] ;
    uint8_t  slot       = getSlotStat( reqAddress ) ; 

    readSlotDataExp( slot ) ;
}

// throttle initiated a null move, respond with E7
void  LocoNetUSB::processMoveSlots()
{

    Serial.println(F("processMoveSlots"));
    uint8_t src  = message.payload[0];
    uint8_t dest = message.payload[1];

    if( src == dest && dest > 0 ) { readSlotDataExp( dest ) ; } // may need to be changed to readSlotData( dest )
    else if( src  == 0 )          { readSlotDataExp( src  ) ; }               // DISPATCH GET, an other throttle issues a command for dispatch PUT
    else if( dest == 0 )          { dispatchAddres = slot[dest].address ; }  // DISPATCH PUT, mark slot as distpach
}



void LocoNetUSB::readSlotData( uint8_t _slot )
{
    Slot *s = &slot[_slot];

    uint8_t len = 0 ;
    lnMessage message2send ;
    message2send.OPCODE = OPC_SL_RD_DATA ;   // 0xE7 (READ)


}

void LocoNetUSB::readSlotDataExp( uint8_t _slot )
{
    Slot *s = &slot[_slot];

    uint8_t len = 0 ;
    lnMessage message2send ;
    message2send.OPCODE = OPC_SL_RD_DATA_EXP ;   // 0xE6 (READ)

    message2send.payload[ len++ ] = 0x15 ;       // length = 21 bytes
    message2send.payload[ len++ ] = _slot ;      // SLOT#  (bij READ komt SLOT direct na LENGTH)

    // ---- d0: STAT1 ----
    message2send.payload[ len++ ] = s->stat1 ;

    // ---- d1–d2: ADDRESS (LOW, HIGH) ----
    uint16_t addr = s->address ;
    uint8_t adr_lo = addr        & 0x7F ;
    uint8_t adr_hi = (addr >> 7) & 0x7F ;

    message2send.payload[ len++ ] = adr_lo ;     // d1
    message2send.payload[ len++ ] = adr_hi ;     // d2

    // ---- d3: FLAGS (power, busy, etc) ----
    uint8_t flags = 0x01 ;                       // power=1
    message2send.payload[ len++ ] = flags ;      // d3

    // ---- d4: SPEED (7-bit) ----
    uint8_t spd7 = s->speed & 0x7F ;
    message2send.payload[ len++ ] = spd7 ;       // d4

    // ---- d5: high-order function bits ----
    uint8_t w5 = 0 ;
    w5 |= ( (s->F21_F28 >> 4) & 0x01 ) << 6 ;    // F28
    w5 |= ( (s->F13_F20 >> 4) & 0x01 ) << 5 ;    // F20
    w5 |= ( (s->F9_F12  >> 4) & 0x01 ) << 4 ;    // F12
    w5 |= ( s->speed & 0x0F ) ;                  // speed low 4 bits

    message2send.payload[ len++ ] = w5 ;         // d5

    // ---- d6: DIR + F0–F4 ----
    uint8_t dir = (s->speed >> 7) & 0x01 ;
    uint8_t w6 = (dir << 5) | (s->F0_F4 & 0x1F) ;
    message2send.payload[ len++ ] = w6 ;         // d6

    // ---- d7: F5–F8 + F9–F12 ----
    uint8_t w7 = s->F5_F8 | (s->F9_F12 << 4) ;
    message2send.payload[ len++ ] = w7 ;         // d7

    // ---- d8: F13–F20 ----
    message2send.payload[ len++ ] = s->F13_F20 ; // d8

    // ---- d9: F21–F28 ----
    message2send.payload[ len++ ] = s->F21_F28 ; // d9

    // ---- d10: protocol + speed mode ----
    uint8_t w10 = 0 ;
    w10 |= 8 ;    // protocol = DCC
    w10 |= 6 ;    // 28 speed steps
    message2send.payload[ len++ ] = w10 ;        // d10

    // ---- d11: SS of consist (unused) ----
    message2send.payload[ len++ ] = 0x00 ;       // d11

    // ---- d12/d13: consist link ----
    message2send.payload[ len++ ] = 0x7F ;       // d12 = no link
    message2send.payload[ len++ ] = 0x00 ;       // d13 = no link

    // ---- d14/d15: throttle ID ----
    message2send.payload[ len++ ] = 0x01 ;       // d14
    message2send.payload[ len++ ] = 0x01 ;       // d15

    sendMessage( &message2send, len ) ;
}




void LocoNetUSB::processSwitchRequest()
{
    uint8_t sw1 = message.payload[0];
    uint8_t sw2 = message.payload[1];

    // Extract 11-bit accessory address
    uint16_t addr =
        ((sw2 & 0x0F) << 7) |   // A10..A7
        (sw1 & 0x7F);           // A6..A0

    uint8_t dir = (sw2 & 0b00000100) ? 1 : 0;
    uint8_t on  = (sw2 & 0b00000010) ? 1 : 0;

    // Only call callback when ON=1 (Digitrax convention)
    if(on)
        notifyLnetAccessory(addr, dir);
}

void LocoNetUSB::processDccExtRequest()
{
    // Structure:
    // payload[0] = LEN
    // payload[1] = 0x7F
    // payload[2] = REPS
    // payload[3] = DHI
    // payload[4] = IM1  (addr low 7b)
    // payload[5] = IM2  (addr high 4b)
    // payload[6] = IM3  (value low 7b)
    // (+optional IM4/IM5 for longer packets)

    if (message.length < 8) return;

    uint8_t dhi = message.payload[3];
    uint8_t type = (dhi >> 4) & 0b111;

    // 101 = Extended Accessory packet
    if (type != 0b101) return;

    uint8_t im1 = message.payload[4];
    uint8_t im2 = message.payload[5];
    uint8_t im3 = message.payload[6];

    // --- 11-bit extended accessory address ---
    uint16_t a_low  = im1 & 0x7F;  // A6..A0
    uint16_t a_high = im2 & 0x0F;  // A10..A7

    uint16_t address = (a_high << 7) | a_low;

    // --- 8-bit extended accessory VALUE (0–255) ---
    uint8_t low7  = im3 & 0x7F;        // bits 0..6
    uint8_t high1 = (dhi & 0x01);      // bit 7
    uint8_t value = (high1 << 7) | low7;

    notifyLnetAccessoryExt(address, value);
}




// helpers

uint8_t calculateChecksum( lnMessage* mess, uint8_t len )  // with lenght is 2, we have a 4 byte message. OPCODE + 2 payload bytes + checksum
{
    uint8_t checksum = 0xFF ^ mess->OPCODE ;

    for( int i = 0 ; i < len ; i ++ )
    {
        checksum ^= mess->payload[i] ;
    }
    return checksum ;
}


// transmitts the loconet messsage on the serial bus to the computer.
void LocoNetUSB::sendMessage( lnMessage* mess, uint8_t len ) 
{
    Serial.write( mess -> OPCODE ) ;
    for( int i = 0 ; i < len ; i ++ )
    {
        Serial.write( mess->payload[i] ) ;
    }
    uint8_t checksum = calculateChecksum( mess, len ) ;
    Serial.write( checksum ) ;
}



// transmitt functions
void LocoNetUSB::sendAccesory( uint16_t _address, uint8_t state ) // 4 byte message
{
    uint8_t sw1 =  _address & 0x7F;         // A0..A6
    uint8_t sw2 = (_address >> 7) & 0x0F;   // A7..A10 → bits 3..0
    if( state ) sw2 |= 0b00000100 ;
    sw2 |= 0b00000010 ;  // Always send an ON pulse (standard)
    lnMessage message2send ;
    uint8_t len = 0 ;
    message2send.OPCODE = OPC_SW_REQ ;
    message2send.payload[ len++ ] = sw1 ;
    message2send.payload[ len++ ] = sw2 ;
    sendMessage( &message2send, len ) ;     // length 2 is parsed here, the first byte is the OPC, 2 payload bytes + a checksum makes 4

}

void LocoNetUSB::sendSensor( uint16_t _address, uint8_t state )  // 4 byte message
{
    uint8_t sn1 = _address & 0x7F ;         // SN1: A0..A6
    uint8_t sn2 = (_address >> 7) & 0x0F ;  // SN2: A7..A10 in bits 3..0
    if (state) sn2 |= 0x20;   // bit5 = occupied / active
    lnMessage message2send ;
    uint8_t len = 0 ;
    message2send.OPCODE = OPC_INPUT_REP ;
    message2send.payload[ len++ ] = sn1 ;
    message2send.payload[ len++ ] = sn2 ;
    sendMessage( &message2send, len ) ;     // length 2 is parsed here, the first byte is the OPC, 2 payload bytes + a checksum makes 4
}

void LocoNetUSB::sendLocoSpeed( uint8_t mySlot )
{
    uint8_t len = 0 ;
    lnMessage message2send ;
    message2send.OPCODE = OPC_LOCO_SPD ;
    message2send.payload[ len++ ] = mySlot ;
    message2send.payload[ len++ ] = slot[mySlot].speed ;

    sendMessage( &message2send, len ) ;
}

void LocoNetUSB::sendLocoDirF0toF4( uint8_t mySlot )
{
    uint8_t len = 0 ;

    uint8_t dirf = ( slot[mySlot].dir ? 0x20 : 0x00 ) | ( slot[mySlot].F0_F4 & 0x0F ) ;
    lnMessage message2send ;
    message2send.OPCODE = OPC_LOCO_DIRF ;
    message2send.payload[ len++ ] = mySlot ;
    message2send.payload[ len++ ] = dirf ;

    sendMessage( &message2send, len ) ;
}

void LocoNetUSB::sendLocoF5toF8( uint8_t mySlot )
{
    uint8_t len = 0 ;
    lnMessage message2send ;
    message2send.OPCODE = OPC_LOCO_SND ;
    message2send.payload[ len++ ] = mySlot ;
    message2send.payload[ len++ ] = slot[mySlot].F5_F8  ;

    sendMessage( &message2send, len ) ;
}

void LocoNetUSB::sendLocoF9toF12( uint8_t mySlot )
{
    uint8_t len = 0 ;
    lnMessage message2send ;
    message2send.OPCODE = OPC_LOCO_F9_F12 ;
    message2send.payload[ len++ ] = mySlot ;
    message2send.payload[ len++ ] = slot[mySlot].F9_F12 ;

    sendMessage( &message2send, len ) ;
}

void LocoNetUSB::sendConfiguration( )
{

}


// Incomming loconet Locomotive commands
void LocoNetUSB::processLocoSpeed()
{
    // OPC_LOCO_SPD: <A0><SLOT#><SPD><CHK>
    uint8_t slot  = message.payload[0];
    uint8_t speed = message.payload[1] & 0x7F;   // msb moet 0 zijn, maar masken is veilig
    notifyLnetLocoSpeed( slot, speed );
}

void LocoNetUSB::processLocoDirF0toF4()
{
    // OPC_LOCO_DIRF: <A1><SLOT#><DIRF><CHK>
    uint8_t slot = message.payload[0];
    uint8_t dirf = message.payload[1];

    // D5 = direction (1 = forward)
    uint8_t dir = (dirf & (1u << 5)) ? 1u : 0u;
    notifyLnetLocoDirection( slot, dir );

    // D4..D0 = F0, F4, F3, F2, F1 (zie 1.2d spec)
    notifyLnetLocoFunction( slot,  0, (dirf & (1u << 4)) ? 1u : 0u ); // F0
    notifyLnetLocoFunction( slot,  4, (dirf & (1u << 3)) ? 1u : 0u ); // F4
    notifyLnetLocoFunction( slot,  3, (dirf & (1u << 2)) ? 1u : 0u ); // F3
    notifyLnetLocoFunction( slot,  2, (dirf & (1u << 1)) ? 1u : 0u ); // F2
    notifyLnetLocoFunction( slot,  1, (dirf & (1u << 0)) ? 1u : 0u ); // F1
}

void LocoNetUSB::processLocoF5toF8()
{
    // OPC_LOCO_SND: <A2><SLOT#><SND><CHK>
    uint8_t slot = message.payload[0];
    uint8_t snd  = message.payload[1];

    // D3..D0 = F8..F5
    notifyLnetLocoFunction( slot,  8, (snd & (1u << 3)) ? 1u : 0u ); // F8
    notifyLnetLocoFunction( slot,  7, (snd & (1u << 2)) ? 1u : 0u ); // F7
    notifyLnetLocoFunction( slot,  6, (snd & (1u << 1)) ? 1u : 0u ); // F6
    notifyLnetLocoFunction( slot,  5, (snd & (1u << 0)) ? 1u : 0u ); // F5
}

void LocoNetUSB::processLocoF9toF12()
{
    // OPC_LOCO_F912: <A3><SLOT#><F912><CHK>
    // SPEC NOTE: de 1.2d spec zegt alleen "SET SLOT F9-F12 functions".
    // Hier neem ik (conform DIRF/SND patroon) aan:
    //   D3 = F12, D2 = F11, D1 = F10, D0 = F9   ***CHECK dit tegen je eigen testcases***
    uint8_t slot = message.payload[0];
    uint8_t f912 = message.payload[1];

    notifyLnetLocoFunction( slot, 12, (f912 & (1u << 3)) ? 1u : 0u ); // F12
    notifyLnetLocoFunction( slot, 11, (f912 & (1u << 2)) ? 1u : 0u ); // F11
    notifyLnetLocoFunction( slot, 10, (f912 & (1u << 1)) ? 1u : 0u ); // F10
    notifyLnetLocoFunction( slot,  9, (f912 & (1u << 0)) ? 1u : 0u ); //  F9
}

void LocoNetUSB::processLocoF13toF19()
{
    // SPEC NOTE:
    // F13–F19 zitten in slotexpansie-byte w8:
    //   D6 = F19, ..., D0 = F13. Ik ga er hier vanuit dat de opcode
    // die jij aan deze functie koppelt dezelfde bit-layout gebruikt.
    // Als dat niet zo is (eigen protocol / PEER_XFER etc.), pas de
    // bitmasks hieronder aan jouw definitieve layout aan.

    uint8_t slot = message.payload[0];
    uint8_t fn   = message.payload[1];

    notifyLnetLocoFunction( slot, 19, (fn & (1u << 6)) ? 1u : 0u ); // F19
    notifyLnetLocoFunction( slot, 18, (fn & (1u << 5)) ? 1u : 0u ); // F18
    notifyLnetLocoFunction( slot, 17, (fn & (1u << 4)) ? 1u : 0u ); // F17
    notifyLnetLocoFunction( slot, 16, (fn & (1u << 3)) ? 1u : 0u ); // F16
    notifyLnetLocoFunction( slot, 15, (fn & (1u << 2)) ? 1u : 0u ); // F15
    notifyLnetLocoFunction( slot, 14, (fn & (1u << 1)) ? 1u : 0u ); // F14
    notifyLnetLocoFunction( slot, 13, (fn & (1u << 0)) ? 1u : 0u ); // F13
}



void LocoNetUSB::processConfiguration()
{
    uint8_t type = message.payload[1] ;
    switch( type )
    {
    case 1: setConfiguration( &message ) ; break ;
    case 2: setPanelButton(   &message ) ; break ;
    case 3: setRoute(         &message ) ; break ;
    case 4: setThrottle(      &message ) ; break ;

    case 5: getConfiguration(); break;
    case 6: getPanelButton  ( message.payload[2] ); break; // when a request is sent by PC, the 2nd payload byte should carry the ID of button, route or throttle.
    case 7: getRoute        ( message.payload[2] ); break;
    case 8: getThrottle     ( message.payload[2] ); break;
    }
}



LocoNetUSB locoNetUsb ;