#include "slot.h"
#include "DCC.h"
#include "S88.h"
#include "throttle.h"
#include "loconetUSB.h"
#include "NmraDcc.h"
#include "controlPanel.h"

/* TODO 
    *   organize EEPROM
    *   control panel code (LAST)
    *   PWM controller code, (modified weistra to work at 100Hz?
    *   add secondary PWM controller unit due to cab control
    *   custom loconet berichten toevoegen om dingen in te regelen
    *   in kaart brengen der settings.
        * aantal S88
        * booster mode vs sniffer mode
        * aantal control panel modules
        * uitwerken controle paneel settings (bezetmelder, adressen, routes)
    * add DEBUG dingen.
    * Fix de DCC controle functie en de brug functionaliteiten.

    Het merendeel zit er in, het is nu wellicht tijd voor deel tests. 
       S88 heeft HW nodig dus nog ff niet

       De loconet berichten kunnen in principe getest worden met een python script. Dan kan ook de slot manager ed getest worden.
       leuke klus voor chad.
          - Ik wil knoppen met daarachter invulbare velden zodat ik berichten kan spoofen en antwoorden kan bekijken
          - Ik wil een slot tracker hebben, die slots kan monitoren. De callbacks kunnen gespoofd worden

       En ik wil tussen de loconet berichten door ook iets van data printen. als ik 7 bit data blijf sturen, dan trip ik de OPCODE dinges niet. en dan debug co-bestaan
       in principe met ascii gaat dat al goed. Dus ik kan feitelijk serial.print doen waar ik maar wil.


       CONTORL PANEL  is done for big part
         the route manager is to be done entirely
         control functions to set accesorries LEDs
         
         introduce a type to save one bit per unit. by removing isAccessory, isOccupancy, isRoutebutton
        
    // TODO add custom loconet  messages for configuration
    both to send and receive.

    5 bits S88 count
    2 bits S88 start address  (4 groups max)

    1 bit  booster/sniffer
    2 bit  throttle mode

    throttles? 
    6x digital A-E
    6x analog  A-E
    3x analog  A-C
    2x analog  A-B 

    wat ook leuk is, als je 1 digitale throttle heb en een setje analoge, zodat je ook nog kan dispatchen van de ene naar de ander

    Also: Upon booting, the centrale could voluntair its settings to a computer program. Or send all with one specific unique command

    Throttles can be simulated by spoofing ADC samples
    

    We need settings for:
      config message      <OPC>   < S88 >    < DCC >
      S88 modules count   <OPC>    <count>   <dummy>   <checksum> 
      control panel:      <OPC>  <Button ID>, <type>  <addr H> <addr L> 
      Routes              <OPC>  <length>  <state + adrH_x> <adrL_x> .... <checksum>
      short/over circuit time  <OPC>  <short>  <overcurrent> <checksum>
      DCC booster/sniffer
      Throttle addresses  <>  // for 6 throttles we may need up to 60 addresses, 10 per throttle.
                          <OPC> <length (12)> <ADR_1_H> <ADR_1_L> <ADR_2_H> <ADR_2_L> <ADR_3_H> <ADR_3_L> <ADR_4_H> <ADR_4_L> <ADR_5_H> <ADR_5_L> <ADR_6_H> <ADR_6_L> <checksum>
                           // length may work with carrying throttle counts, with 2 throttles we can just send 2x 12 bytes and be done.

                    LEN             TYP 
    <OPC_PEER_XFER> <8>             <1>  <AACCCCC> <xxxxBTT> <SSSSSSS> <OOOOOOO> <CHECKSUM>    S88 address, module count,  booster/sniffer Throttle mode, short time, OC time
    <OPC_PEER_XFER> <8>             <2>  <BBBBBBB> <xxxxTTT> <AAAAAAA> <AAAAAAA> <CHECKSUM>  button ID, button Type, ADR H, ADR L
    <OPC_PEER_XFER> <6 + 2x points> <3>  <1111111> <2222222> <SAAAAAA> <AAAAAAA> ....  <CHECKSUM>   btn ID, btn ID, 10 address _ state
    <OPC_PEER_XFER> <25 >           <4>  <xxxxAAA> <AAAAAA1> <AAAAAA1> ....  <AAAAA10> <AAAAA10> <CHECKSUM>   throttle ID + 10 x 2 bytes

*/
Throttle        throttles[6] ;  // up to 6 throttles.
S88             s88(2,3,4,5) ;
NmraDcc         dcc ;
ControlPanel    panel ;



