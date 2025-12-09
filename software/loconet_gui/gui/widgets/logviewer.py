from PyQt6.QtWidgets import QWidget, QVBoxLayout, QTextEdit
class LogViewer(QWidget):
    def __init__(self):
        super().__init__()
        l=QVBoxLayout(self)
        self.log=QTextEdit(); self.log.setReadOnly(True)
        l.addWidget(self.log)
    def add_hex(self,data):
        self.log.append(" ".join(f"<{b:02X}>" for b in data))
    def add_text(self,t): self.log.append(t)
