# -----------------------------
#  LocoNet Message Builders
# -----------------------------

def checksum(opc, payload):
    """Calculate LocoNet XOR checksum."""
    c = 0xFF ^ opc
    for b in payload:
        c ^= b
    return c


# -----------------------------
#  ACCESSORY COMMAND (OPC_SW_REQ)
# -----------------------------
def build_switch_request(addr, direction, on=True):
    """
    Build a LocoNet switch request (accessory command).
    addr: 11-bit address (0–2047)
    direction: 0=Thrown, 1=Closed
    on: always True for Digitrax ON-pulse
    """
    sw1 = addr & 0x7F
    sw2 = (addr >> 7) & 0x0F

    if direction:
        sw2 |= 0x04    # direction bit

    if on:
        sw2 |= 0x02    # ON pulse

    opc = 0xB0
    payload = [sw1, sw2]
    chk = checksum(opc, payload)

    return bytes([opc] + payload + [chk])


# -----------------------------
#  SENSOR REPORT (OPC_INPUT_REP)
# -----------------------------
def build_sensor_report(addr, state):
    """
    Build a LocoNet sensor/feedback report.
    addr: 11-bit address
    state: 0=off, 1=on (bit5)
    """
    sn1 = addr & 0x7F
    sn2 = (addr >> 7) & 0x0F

    if state:
        sn2 |= 0x20    # occupancy bit

    opc = 0xB2
    payload = [sn1, sn2]
    chk = checksum(opc, payload)

    return bytes([opc] + payload + [chk])


# -----------------------------
#  LOCO SPEED (OPC_LOCO_SPD)
# -----------------------------
def build_loco_speed(slot, speed):
    """
    Build OPC_LOCO_SPD (A0).
    speed: 0–127
    """
    opc = 0xA0
    payload = [slot, speed & 0x7F]
    chk = checksum(opc, payload)

    return bytes([opc] + payload + [chk])


# -----------------------------
#  LOCO DIR + F0..F4 (OPC_LOCO_DIRF)
# -----------------------------
def build_loco_dirf(slot, direction, f0, f1, f2, f3, f4):
    """
    Build OPC_LOCO_DIRF (A1).
    direction: 0 = reverse, 1 = forward
    F0..F4 = booleans
    """
    dirf = 0

    if direction:
        dirf |= 0x20  # direction bit

    # F-bits (Digitrax bit order)
    dirf |= (1 if f0 else 0) << 4
    dirf |= (1 if f4 else 0) << 3
    dirf |= (1 if f3 else 0) << 2
    dirf |= (1 if f2 else 0) << 1
    dirf |= (1 if f1 else 0)

    opc = 0xA1
    payload = [slot, dirf]
    chk = checksum(opc, payload)

    return bytes([opc] + payload + [chk])


# -----------------------------
#  LOCO F5..F8 (OPC_LOCO_SND)
# -----------------------------
def build_loco_f5_f8(slot, f5, f6, f7, f8):
    """
    Build OPC_LOCO_SND (A2).
    """
    snd = (
        (1 if f8 else 0) << 3 |
        (1 if f7 else 0) << 2 |
        (1 if f6 else 0) << 1 |
        (1 if f5 else 0)
    )

    opc = 0xA2
    payload = [slot, snd]
    chk = checksum(opc, payload)

    return bytes([opc] + payload + [chk])


# -----------------------------
#  LOCO F9..F12 (OPC_LOCO_F9_F12)
# -----------------------------
def build_loco_f9_f12(slot, f9, f10, f11, f12):
    """
    Build OPC_LOCO_F9_F12 (A3).
    """
    val = (
        (1 if f12 else 0) << 3 |
        (1 if f11 else 0) << 2 |
        (1 if f10 else 0) << 1 |
        (1 if f9 else 0)
    )

    opc = 0xA3
    payload = [slot, val]
    chk = checksum(opc, payload)

    return bytes([opc] + payload + [chk])


# -----------------------------
#  SLOT REQUEST (RQ_SL_DATA)
# -----------------------------
def build_slot_request(slot):
    """
    Sends <BB> <slot> <00> <chk>
    """
    opc = 0xBB
    payload = [slot, 0x00]
    chk = checksum(opc, payload)

    return bytes([opc] + payload + [chk])



# -----------------------------
#  SLOT MOVE (OPC_MOVE_SLOTS)
# -----------------------------
def build_slot_move(src, dest):
    """
    Build OPC_MOVE_SLOTS (0xBA).
    src: source slot number
    dest: destination slot number
    """
    opc = 0xBA
    payload = [src, dest]
    chk = checksum(opc, payload)
    return bytes([opc] + payload + [chk])


# -----------------------------
#  LOCO ADDRESS REQUEST (OPC_LOCO_ADR / 0xBF)
# -----------------------------
def build_loco_adr(address):
    """
    Build OPC_LOCO_ADR (0xBF)
    Address is a 14-bit DCC address.
    """
    adr_hi = (address >> 7) & 0x7F
    adr_lo = address & 0x7F

    opc = 0xBF
    payload = [adr_hi, adr_lo]
    chk = checksum(opc, payload)

    return bytes([opc] + payload + [chk])
