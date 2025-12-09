from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QTabWidget, QHBoxLayout,
    QLabel, QComboBox, QPushButton
)

import serial.tools.list_ports

from serial_manager import SerialManager
from loconet_parser import LocoNetParser

# Widgets
from gui.widgets.logviewer import LogViewer
from gui.widgets.accessorysender import AccessorySender
from gui.widgets.sendersender import SensorSender
from gui.widgets.locosender import LocoSender
from gui.widgets.slotviewer import SlotViewer


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("LocoNet USB Test GUI")

        self.ascii_buffer = ""

        main = QWidget()
        layout = QVBoxLayout(main)

        # -----------------------------
        # Serial Port Selection Bar
        # -----------------------------
        bar = QHBoxLayout()
        bar.addWidget(QLabel("COM Port"))
        self.portlist = QComboBox()
        self.refresh_ports()
        bar.addWidget(self.portlist)

        bar.addWidget(QLabel("Baud"))
        self.baud = QComboBox()
        self.baud.addItems(["16600", "19200", "57600", "115200"])
        self.baud.setCurrentText("115200")
        bar.addWidget(self.baud)

        self.btn_connect = QPushButton("Connect")
        self.btn_connect.clicked.connect(self.connect)
        bar.addWidget(self.btn_connect)

        layout.addLayout(bar)

        # -----------------------------
        # Tabs
        # -----------------------------
        tabs = QTabWidget()
        self.log = LogViewer()
        self.acc = AccessorySender()
        self.sens = SensorSender()
        self.loco = LocoSender()
        self.slot = SlotViewer()

        tabs.addTab(self.log, "Monitor")
        tabs.addTab(self.acc, "Accessory")
        tabs.addTab(self.sens, "Sensor")
        tabs.addTab(self.loco, "Locomotive")
        tabs.addTab(self.slot, "Slots")

        layout.addWidget(tabs)
        self.setCentralWidget(main)

        # Serial + parser
        self.serial = None
        self.parser = LocoNetParser(
            message_callback=self.on_message,
            ascii_callback=self.on_ascii
        )

    def on_ascii(self, line):
        self.log.add_text(line)

    # -----------------------------
    # Serial Port Handling
    # -----------------------------
    def refresh_ports(self):
        self.portlist.clear()
        for p in serial.tools.list_ports.comports():
            self.portlist.addItem(p.device)

    def connect(self):
        """Connect or disconnect from serial port."""
        if self.serial is None:
            port = self.portlist.currentText()
            baud = int(self.baud.currentText())

            self.serial = SerialManager(port, baud, self.parser, self.on_ascii)
            self.serial.start()
            self.btn_connect.setText("Disconnect")

            # Connect GUI senders
            self.acc.set_sender(self.serial.write)
            self.sens.set_sender(self.serial.write)
            self.loco.set_sender(self.serial.write)
            self.slot.set_sender(self.serial.write)

        else:
            self.serial.stop()
            self.serial = None
            self.btn_connect.setText("Connect")

    # -----------------------------
    # ASCII Input Handler
    # -----------------------------
    def on_ascii(self, ch):
        """Collect ASCII characters into lines, flush on newline."""
        if ch == "\n" or ch == "\r":
            if self.ascii_buffer.strip() != "":
                self.log.add_text(self.ascii_buffer)
            self.ascii_buffer = ""
        else:
            self.ascii_buffer += ch

    # -----------------------------
    # LocoNet Message Handler
    # -----------------------------
    def on_message(self, msg):
        """Incoming parsed LocoNet frame."""
        # Hex dump
        self.log.add_hex(msg)
        # Parsed line
        self.log.add_text(self.pretty(msg))

    # -----------------------------
    # Message Pretty Printer
    # -----------------------------
    def pretty(self, msg):
        opc = msg[0]

        # ---- SWITCH REQUEST ----
        if opc == 0xB0:
            sw1 = msg[1]
            sw2 = msg[2]
            addr = ((sw2 & 0x0F) << 7) | (sw1 & 0x7F)
            direction = 1 if (sw2 & 0x04) else 0
            return f"OPC_SW_REQ addr={addr} dir={direction}"

        # ---- SENSOR REPORT ----
        if opc == 0xB2:
            sn1 = msg[1]
            sn2 = msg[2]
            addr = ((sn2 & 0x0F) << 7) | (sn1 & 0x7F)
            state = 1 if (sn2 & 0x20) else 0
            return f"OPC_INPUT_REP addr={addr} state={state}"

        # ---- LOCO SPEED ----
        if opc == 0xA0:
            slot = msg[1]
            speed = msg[2] & 0x7F
            return f"OPC_LOCO_SPD slot={slot} speed={speed}"

        # ---- DIRF ----
        if opc == 0xA1:
            slot = msg[1]
            dirf = msg[2]
            direction = 1 if (dirf & 0x20) else 0
            return f"OPC_LOCO_DIRF slot={slot} dir={direction}"

        # ---- F9-F12 ----
        if opc == 0xA3:
            slot = msg[1]
            return f"OPC_LOCO_F9_F12 slot={slot}"

        # ---- SLOT MOVE ----
        if opc == 0xBA:
            src = msg[1]
            dest = msg[2]
            return f"OPC_MOVE_SLOTS src={src} dest={dest}"

        # ---- LOCO ADR ----
        if opc == 0xBF:
            adr_hi = msg[1]
            adr_lo = msg[2]
            adr = (adr_hi << 7) | adr_lo
            return f"OPC_LOCO_ADR request addr={adr}"

        # ---- SLOT REQUEST ----
        if opc == 0xBB:
            sl = msg[1]
            return f"OPC_RQ_SL_DATA slot={sl}"

        # ---- SLOT DATA SHORT ----
        if opc == 0xE7:
            return "OPC_SL_RD_DATA (short slot read)"

        # ---- SLOT DATA EXP ----
        if opc == 0xE6:
            return "OPC_SL_RD_DATA_EXP (expanded slot read)"

        return f"OPC=0x{opc:02X} (unhandled)"
