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
        self.timer.start(10)   # elke 10 ms LocoNet checken
        self.LocoNet.onAccessory = self.onAccessoryEvent
        self.LocoNet.onSensor    = self.onSensorEvent

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

    def calcLnChecksum(self, data):
        chk = 0xFF
        for b in data:
            chk ^= b
        return chk

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

    def onSpeedChanged(self, value):
        self.speedDisplay.setText(str(value))
        # hier later: notify throttle, send LocoNet, etc.

    def buildThrottleTab(self):
        w = QWidget()
        layout = QVBoxLayout()

        # === SPEED DISPLAY =======================================================
        speedBox = QLabel("0")
        speedBox.setAlignment(Qt.AlignmentFlag.AlignCenter)
        speedBox.setFixedHeight(60)
        speedBox.setStyleSheet("""
            font-size: 32px;
            background: white;
            border: 2px solid black;
            border-radius: 6px;
        """)
        self.speedDisplay = speedBox
        layout.addWidget(speedBox)


        # === SPEED KNOB + DIRECTION SWITCH =======================================
        topRow = QHBoxLayout()

        # F0 button (round)
        self.btnF0 = QPushButton("F0")
        self.btnF0.setCheckable(True)
        self.btnF0.setFixedSize(50, 50)
        self.btnF0.setStyleSheet("""
            QPushButton {
                border: 2px solid black;
                border-radius: 25px;
                background: white;
                color:black;
            }
            QPushButton:checked {
                background: green;
                color: black;
            }
        """)
        topRow.addWidget(self.btnF0)


        # Speed dial (0–128)
        speedDial = QDial()
        speedDial.setRange(0, 128)
        speedDial.setNotchesVisible(True)
        speedDial.setFixedSize(150, 150)
        speedDial.valueChanged.connect(self.onSpeedChanged)
        self.speedDial = speedDial
        topRow.addWidget(speedDial)

        # Direction switch (Up/Down)
        dirFrame = QVBoxLayout()

        dirLabel = QLabel("dir")
        dirLabel.setAlignment(Qt.AlignmentFlag.AlignCenter)
        dirFrame.addWidget(dirLabel)

        self.dirSwitch = QSlider(Qt.Orientation.Vertical)
        self.dirSwitch.setMinimum(0)  # 0 = reverse, 1 = forward
        self.dirSwitch.setMaximum(1)
        self.dirSwitch.setFixedHeight(100)
        dirFrame.addWidget(self.dirSwitch)

        topRow.addLayout(dirFrame)

        layout.addLayout(topRow)


        # === RANGE LABELS UNDER THE KNOB =========================================
        rangeRow = QHBoxLayout()
        lbl0 = QLabel("0")
        lbl128 = QLabel("128")
        lbl0.setStyleSheet("font-size: 18px;")
        lbl128.setStyleSheet("font-size: 18px;")
        rangeRow.addWidget(lbl0)
        rangeRow.addStretch()
        rangeRow.addWidget(lbl128)
        layout.addLayout(rangeRow)


        # === FUNCTION BUTTON GRID F1..F35 ========================================
        funcGrid = QGridLayout()
        self.fnButtons = []

        # Styling for round buttons
        fnStyle = """
            QPushButton {
                color: black;
                border: 2px solid black;
                border-radius: 20px;
                background: white;
                font-size: 14px;
            }
            QPushButton:checked {
                background: green;
                color: black;
            }
        """

        fnNumber = 1
        for row in range(7):      # 7 rows
            for col in range(5):  # 5 columns → 7×5 = 35 buttons
                btn = QPushButton(f"F{fnNumber}")
                btn.setCheckable(True)
                btn.setFixedSize(40, 40)
                btn.setStyleSheet(fnStyle)
                funcGrid.addWidget(btn, row, col)
                self.fnButtons.append(btn)
                fnNumber += 1

        layout.addLayout(funcGrid)

        w.setLayout(layout)
        return w


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
