from PyQt6.QtWidgets import QWidget, QVBoxLayout, QSpinBox, QLabel, QComboBox, QHBoxLayout, QPushButton
from loconet_messages import build_sensor_report

class SensorSender(QWidget):
    def __init__(self):
        super().__init__()
        layout = QVBoxLayout(self)

        h = QHBoxLayout()
        layout.addLayout(h)

        h.addWidget(QLabel("Address"))
        self.addr = QSpinBox()
        self.addr.setRange(0, 2047)
        h.addWidget(self.addr)

        h.addWidget(QLabel("State"))
        self.state = QComboBox()
        self.state.addItems(["LOW (0)", "HIGH (1)"])
        h.addWidget(self.state)

        send = QPushButton("Send Sensor Report")
        send.clicked.connect(self.send)
        layout.addWidget(send)

        self.sender = None

    def set_sender(self, fn):
        self.sender = fn

    def send(self):
        if not self.sender:
            return
        msg = build_sensor_report(self.addr.value(), self.state.currentIndex())
        self.sender(msg)