const int config_eeAddress = 0 ;
// <OPC_PEER_XFER> <8> <1>  <AACCCCC> <xxxxBTT> <SSSSSSS> <OOOOOOO> <CHECKSUM>    S88 address, module count,  booster/sniffer Throttle mode, short time, OC time
typedef struct      // not yet in use.
{
    uint8_t s88BaseAddress  : 2 ;
    uint8_t nS88Count       : 5 ;
    uint8_t boosterMode     : 1 ;
    uint8_t throttleMode    : 3 ;
    uint8_t shortTime       : 7 ;
    uint8_t overCurrentTime : 6 ;

} Settings ;

Settings settings ;


void setup()
{
    initDCC() ;

    dcc.pin( 2, 0 ) ;
    dcc.init( MAN_ID_DIY, 11, FLAGS_OUTPUT_ADDRESS_MODE | FLAGS_DCC_ACCESSORY_DECODER, 0 );

    Serial.begin( 115200 ) ;

    Serial.println(F("hello world")) ;

    //locoNetUsb.readSlotDataExp( 0 ) ;
}

void loop()
{
    panel.update() ;
    dcc.process() ;
    updateSlots() ;
    DCCsignals() ;
    s88.update() ;
    locoNetUsb.update() ;
}

// COMES FROM S88 BUS
void notifyS88sensor( uint16_t address, uint8_t state)
{
    locoNetUsb.sendSensor( address, state ) ;
    panel.setOccupancy( address, state ) ;
}





// COMES FROM THROTTLES
void notifyThrottleSpeed( uint8_t slot, uint8_t dir, uint8_t speed )
{
    setSpeedDir( slot, speed, dir ) ; // relay command to DCC module.
    locoNetUsb.sendLocoSpeed( slot ) ;
}

void notifyThrottleFunction( uint8_t slot, uint8_t fn, uint8_t state )
{
    setFunction( slot, fn, state ) ;  // this function sets all slot's functions well.

    // yeah.. need to think of something, to fix this mess. I believe that we should simply set or clear the slot's function and
    locoNetUsb.sendLocoDirF0toF4( slot ) ;
    locoNetUsb.sendLocoF5toF8(    slot ) ;
    locoNetUsb.sendLocoF9toF12(   slot ) ;
}

void notifySlotChange( uint8_t slot )       // used when throttles change slots
{
    locoNetUsb.readSlotDataExp( slot ) ;
}



// SLOT MANGER
void notifySlotPurge( uint8_t slot2purge )      // slot manager may purge when slots aren't used for some time.
{
    locoNetUsb.readSlotDataExp( slot2purge ) ; // this slot is purged, and now a read slot data is sent telling the PC that the slot is no longer in use.
    // no relation to control panel.
}



// COMES FROM PC COMMANDS / FROM LOCONET MODULE   UPDATE DCC CODE AND CONTROL PANEL
uint8_t notifyLnetSwitch( uint16_t address, uint8_t direction )
{
    setAccessory( address, direction ) ;
    panel.setAccessory( address, direction ) ;
}

uint8_t notifyLnetSensor( uint16_t address, uint8_t state ) // unsure if this is needed. This works when you in iTrain manually trigger a sensor..
{
    panel.setOccupancy( address, state ) ;
}




// LOCONET LOCOMOTIVE COMMANDS, -> DCC code only
uint8_t notifyLnetLocoSpeed( uint8_t currentSlot, uint8_t speed )
{
    setSpeedDir( currentSlot, speed, slot[currentSlot].dir ) ;
}

uint8_t notifyLnetLocoDirection( uint8_t currentSlot, uint8_t dir )
{
    setSpeedDir( currentSlot, slot[currentSlot].speed, dir ) ;
}

uint8_t notifyLnetLocoFunction( uint8_t slot, uint8_t fn, uint8_t value )
{
    setFunction( slot, fn, value ) ;
}

// possible problems, though we have just 25 slots, we need to for-loop for every loco instruction and this is ALOT.
// therefor we need to debounce this. NEW instructions are repeately sent like 10-20 times. Therefor, if we catch 2 identical addresses in sequence, it means we have a NEW instruction
// and the new instruction needs to be relayed to the DCC outgoing code.

void notifyDccSpeed( uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps) 
{
    static uint16_t    first = 0xFFFF ;
    static uint16_t   second = 0xFFFF ;
    static uint16_t lastAddr = 0xFFFF ;
    static  uint8_t    state =      0 ;

    if( state )  first = Addr ;     // if 2 following cycli the address has not changed, first and second will carry the same address 
    else        second = Addr ;
    state ^= 1 ;

    if( first == second && lastAddr != Addr )  // if first and second carry the same address and it is not yet proccessed, do it!
    {   lastAddr  = Addr ;

        uint8_t slot = getSlot( Addr ) ;
        setSpeedDir( slot, Speed, Dir ) ;

    }
}


