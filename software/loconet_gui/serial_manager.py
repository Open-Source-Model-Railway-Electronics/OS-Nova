import threading
import time
import serial


class SerialManager:
    def __init__(self, port, baud, parser, ascii_callback=None):
        self.port = port
        self.baud = baud
        self.parser = parser
        self.ascii_callback = ascii_callback

        self.running = False
        self.ser = None
        self.thread = None

    def start(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0)
        except Exception as e:
            print("Serial open error:", e)
            return

        self.running = True
        self.thread = threading.Thread(target=self._loop, daemon=True)
        self.thread.start()

    def stop(self):
        self.running = False
        time.sleep(0.05)

        if self.ser:
            try:
                self.ser.close()
            except:
                pass
            self.ser = None

        self.thread = None

    def _loop(self):
        """Background reader thread."""
        while self.running:
            if self.ser and self.ser.in_waiting:
                try:
                    b = self.ser.read(1)
                    if not b:
                        continue
                    v = b[0]

                    # ASCII 32–126
                    if 32 <= v <= 126:
                        if self.ascii_callback:
                            self.ascii_callback(chr(v))
                        continue

                    # LocoNet
                    self.parser.feed(v)

                except:
                    break

            time.sleep(0.001)

    def write(self, data: bytes):
        """Send bytes to the serial port."""
        if self.ser:
            try:
                self.ser.write(data)
            except:
                pass
