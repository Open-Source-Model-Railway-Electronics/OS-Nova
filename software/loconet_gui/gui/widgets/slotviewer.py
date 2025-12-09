from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QLabel, QSpinBox, QPushButton,
    QTextEdit, QHBoxLayout, QGroupBox
)

from loconet_messages import (
    checksum,
    build_slot_request,
    build_slot_move,
    build_loco_adr
)


class SlotViewer(QWidget):
    def __init__(self):
        super().__init__()
        layout = QVBoxLayout(self)

        #
        # --- SLOT REQUEST ---
        #
        gb_req = QGroupBox("Slot Read (RQ_SL_DATA 0xBB)")
        req_lay = QVBoxLayout(gb_req)

        hl = QHBoxLayout()
        hl.addWidget(QLabel("Slot #"))
        self.req_slot = QSpinBox()
        self.req_slot.setRange(0, 120)
        hl.addWidget(self.req_slot)

        btn = QPushButton("Send Slot Request")
        btn.clicked.connect(self.do_slot_request)
        req_lay.addLayout(hl)
        req_lay.addWidget(btn)

        layout.addWidget(gb_req)

        #
        # --- SLOT MOVE ---
        #
        gb_mv = QGroupBox("Slot Move (0xBA)")
        mv_lay = QVBoxLayout(gb_mv)

        mvh = QHBoxLayout()
        mvh.addWidget(QLabel("SRC"))
        self.move_src = QSpinBox()
        self.move_src.setRange(0, 120)
        mvh.addWidget(self.move_src)

        mvh.addWidget(QLabel("DEST"))
        self.move_dest = QSpinBox()
        self.move_dest.setRange(0, 120)
        mvh.addWidget(self.move_dest)

        btn2 = QPushButton("Send Slot Move")
        btn2.clicked.connect(self.do_slot_move)
        mv_lay.addLayout(mvh)
        mv_lay.addWidget(btn2)

        layout.addWidget(gb_mv)

        #
        # --- LOCO ADR REQUEST ---
        #
        gb_adr = QGroupBox("Loco Address Request (0xBF)")
        adr_lay = QVBoxLayout(gb_adr)

        ah = QHBoxLayout()
        ah.addWidget(QLabel("Address"))
        self.loco_addr = QSpinBox()
        self.loco_addr.setRange(0, 16383)
        ah.addWidget(self.loco_addr)

        btn3 = QPushButton("Send Loco ADR Request")
        btn3.clicked.connect(self.do_loco_adr)
        adr_lay.addLayout(ah)
        adr_lay.addWidget(btn3)

        layout.addWidget(gb_adr)

        #
        # --- OUTPUT TEXT ---
        #
        self.output = QTextEdit()
        layout.addWidget(self.output)

        self.sender = None

    def set_sender(self, fn):
        self.sender = fn

    # --- SLOT REQUEST ---
    def do_slot_request(self):
        if not self.sender: return
        sl = self.req_slot.value()
        msg = build_slot_request(sl)
        self.sender(msg)
        self.output.append(f"Sent RQ_SL_DATA for slot {sl}")

    # --- SLOT MOVE ---
    def do_slot_move(self):
        if not self.sender: return
        src = self.move_src.value()
        dest = self.move_dest.value()
        msg = build_slot_move(src, dest)
        self.sender(msg)
        self.output.append(f"SLOT MOVE: {src} → {dest}")

    # --- OPC_LOCO_ADR ---
    def do_loco_adr(self):
        if not self.sender: return
        adr = self.loco_addr.value()
        msg = build_loco_adr(adr)
        self.sender(msg)
        self.output.append(f"Sent OPC_LOCO_ADR for address {adr}")
