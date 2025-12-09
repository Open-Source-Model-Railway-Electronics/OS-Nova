#!/usr/bin/env python3
import sys
from PyQt6.QtWidgets import QApplication
from gui.mainwindow import MainWindow

if __name__ == "__main__":
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    sys.exit(app.exec())
