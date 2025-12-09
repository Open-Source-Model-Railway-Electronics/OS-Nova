#include <Arduino.h>

// --- ACCESSORY BASIC ---
#define OPC_SW_REQ            0xB0    // Switch command (no ACK)
#define OPC_SW_REP            0xB1    // Switch feedback
#define OPC_INPUT_REP         0xB2    // Sensor input
#define OPC_SW_ACK            0xBD    // Switch with ACK
#define OPC_SW_STATE          0xBC    // Request switch state

// --- LOCO CONTROL ---
#define OPC_LOCO_SPD          0xA0    // Speed
#define OPC_LOCO_DIRF         0xA1    // Direction + F0..F4
#define OPC_LOCO_SND          0xA2    // F5..F8
#define OPC_LOCO_F9_F12       0xA3    // F9..F12
#define OPC_LOCO_F13_F19      0xA4    // F13..F19 

#define OPC_WR_SL_DATA_EXP      0xEE 
#define OPC_SL_RD_DATA_EXP      0xE6 
#define OPC_RQ_SL_DATA          0xBB
#define OPC_SL_RD_DATA          0xE7
#define OPC_WR_SL_DATA          0xEF
#define OPC_MOVE_SLOTS          0xBA
#define OPC_SLOT_STAT1          0xB4
#define OPC_LOCO_ADR            0xBF
#define OPC_IDLE                0x85

// CUSTOM
#define OPC_PEER_XFER           0xE5

// --- EXTENDED / DCC PACKETS ---
#define OPC_IMM_PACKET        0xED    // DCC Immediate Packet (EXTENDED ACCESSORY!)

#define OPC_MULTI_SENSE       0xD0    // Detection / Transponder
#define OPC_AUX_WR            0xDF
#define OPC_AUX_RD            0xD7



typedef struct 
{
    uint8_t OPCODE ;
    uint8_t length ;
    uint8_t payload[16] ; // 16 to be finalized later
} lnMessage ;

class LocoNetUSB
{
public:
    uint8_t update() ;

    void sendAccesory( uint16_t _address, uint8_t state ) ;
    void sendSensor(   uint16_t _address, uint8_t state ) ;
    void sendMessage( lnMessage* mess, uint8_t len ) ;

    void    sendLocoSpeed(     uint8_t mySlot ) ;               // NB. my own throttles will not send higher than F12, so we have no need for more transmitt functions.
    void    sendLocoDirF0toF4( uint8_t mySlot ) ;
    void    sendLocoF5toF8(    uint8_t mySlot ) ;
    void    sendLocoF9toF12(   uint8_t mySlot ) ;

    void    writeSlotData( uint8_t ) ;
    void    readSlotData( uint8_t ) ;
    void    readSlotDataExp( uint8_t ) ;
    
    void    sendConfiguration() ;

private:
    uint8_t     state ;
    uint8_t     byteCounter ;
    uint16_t    dispatchAddres ;

    lnMessage message ;
    lnMessage message2send ;

    // Incomming commands are processed
    void processSwitchRequest() ;
    void processDccExtRequest() ;
    void processOpcode() ;
    void processSlotRequest() ;          // OPC_RQ_SL_DATA (0xBB)
    void processSlotReadData() ;         // OPC_SL_RD_DATA (0xE7)
    void processSlotReadDataExp() ;      // OPC_SL_RD_DATA_EXP (0xE6)
    void processWriteSlotData() ;        // OPC_WR_SL_DATA (0xEF)
    void processWriteSlotDataExp() ;     // OPC_WR_SL_DATA_EXP (0xEE)
    void processMoveSlots() ;            // OPC_MOVE_SLOTS (0xBA)
    void processSlotStatus1() ;          // OPC_SLOT_STAT1 (0xB4)
    void processLocoAdrRequest() ;       // OPC_LOCO_ADR (0xBF)
    void processIdleEverywhere() ;       // OPC_IDLE (0x85)    
    void processLocoAddressRequest() ;
    void processLocoSpeed() ;
    void processLocoDirF0toF4() ;
    void processLocoF5toF8() ;
    void processLocoF9toF12() ;
    void processLocoF13toF19() ;

    void processConfiguration() ;

} ;

extern uint8_t notifyLnetAccessory(    uint16_t address, uint8_t direction          ) __attribute__((weak)) ;
extern uint8_t notifyLnetSensor(       uint16_t address, uint8_t state              ) __attribute__((weak)) ;
extern uint8_t notifyLnetAccessoryExt( uint16_t address, uint8_t value              ) __attribute__((weak)) ;

extern uint8_t notifyLnetLocoSpeed(     uint8_t slot,    uint8_t speed              ) __attribute__((weak)) ;
extern uint8_t notifyLnetLocoDirection( uint8_t slot,    uint8_t dir                ) __attribute__((weak)) ;
extern uint8_t notifyLnetLocoFunction(  uint8_t slot,    uint8_t fn, uint8_t value  ) __attribute__((weak)) ;

extern void setConfiguration( lnMessage* ) __attribute__((weak)) ;
extern void setPanelButton(   lnMessage* ) __attribute__((weak)) ;
extern void setRoute(         lnMessage* ) __attribute__((weak)) ;
extern void setThrottle(      lnMessage* ) __attribute__((weak)) ;

extern void getConfiguration() __attribute__((weak));
extern void getPanelButton  ( uint8_t ) __attribute__((weak)); // button ID. NOTE I may want to send ALL buttons in one massive blocking loop :-D
extern void getRoute        ( uint8_t ) __attribute__((weak)); // route ID
extern void getThrottle     ( uint8_t ) __attribute__((weak)); // throttle ID

extern LocoNetUSB locoNetUsb ;