void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint16_t FuncState)
{
    static uint16_t first = 0xFFFF, second = 0xFFFF, lastAddr = 0xFFFF;
    static uint8_t  state = 0;

    if(state) first = Addr; else second = Addr;
    state ^= 1;

    if(first == second && lastAddr != Addr)
    {
        lastAddr = Addr;
        uint8_t slotNum = getSlot(Addr);

        if(FuncGrp == FN_0_4)
        {
            setFunction(slotNum, 0, (FuncState & FN_BIT_00) ? 1 : 0);  // F0
            setFunction(slotNum, 1, (FuncState & FN_BIT_01) ? 1 : 0);
            setFunction(slotNum, 2, (FuncState & FN_BIT_02) ? 1 : 0);
            setFunction(slotNum, 3, (FuncState & FN_BIT_03) ? 1 : 0);
            setFunction(slotNum, 4, (FuncState & FN_BIT_04) ? 1 : 0);  // F4
            return;
        }

        uint8_t startFn=0, endFn=0;

        if(      FuncGrp ==   FN_5_8 ){ startFn =  5; endFn =  8; }
        else if( FuncGrp ==  FN_9_12 ){ startFn =  9; endFn = 12; }
        else if( FuncGrp == FN_13_20 ){ startFn = 13; endFn = 20; }
        else if( FuncGrp == FN_21_28 ){ startFn = 21; endFn = 28; }
        else return;

        for(uint8_t fn = startFn; fn <= endFn; fn++)
        {
            uint8_t mask = (1 << fn);
            setFunction(slotNum, fn, (FuncState & mask) ? 1 : 0);
        }
    }
}

// LOCONET PROGRAM COMMANDS
void notifyCvWrite( uint16_t CV, uint8_t value )
{
    writeCv( CV, value ) ;
}

void notifyPomWrite( uint16_t address, uint16_t CV, uint8_t value )
{
    uint8_t slot = getSlot( address ) ;
    writePom( slot, CV, value ) ;
}



// LOCONET CONFIGURATION COMMANDS
                    //          2        3         4          5
// <OPC_PEER_XFER> <8> <1>  <AACCCCC> <xxxxBTT> <SSSSSSS> <xOOOOOO> <CHECKSUM>    S88 address, module count,  booster/sniffer Throttle mode, short time, OC time
void setConfiguration(  lnMessage *mess )
{
    uint16_t eeAddress = config_eeAddress ;
    settings.s88BaseAddress  = ((mess->payload[2] >> 5) * nBitsPerModule * maxModules) + 1 ; //  1-496   497-992   993-1488   1489-1984   1985-2480
    settings.nS88Count       =  (mess->payload[2] & 0x1F ) ;
    settings.boosterMode     =  (mess->payload[3] >> 2   ) & 1 ;
    settings.throttleMode    =  (mess->payload[3] & 0x03 ) ;
    settings.shortTime       =  (mess->payload[4]        ) ;
    settings.overCurrentTime =  (mess->payload[5] & 0x3F ) ;
   // 2cEeprom.put( eeAddress, settings ) ;
}

                    //          2        3         4          5
// <OPC_PEER_XFER> <8>  <2>  <BBBBBBB> <xxxxITT> <AAAAAAA> <AAAAAAA> <CHECKSUM>   ID,  invert LED + Type, ADR H, ADR L
void setPanelButton( lnMessage *mess )
{
    uint16_t eeAddress = mess->payload[2] * sizeof(IOunit) ;

    IOunit toStore ;
    toStore.type        =  mess->payload[3]        & 0b11 ;
    toStore.invertedLed = (mess->payload[3] >> 2)  & 0b01 ;
    toStore.address   = (uint16_t)mess->payload[4]<< 7 | (uint16_t)mess->payload[5] ;
    //i2cEeprom.put( eeAddress, toStore ) ;

}
//                       0          1       2          3         4         5        6
// <OPC_PEER_XFER> <7 + 2x points> <3> <ROUTE ID>  <1111111> <2222222> <SAAAAAA> <AAAAAAA> ....  <CHECKSUM>   btn ID, btn ID, 10 address _ state
void setRoute( lnMessage *mess )
{
    uint8_t  routeID    = mess->payload[2] ;
    uint16_t eeAddress  = route_eeAddress + routeID * sizeof( Route );

    Route route ;

    route.firstButton  = mess->payload[3] ;
    route.secondButton = mess->payload[4] ;

    for ( int point = 0 ; point < pointsPerRoute ; point )
    {
       route.address[point] = (((uint16_t)mess->payload[5+(point*2)] & 0x3F)<<7) | (uint16_t)mess->payload[6+(point*2)] ;
       route.state[point]   = mess->payload[5+(point*2)] >> 6 ;
    }
    //i2cEeprom.put( eeAddress, route ) ;
}

