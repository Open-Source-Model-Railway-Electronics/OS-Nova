from PyQt6.QtWidgets import QWidget,QVBoxLayout,QSpinBox,QLabel,QComboBox,QHBoxLayout,QPushButton
from loconet_messages import build_switch_request

class AccessorySender(QWidget):
    def __init__(self):
        super().__init__()
        l=QVBoxLayout(self)
        h=QHBoxLayout(); l.addLayout(h)
        h.addWidget(QLabel("Address")); self.addr=QSpinBox(); self.addr.setRange(0,2047); h.addWidget(self.addr)
        h.addWidget(QLabel("Direction")); self.dir=QComboBox(); self.dir.addItems(["Thrown","Closed"]); h.addWidget(self.dir)
        b=QPushButton("Send Switch Request"); b.clicked.connect(self.send); l.addWidget(b)
        self.sender=None
    def set_sender(self,f): self.sender=f
    def send(self):
        if not self.sender: return
        msg=build_switch_request(self.addr.value(),1 if self.dir.currentIndex()==1 else 0)
        self.sender(msg)
