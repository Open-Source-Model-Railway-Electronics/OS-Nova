import sys
from PyQt6.QtWidgets import (
    QApplication, QWidget, QMainWindow, QTabWidget, QLabel, QLineEdit, QPushButton,
    QVBoxLayout, QHBoxLayout, QGridLayout, QSlider, QComboBox, QCheckBox,
    QScrollArea, QDial, QFrame      # ← deze moet erbij!
)
from PyQt6.QtCore import Qt, QTimer 

from loconet import LocoNetSerial

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.currentSlot = None

        self.setWindowTitle("Train-Science LocoNet Tool")
        self.resize(900, 600)

        tabs = QTabWidget()

        tabs.addTab(self.buildProgrammingTab(), "Programming")
        tabs.addTab(self.buildSensorsTab(), "Sensors")
        tabs.addTab(self.buildAccessoryTab(), "Accessories")
        tabs.addTab(self.buildThrottleTab(), "Throttle")
        tabs.addTab(self.buildGeneralTab(), "General Settings")
        tabs.addTab(self.buildThrottleSettingsTab(), "Throttle Settings")
        tabs.addTab(self.buildPanelSettingsTab(), "Control Panel")

        self.setCentralWidget(tabs)

        self.LocoNet = LocoNetSerial()
        self.timer = QTimer()
        self.timer.timeout.connect(self.pollLocoNet)
        self.timer.start(10)

        self.LocoNet.onAccessory = self.onAccessoryEvent
        self.LocoNet.onSensor    = self.onSensorEvent
        self.LocoNet.onSlotRead  = self.onSlotRead   # NEW

        self.currentSlot     = 5
        self.currentAddress  = 5
        self.acquirePending  = False
        self.dispatchAddress = None

    #-----------------------------------------------------------
    # 0) LOCONET COMMUNICATION
    #-----------------------------------------------------------
    def onAccessoryEvent(self, addr, direction, onpulse):
        index = addr - 1
        if index < 0 or index >= 32*16:
            return

        row = index // 16
        col = index % 16

        btn = self.accessoryButtons[row][col]

        # Button toggling (same as clicking)
        if direction == 1:
            # ON
            btn.setProperty("active", True)
            btn.setStyleSheet("font-size: 60px; color: cyan; border: none;")
        else:
            # OFF
            btn.setProperty("active", False)
            btn.setStyleSheet("font-size: 60px; color: gray; border: none;")

    def onSensorEvent(self, addr, state):
        index = addr - 1
        if index < 0 or index >= 32*16:
            return

        self.setSensorState(index, state == 1)


    # ---------------------------------------------------------
    # 1) PROGRAMMING TAB
    # ---------------------------------------------------------


    def updateProgrammingUi(self):
        is_pom = (self.cmbMode.currentIndex() == 1)  # 0=Service, 1=POM
        self.addrField.setEnabled(is_pom)

    def onWriteCv(self):
        try:
            cv = int(self.cvField.text())
            val = int(self.valField.text())
        except:
            print("Invalid CV/Value")
            return

        mode = self.cmbMode.currentIndex()

        if mode == 0:   # Service Mode
            self.sendServiceCvWrite(cv, val)
        else:           # POM
            try:
                addr = int(self.addrField.text())
            except:
                print("Invalid address")
                return
            self.sendPomCvWrite(addr, cv, val)

    def sendPomCvWrite(self, address, cv, value):
        # DCC PoM packet: 3 bytes CV programming
        cv_addr = cv - 1
        pkt = [
            (address >> 8) & 0x7F,
            address & 0xFF,
            0xEC | ((cv_addr >> 8) & 0x03),   # Write byte instruction
            cv_addr & 0xFF,
            value & 0xFF
        ]

        # LocoNet IMM_PACKET wrapper
        frame = [
            0xED,      # OPC_IMM_PACKET
            0x0F,      # LEN (DHI included)
            0x7F,      # DHI (standard)
            0x01,      # REPS
            *pkt
        ]

        chk = self.calcLnChecksum(frame)
        self.LocoNet.send(bytes(frame + [chk]))

        print(f"[POM CV WRITE] Addr={address} CV={cv} Value={value}")

    def sendServiceCvWrite(self, cv, value):
        cvh = ((cv >> 7) & 0x01) | ((cv >> 8) & 0x02) | ((cv >> 9) & 0x04)
        cvl = cv & 0x7F
        data7 = value & 0x7F
        cvh |= ((value >> 7) & 0x02)  # D7 bit in CVH

        frame = [
            0xEF,      # OPC_WR_SL_DATA
            0x7C,      # Slot 0x7C = programming
            cvh,
            cvl,
            data7
        ]

        chk = self.calcLnChecksum(frame)
        self.LocoNet.send(bytes(frame + [chk]))

        print(f"[SERVICE CV WRITE] CV={cv} Value={value}")

    def buildProgrammingTab(self):
        w = QWidget()
        layout = QVBoxLayout()

        # Mode selector
        modeLayout = QHBoxLayout()
        modeLayout.addWidget(QLabel("Mode:"))

        self.cmbMode = QComboBox()
        self.cmbMode.addItems(["Service Mode", "Programming on Main (PoM)"])
        modeLayout.addWidget(self.cmbMode)

        layout.addLayout(modeLayout)

        # Address (only for POM)
        self.addrField = QLineEdit()
        addrLayout = QHBoxLayout()
        addrLayout.addWidget(QLabel("Address:"))
        addrLayout.addWidget(self.addrField)
        layout.addLayout(addrLayout)

        # CV number
        cvLayout = QHBoxLayout()
        self.cvField = QLineEdit()
        cvLayout.addWidget(QLabel("CV Number:"))
        cvLayout.addWidget(self.cvField)
        layout.addLayout(cvLayout)

        # Value
        valLayout = QHBoxLayout()
        self.valField = QLineEdit()
        valLayout.addWidget(QLabel("Value:"))
        valLayout.addWidget(self.valField)
        layout.addLayout(valLayout)

        # Write button
        self.btnWrite = QPushButton("Write CV")
        self.btnWrite.clicked.connect(self.onWriteCv)
        self.cmbMode.currentIndexChanged.connect(self.updateProgrammingUi)
        self.updateProgrammingUi()
        layout.addWidget(self.btnWrite)

        layout.addStretch()
        w.setLayout(layout)
        return w

    # ---------------------------------------------------------
    # 2) SENSORS TAB
    # ---------------------------------------------------------

    def buildSensorsTab(self):
        w = QWidget()
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Sensor Feedback (32 rows × 16 sensors)"))

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)

        container = QWidget()
        grid = QGridLayout()

        self.sensorLeds = []  # 2D list: sensorLeds[row][col]

        for row in range(32):

            # 1. Rownummer berekenen
            # Elke rij begint met sensorindex = row*16 + 1
            rowNumber = row * 16 + 1

            rowLabel = QLabel(str(rowNumber))
            rowLabel.setStyleSheet("font-size: 14px; color: white;")
            grid.addWidget(rowLabel, row, 0)

            rowLeds = []
            for col in range(16):
                lbl = QLabel("●")
                lbl.setStyleSheet("font-size: 60px; color: gray;")
                grid.addWidget(lbl, row, col + 1)  # +1 want kolom 0 is het label
                rowLeds.append(lbl)

            self.sensorLeds.append(rowLeds)

        container.setLayout(grid)
        scroll.setWidget(container)

        layout.addWidget(scroll)
        w.setLayout(layout)
        return w
    
    def setSensorState(self, index: int, occupied: bool):
        if index < 0 or index >= 32 * 16:
            return

        row = index // 16
        col = index % 16

        lbl = self.sensorLeds[row][col]
        if occupied:
            lbl.setStyleSheet("font-size: 60px; color: cyan;")
        else:
            lbl.setStyleSheet("font-size: 60px; color: gray;")

    # ---------------------------------------------------------
    # 2.1  ACCESSORY TAB
    # ---------------------------------------------------------
    def buildAccessoryTab(self):
        w = QWidget()
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Accessories (32 rows × 16 per row)"))

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)

        container = QWidget()
        grid = QGridLayout()

        self.accessoryButtons = []  # 2D list

        for row in range(32):
            rowButtons = []

            # Rownumber left
            rowNumber = row * 16 + 1
            rowLabel = QLabel(str(rowNumber))
            rowLabel.setStyleSheet("font-size: 14px; color: white;")
            grid.addWidget(rowLabel, row, 0)

            for col in range(16):
                btn = QPushButton("●")
                btn.setFixedSize(40, 40)
                btn.setStyleSheet("""
                    QPushButton {
                        font-size: 60px;
                        color: gray;
                        border: none;
                    }
                """)
                btn.setProperty("active", False)

                # Store coordinates in button so the click handler knows which accessory
                btn.row = row
                btn.col = col
                btn.index = row * 16 + col

                btn.clicked.connect(self.onAccessoryClicked)

                grid.addWidget(btn, row, col + 1)
                rowButtons.append(btn)

            self.accessoryButtons.append(rowButtons)

        container.setLayout(grid)
        grid.setContentsMargins(40, 0, 0, 0)    # ← juiste regel, juiste plek
        scroll.setWidget(container)

        layout.addWidget(scroll)                # ← deze MOET blijven bestaan
        w.setLayout(layout)
        return w
    
    def onAccessoryClicked(self,):
        btn = self.sender()
        idx = btn.index

        # Toggle visual state
        if btn.property("active"):
            btn.setProperty("active", False)
            btn.setStyleSheet("font-size: 60px; color: gray; border: none;")
            newState = 0
        else:
            btn.setProperty("active", True)
            btn.setStyleSheet("font-size: 60px; color: cyan; border: none;")
            newState = 1

        # SEND COMMAND TO LOCONET
        # Convert index → accessory address
        address = idx + 1    # or +0 depending on your numbering

        print(f"Accessory {address} toggled to", newState)

       # address is GUI number 1..32 → LocoNet uses 0..31
        ln_addr = address 

        sw1 = ln_addr & 0x7F
        sw2 = (ln_addr >> 7) & 0x0F

        # direction = thrown/closed
        if newState == 1:
            sw2 |= 0x04      # DIR bit
        sw2 |= 0x02          # ON pulse

        chk = (0xB0 ^ sw1 ^ sw2 ^ 0xFF) & 0xFF
        self.LocoNet.send(bytes([0xB0, sw1, sw2, chk]))


    # ---------------------------------------------------------
    # 3) THROTTLE TAB
    # ---------------------------------------------------------
    def ln_checksum(self, data: list[int]) -> int:
        chk = 0xFF
        for b in data:
            chk ^= (b & 0xFF)
        return chk & 0xFF

    def buildThrottleTab(self):
        w = QWidget()
        layout = QVBoxLayout(w)
        layout.setSpacing(10)
        layout.setContentsMargins(10, 10, 10, 10)

        # --- Address row ---
        addrRow = QHBoxLayout()
        addrRow.addWidget(QLabel("Address:"))

        self.edtThrottleAddr = QLineEdit()
        self.edtThrottleAddr.setFixedWidth(80)
        addrRow.addWidget(self.edtThrottleAddr)

        self.lblSlotInfo = QLabel("Slot: - / Status: -")
        addrRow.addWidget(self.lblSlotInfo)

        self.btnAcquire = QPushButton("Acquire")
        self.btnAcquire.clicked.connect(self.onAcquireClicked)
        addrRow.addWidget(self.btnAcquire)

        self.btnDispatch = QPushButton("Dispatch")
        self.btnDispatch.clicked.connect(self.onDispatchClicked)
        addrRow.addWidget(self.btnDispatch)

        self.btnGetAddress = QPushButton("Get address")
        self.btnGetAddress.clicked.connect(self.onGetAddressClicked)
        addrRow.addWidget(self.btnGetAddress)

        addrRow.addStretch()
        layout.addLayout(addrRow)

        # --- Speed display ---
        self.speedDisplay = QLabel("0")
        self.speedDisplay.setFixedSize(90, 40)
        self.speedDisplay.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.speedDisplay.setStyleSheet(
            "font-size: 32px; background: white; color: black; border: 2px solid black;"
        )

        speedRow = QHBoxLayout()
        speedRow.addStretch()
        speedRow.addWidget(self.speedDisplay)
        speedRow.addStretch()
        layout.addLayout(speedRow)

        # --- Controls row ---
        topRow = QHBoxLayout()
        topRow.setSpacing(12)

        self.btnF0 = QPushButton("💡")
        self.btnF0.setCheckable(True)
        self.btnF0.setFixedSize(60, 60)
        self.btnF0.setStyleSheet(
            "font-size: 26px; border: 2px solid black; border-radius: 30px; background: white;"
        )
        self.btnF0.toggled.connect(self.onF0Toggled)
        topRow.addWidget(self.btnF0)

        self.speedSlider = QSlider(Qt.Orientation.Horizontal)
        self.speedSlider.setRange(0, 127)
        self.speedSlider.setFixedHeight(40)
        self.speedSlider.valueChanged.connect(self.onSpeedChanged)
        topRow.addWidget(self.speedSlider, stretch=1)

        self.btnDir = QPushButton("▶")
        self.btnDir.setCheckable(True)
        self.btnDir.setFixedSize(80, 60)
        self.btnDir.setStyleSheet(
            "font-size: 28px; font-weight: bold; background: #ddd; border: 2px solid black;"
        )
        self.btnDir.toggled.connect(self.onDirectionChanged)
        topRow.addWidget(self.btnDir)

        layout.addLayout(topRow)
        layout.addSpacing(20)

        # --- Function buttons F1–F28 ---
        funcGrid = QGridLayout()
        funcGrid.setHorizontalSpacing(6)
        funcGrid.setVerticalSpacing(6)

        self.fnButtons = []

        fn = 1
        for row in range(4):
            for col in range(7):
                if fn > 28:
                    break
                btn = QPushButton(f"F{fn}")
                btn.setCheckable(True)
                btn.setFixedSize(40, 40)
                btn.setStyleSheet("""
                    QPushButton {
                        background: white;
                        color: black;
                        border: 2px solid black;
                        border-radius: 20px;
                        font-size: 14px;
                    }
                    QPushButton:checked {
                        background: green;
                        color: black;
                    }
                """)
                btn.toggled.connect(lambda checked, n=fn: self.onFnButtonToggled(n, checked))
                funcGrid.addWidget(btn, row, col)
                self.fnButtons.append(btn)
                fn += 1

        funcRow = QHBoxLayout()
        funcRow.addStretch()
        funcRow.addLayout(funcGrid)
        funcRow.addStretch()
        layout.addLayout(funcRow)

        layout.addStretch()
        return w

    # ---------------------------------------------------------
    # THROTTLE LOGIC
    # ---------------------------------------------------------

    def onGetAddressClicked(self):
        txt = self.edtThrottleAddr.text().strip()
        try:
            addr = int(txt)
        except ValueError:
            return

        if addr <= 0 or addr > 10239:
            return

        # --- split address per LocoNet spec ---
        adr_lo = addr & 0x7F
        adr_hi = (addr >> 7) & 0x7F

        frame = [
            0xBF,        # OPC_LOCO_ADR
            adr_hi,
            adr_lo
        ]

        chk = self.ln_checksum(frame)
        self.LocoNet.send(bytes(frame + [chk]))

        # GUI state
        self.acquirePending = True
        self.dispatchAddress = None

        print(f"[THROTTLE] Get address request sent for addr {addr}")


    def sendExpandedFunctions(self):
        if self.currentSlot is None:
            return

        slot = self.currentSlot & 0x7F

        # --- bouw w8 (F13–F19) ---
        w8 = 0
        for i in range(7):  # F13..F19
            if self.fnButtons[12 + i].isChecked():
                w8 |= (1 << i)

        # --- bouw w9 (F21–F27) ---
        w9 = 0
        for i in range(7):  # F21..F27
            if self.fnButtons[20 + i].isChecked():
                w9 |= (1 << i)

        # --- overige bytes: voorlopig kopiëren / 0 ---
        frame = [
            0xEE,          # OPC_WR_SL_DATA_EXP
            0x15,          # LEN = 21 bytes totaal
            0x01,          # MAST# (meestal 1)
            slot,          # SLOT#
            0x00,          # w0 Status1
            self.currentAddress & 0x7F,          # w1 addr low
            (self.currentAddress >> 7) & 0x7F,   # w2 addr high
            0x00,          # w3 flags
            self.speedSlider.value() & 0x7F,        # w4 speed
            0x00,          # w5 speed steps
            0x00,          # w6 DIRF
            0x00,          # w7 F5–F11
            w8 & 0x7F,     # w8 F13–F19
            w9 & 0x7F,     # w9 F21–F27
            0x00,          # w10
            0x00,          # w11
            0x00,          # w12
            0x00,          # w13
            0x00,          # w14
            0x00           # w15
        ]

        chk = self.ln_checksum(frame)
        self.LocoNet.send(bytes(frame + [chk]))

        print(f"[LN] EXP SLOT WRITE slot={slot} w8={w8:02X} w9={w9:02X}")

    def onAcquireClicked(self):
        txt = self.edtThrottleAddr.text().strip()
        try:
            addr = int(txt)
        except ValueError:
            return
        if addr <= 0 or addr > 10239:
            return

        self.acquirePending = True
        self.LocoNet.sendLocoAdrRequest(addr)

    def onDispatchClicked(self):
        txt = self.edtThrottleAddr.text().strip()
        try:
            addr = int(txt)
        except ValueError:
            return
        self.dispatchAddress = addr
        print(f"[THROTTLE] Dispatch prepared for address {addr}")

    def onSlotRead(self, slot, stat1, addr, speed, dirf, snd):
        active_bits = (stat1 >> 4) & 0x03
        if   active_bits == 0b11: status = "IN_USE"
        elif active_bits == 0b10: status = "IDLE"
        elif active_bits == 0b01: status = "COMMON"
        else:                     status = "FREE"

        self.lblSlotInfo.setText(f"Slot: {slot} / {status}")

        txt = self.edtThrottleAddr.text().strip()
        try:
            requested = int(txt)
        except ValueError:
            requested = None

        if requested is not None and requested == addr:
            self.currentSlot = slot

        # auto acquire on IDLE/COMMON once
        if self.acquirePending and status in ("IDLE", "COMMON"):
            self.LocoNet.sendMoveSlots(slot, slot)
            self.acquirePending = False

        self.currentSlot = slot
        self.currentAddress = addr

        # update UI from slot data
        self.speedSlider.blockSignals(True)
        self.speedSlider.setValue(speed)
        self.speedSlider.blockSignals(False)
        self.speedDisplay.setText(str(speed))

        dir_val = 1 if (dirf & 0x20) else 0
        self.btnDir.blockSignals(True)
        self.btnDir.setChecked(bool(dir_val))
        self.btnDir.blockSignals(False)

        # F0
        self.btnF0.blockSignals(True)
        self.btnF0.setChecked(bool(dirf & 0x10))
        self.btnF0.blockSignals(False)

        # F1..F4 from DIRF bits 0..3
        for i in range(4):
            self.fnButtons[i].blockSignals(True)
            self.fnButtons[i].setChecked(bool(dirf & (1 << i)))
            self.fnButtons[i].blockSignals(False)

        # F5..F8 from SND low nibble
        for i in range(4):
            self.fnButtons[4 + i].blockSignals(True)
            self.fnButtons[4 + i].setChecked(bool(snd & (1 << i)))
            self.fnButtons[4 + i].blockSignals(False)

        # F9..F12 from SND high nibble
        upper = (snd >> 4) & 0x0F
        for i in range(4):
            self.fnButtons[8 + i].blockSignals(True)
            self.fnButtons[8 + i].setChecked(bool(upper & (1 << i)))
            self.fnButtons[8 + i].blockSignals(False)

    def onSpeedChanged(self, value):
        self.speedDisplay.setText(str(value))
        print( 'hello bro ')

        self.speedDisplay.setText(str(value))
        if not isinstance(self.currentSlot, int):
            return

        speed = value & 0x7F

        chk = (0xA0 ^ self.currentSlot ^ speed ^ 0xFF) & 0xFF
        self.LocoNet.send(bytes([
            0xA0,
            self.currentSlot,
            speed,
            chk
        ]))



    def onDirectionChanged(self, checked):
        if self.currentSlot is None:
            return
        self.sendF0F4DirPacket()


    def onF0Toggled(self, checked):
        if self.currentSlot is None:
            return
        self.sendF0F4DirPacket()

    # ----- helpers for function packets ---------------------------------
    def sendF0F4DirPacket(self):
        if self.currentSlot is None:
            return
        dir_val = 1 if self.btnDir.isChecked() else 0
        f0 = self.btnF0.isChecked()
        f1 = self.fnButtons[0].isChecked()
        f2 = self.fnButtons[1].isChecked()
        f3 = self.fnButtons[2].isChecked()
        f4 = self.fnButtons[3].isChecked()

        dirf = 0
        if dir_val:
            dirf |= 0x20
        if f0:
            dirf |= (1 << 4)
        if f1:
            dirf |= (1 << 0)
        if f2:
            dirf |= (1 << 1)
        if f3:
            dirf |= (1 << 2)
        if f4:
            dirf |= (1 << 3)

        self.LocoNet.sendLocoDirF0F4(self.currentSlot, dirf)

    def sendF5F8Packet(self):
        if self.currentSlot is None:
            return

        snd = 0
        if self.fnButtons[4].isChecked(): snd |= (1 << 0)  # F5
        if self.fnButtons[5].isChecked(): snd |= (1 << 1)  # F6
        if self.fnButtons[6].isChecked(): snd |= (1 << 2)  # F7
        if self.fnButtons[7].isChecked(): snd |= (1 << 3)  # F8

        self.LocoNet.sendLocoF5F8(self.currentSlot, snd)

    def sendF9F12Packet(self):
        if self.currentSlot is None:
            return

        f = 0
        if self.fnButtons[8].isChecked():  f |= (1 << 0)  # F9
        if self.fnButtons[9].isChecked():  f |= (1 << 1)  # F10
        if self.fnButtons[10].isChecked(): f |= (1 << 2)  # F11
        if self.fnButtons[11].isChecked(): f |= (1 << 3)  # F12

        self.LocoNet.sendLocoF9F12(self.currentSlot, f)

            
    def onFnButtonToggled(self, n: int, checked: bool):
        if self.currentSlot is None:
            return

        if 1 <= n <= 4:
            self.sendF0F4DirPacket()
        elif 5 <= n <= 8:
            self.sendF5F8Packet()
        elif 9 <= n <= 12:
            self.sendF9F12Packet()
        elif n >= 13:
            self.sendExpandedFunctions()




    # ---------------------------------------------------------
    # 4) GENERAL SETTINGS TAB
    # ---------------------------------------------------------

    def buildGeneralTab(self):
        w = QWidget()
        layout = QVBoxLayout()

        # Serial port
        portLayout = QHBoxLayout()
        self.cmbPort = QComboBox()
        self.cmbPort.addItems(["COM1", "COM2", "COM3"])
        portLayout.addWidget(QLabel("Serial Port:"))
        portLayout.addWidget(self.cmbPort)

        layout.addLayout(portLayout)

        # Baudrate
        baudLayout = QHBoxLayout()
        self.cmbBaud = QComboBox()
        self.cmbBaud.addItems(["57600", "115200"])
        baudLayout.addWidget(QLabel("Baudrate:"))
        baudLayout.addWidget(self.cmbBaud)
        layout.addLayout(baudLayout)

        # Debug checkbox
        self.chkDebug = QCheckBox("Enable LocoNet Debug Logging")
        layout.addWidget(self.chkDebug)

        layout.addStretch()
        w.setLayout(layout)
        return w

    # ---------------------------------------------------------
    # 5) THROTTLE SETTINGS TAB
    # ---------------------------------------------------------

    def buildThrottleSettingsTab(self):
        w = QWidget()
        layout = QVBoxLayout()

        self.cmbSpeedSteps = QComboBox()
        self.cmbSpeedSteps.addItems(["14", "28", "128"])

        speedStepLayout = QHBoxLayout()
        speedStepLayout.addWidget(QLabel("Default Speed Steps:"))
        speedStepLayout.addWidget(self.cmbSpeedSteps)
        layout.addLayout(speedStepLayout)

        self.chkAutoRelease = QCheckBox("Auto-release slot when idle")
        layout.addWidget(self.chkAutoRelease)

        layout.addStretch()
        w.setLayout(layout)
        return w

    # ---------------------------------------------------------
    # 6) CONTROL PANEL SETTINGS TAB
    # ---------------------------------------------------------

    def buildPanelSettingsTab(self):
        w = QWidget()
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Control Panel Settings"))

        nameLayout = QHBoxLayout()
        nameLayout.addWidget(QLabel("Panel Name:"))
        self.panelName = QLineEdit()
        nameLayout.addWidget(self.panelName)
        layout.addLayout(nameLayout)

        layout.addStretch()
        w.setLayout(layout)
        return w
    
    def pollLocoNet(self):
        self.LocoNet.read()


# ---------------------------------------------------------
# MAIN EXECUTION
# ---------------------------------------------------------

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())