//   payload index   0     1       2          3         4     ....    23         24
// <OPC_PEER_XFER> <25 >  <4>  <xxxxAAA> <AAAAAA1> <AAAAAA1> ....  <AAAAA10> <AAAAA10> <CHECKSUM>   throttle ID + 10 x 2 bytes
void setThrottle( lnMessage *mess )
{
    uint8_t  throttleID = mess->payload[2] ;
    uint16_t eeAddress  = throttle_eeAddress + throttleID * 20 ; // addresses array size
    
    uint16_t array[10] ;
    for( int i = 0 ; i < 10 ; i ++ )
    {
        array[i] = (((uint16_t)mess->payload[3 + 2*i ])<<7) | (uint16_t)mess->payload[4 + 2*i ] ;
    }
    //i2cEeprom.put( eeAddress, array ) ;
}


void getConfiguration( void )
{
    lnMessage msg;

    msg.OPCODE = OPC_PEER_XFER;
    msg.payload[0] = 8;
    msg.payload[1] = 5;    // GET configuration

    Settings cfg;
    uint16_t eeAddress = config_eeAddress;
    //i2cEeprom.get( eeAddress, cfg );

    uint8_t AA = (cfg.s88BaseAddress - 1) / (nBitsPerModule * maxModules);
    uint8_t CCCCC = cfg.nS88Count;

    msg.payload[2] = (AA << 5) | (CCCCC & 0x1F);
    msg.payload[3] = ((cfg.boosterMode & 1) << 2) | (cfg.throttleMode & 0x03);
    msg.payload[4] = cfg.shortTime;
    msg.payload[5] = (cfg.overCurrentTime & 0x3F);

    locoNetUsb.sendMessage( &msg, msg.payload[0] + 2 );
}


void getPanelButton( uint8_t panelButtonID )
{
    lnMessage msg;

    msg.OPCODE = OPC_PEER_XFER;
    msg.payload[1] = 6;               // GET panel button
    msg.payload[2] = panelButtonID;   // caller must set this ID

    uint16_t eeAddress = panelButtonID * sizeof(IOunit);

    IOunit unit;
    // i2cEeprom.get( eeAddress, unit );

    msg.payload[3] =
          ((unit.invertedLed & 1) << 2)
        | ( unit.type        & 0x03 );

    msg.payload[4] = (unit.address >> 7) & 0x7F;
    msg.payload[5] =  unit.address       & 0x7F;

    msg.payload[0] = 8;

    locoNetUsb.sendMessage( &msg, msg.payload[0] + 2 );
}


void getRoute( uint8_t routeID )
{
    lnMessage msg ;

    msg.OPCODE = OPC_PEER_XFER ;
    msg.payload[1] = 7 ;        // GET route
    msg.payload[2] = routeID ;

    Route r;
    uint16_t eeAddress = route_eeAddress + routeID * sizeof(Route) ;
    // i2cEeprom.get( eeAddress, r );

    msg.payload[3] = r.firstButton ;
    msg.payload[4] = r.secondButton ;

    uint8_t p = 5;

    for(int i = 0; i < pointsPerRoute; i++)
    {
        uint8_t hi6 = (r.address[i] >> 7) & 0x3F ;
        uint8_t lo7 =  r.address[i]       & 0x7F ;

        msg.payload[p++] = (r.state[i] << 6) | hi6 ;
        msg.payload[p++] = lo7 ;
    }

    msg.payload[0] = p ;

    locoNetUsb.sendMessage( &msg, msg.payload[0] + 2 ) ;
}

void getThrottle( uint8_t throttleID )
{
    lnMessage msg ;

    msg.OPCODE = OPC_PEER_XFER ;
    msg.payload[1] = 8 ;          // GET throttle
    msg.payload[2] = throttleID ;

    uint16_t eeAddress = throttle_eeAddress + throttleID * 20 ;

    uint16_t array[10] ;
    // i2cEeprom.get( eeAddress, array );

    uint8_t p = 3 ;

    for(int i=0; i<10; i++)
    {
        uint16_t v = array[i];

        uint8_t lo7 =  v        & 0x7F ;
        uint8_t hi7 = (v >> 7)  & 0x7F ;

        msg.payload[p++] = lo7 ;
        msg.payload[p++] = hi7 ;
    }

    msg.payload[0] = p;

    locoNetUsb.sendMessage( &msg, msg.payload[0] + 2 );
}







// COMES FROM CONTROL PANEL -> DCC & LOCONET

void notifyPanelSetAccessory( uint16_t address, uint8_t state ) 
{
    setAccessory( address, state ) ;
    locoNetUsb.sendAccesory( address, state ) ;
}

void notifyPanelSetOccupancy( uint16_t address, uint8_t state ) 
{
    locoNetUsb.sendSensor( address, state ) ;
}



