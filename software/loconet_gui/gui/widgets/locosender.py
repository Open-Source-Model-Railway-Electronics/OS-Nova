from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QLabel, QSpinBox, QHBoxLayout,
    QPushButton, QSlider, QGridLayout, QCheckBox
)
from PyQt6.QtCore import Qt

from loconet_messages import build_loco_speed, build_loco_dirf

class LocoSender(QWidget):
    def __init__(self):
        super().__init__()
        layout = QVBoxLayout(self)

        # Slot
        sl = QHBoxLayout()
        sl.addWidget(QLabel("Slot"))
        self.slot = QSpinBox()
        self.slot.setRange(0, 120)
        sl.addWidget(self.slot)
        layout.addLayout(sl)

        # Speed
        sbl = QLabel("Speed")
        layout.addWidget(sbl)

        self.speed = QSlider(Qt.Orientation.Horizontal)
        self.speed.setRange(0, 127)
        layout.addWidget(self.speed)

        speed_btn = QPushButton("Send Speed")
        speed_btn.clicked.connect(self.send_speed)
        layout.addWidget(speed_btn)

        # Direction + F0–F4
        hl = QHBoxLayout()
        self.dir = QCheckBox("Direction FWD")
        hl.addWidget(self.dir)
        layout.addLayout(hl)

        # F0–F4
        gl = QGridLayout()
        self.f = []
        for i in range(5):
            cb = QCheckBox(f"F{i}")
            self.f.append(cb)
            gl.addWidget(cb, 0, i)
        layout.addLayout(gl)

        dfbtn = QPushButton("Send DIRF")
        dfbtn.clicked.connect(self.send_dirf)
        layout.addWidget(dfbtn)

        self.sender = None

    def set_sender(self, fn):
        self.sender = fn

    def send_speed(self):
        if not self.sender: return
        msg = build_loco_speed(self.slot.value(), self.speed.value())
        self.sender(msg)

    def send_dirf(self):
        if not self.sender: return
        f0 = self.f[0].isChecked()
        f1 = self.f[1].isChecked()
        f2 = self.f[2].isChecked()
        f3 = self.f[3].isChecked()
        f4 = self.f[4].isChecked()
        msg = build_loco_dirf(self.slot.value(), self.dir.isChecked(), f0,f1,f2,f3,f4)
        self.sender(msg)
