# loconet_parser.py

class LocoNetParser:
    GET_IDLE   = 0
    GET_OPC    = 1
    GET_LENGTH = 2
    GET_PAYLOAD = 3

    def __init__(self, message_callback, ascii_callback=None):
        self.cb = message_callback
        self.cb_ascii = ascii_callback

        self.state = self.GET_IDLE
        self.buf = []
        self.length = 0
        self.ascii_buf = ""

    def feed(self, b):
        # ----------------------------------------------------
        #  ASCII enkel wanneer idle EN het is een printbaar char
        # ----------------------------------------------------
        if self.state == self.GET_IDLE and 32 <= b <= 126:
            ch = chr(b)
            self.ascii_buf += ch
            return

        # newline → flush ASCII buffer
        if self.state == self.GET_IDLE and (b == 10 or b == 13):
            if self.ascii_buf.strip():
                if self.cb_ascii:
                    self.cb_ascii(self.ascii_buf)
            self.ascii_buf = ""
            return

        # ----------------------------------------------------
        #  OPCODE start (MSB = 1)
        # ----------------------------------------------------
        if b & 0x80:
            self.state = self.GET_OPC
            self.buf = [b]

            length_flag = (b & 0x60) >> 5
            if length_flag == 0:
                self.length = 2
                self.state = self.GET_PAYLOAD
            elif length_flag == 1:
                self.length = 4
                self.state = self.GET_PAYLOAD
            elif length_flag == 2:
                self.length = 6
                self.state = self.GET_PAYLOAD
            else:
                self.state = self.GET_LENGTH
            return

        # ----------------------------------------------------
        #  Payload handling
        # ----------------------------------------------------
        if self.state in (self.GET_LENGTH, self.GET_PAYLOAD):
            self.buf.append(b)

        if self.state == self.GET_LENGTH:
            self.length = b
            self.state = self.GET_PAYLOAD
            return

        if self.state == self.GET_PAYLOAD:
            # OPC + payload bytes?
            if len(self.buf) == self.length + 1:
                if self.cb:
                    self.cb(self.buf)
                self.state = self.GET_IDLE
            return
