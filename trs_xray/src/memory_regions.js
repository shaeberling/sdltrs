// Big thanks to http://www.trs-80.com/ for a majority of these!
function getMemoryRegions() {
return [
  {
    "address": [0x3C00, 0x3FFF],
    "model_code": [1, 3],
    "description": "Video RAM"
  },
  {
    "address": [0x37E1],
    "model_code": [1],
    "description": "Disk drive select"
  },
  {
    "address": [0x37E4],
    "model_code": [1],
    "description": "Tape drive select: 0=#1 1=#2"
  },
  {
    "address": [0x37E8],
    "model_code": [1, 3],
    "description": "Printer Status: 63=On, 143=Off"
  },
  {
    "address": [0x37E9],
    "model_code": [1, 3],
    "description": "Printer output"
  },
  {
    "address": [0x37EC],
    "model_code": [1],
    "description": "Disk command/status"
  },
  {
    "address": [0x37ED],
    "model_code": [1],
    "description": "Disk track select"
  },
  {
    "address": [0x37EE],
    "model_code": [1],
    "description": "Disk sector select"
  },
  {
    "address": [0x37EF],
    "model_code": [1],
    "description": "Disk data"
  },
  {
    "address": [0x3800, 0x3840],
    "model_code": [1, 3],
    "description": "Keyboard matrix"
  },
  {
    "address": [0x3FCD, 0x3FCE],
    "model_code": [],
    "description": "End of BASIC Program Pointer (LSB)"
  },
  {
    "address": [0x4000],
    "model_code": [],
    "description": "RST 08 (Syntax Check)"
  },
  {
    "address": [0x4001, 0x4014],
    "model_code": [200],
    "description": "Jump vectors for RST 8 - RST 56"
  },
  {
    "address": [0x4003, 0x4005],
    "model_code": [1, 3],
    "description": "RST 10H (Get Next Character): Jump Vector."
  },
  {
    "address": [0x4006],
    "model_code": [],
    "description": "RST 18H (Cp HL)"
  },
  {
    "address": [0x4009],
    "model_code": [],
    "description": "RST 20H (Get Current Type) Jump Vector."
  },
  {
    "address": [0x400C, 0x400E],
    "model_code": [1, 3],
    "description": "RST 28H (Break Key Vector).  By default"
  },
  {
    "address": [0x400F],
    "model_code": [3],
    "description": "RST 30H Jump Vector."
  },
  {
    "address": [0x4012, 0x4015],
    "model_code": [1, 3],
    "description": "RST 38H Jump Vector."
  },
  {
    "address": [0x4015, 0x401C],
    "model_code": [1, 3],
    "description": "Beginning of the Keyboard Device Control Block (DCB)"
  },
  {
    "address": [0x4015],
    "model_code": [3],
    "description": "Tape RAM - Keyboard DCB: Type = 1 = Read Only"
  },
  {
    "address": [0x4016, 0x4017],
    "model_code": [1],
    "description": "Two byte keyboard driver vector"
  },
  {
    "address": [0x4016],
    "model_code": [3],
    "description": "Two byte keyboard driver vector."
  },
  {
    "address": [0x4018],
    "model_code": [1, 3],
    "description": "Keyboard DCB: Right Shift Toggle"
  },
  {
    "address": [0x4019],
    "model_code": [3],
    "description": "Caps Lock Toggle.  0=Off (i.e."
  },
  {
    "address": [0x401A],
    "model_code": [3],
    "description": "Cursor Blink Count."
  },
  {
    "address": [0x401B],
    "model_code": [3],
    "description": "Cursor Blink Status. (0=Off Nz=On)"
  },
  {
    "address": [0x401C],
    "model_code": [1, 3],
    "description": "Cursor Blink Switch.  Set to 0 for blink"
  },
  {
    "address": [0x401D, 0x4024],
    "model_code": [1, 3],
    "description": "Beggining of Video Display Control Block (DCB)."
  },
  {
    "address": [0x401D],
    "model_code": [3],
    "description": "Tape RAM - Video DCB: Type = 7 = Read/write"
  },
  {
    "address": [0x401E, 0x401F],
    "model_code": [],
    "description": "Two byte video driver vector"
  },
  {
    "address": [0x401E],
    "model_code": [3],
    "description": "Tape RAM - Video DCB: Driver Address (0473h)"
  },
  {
    "address": [0x4020, 0x4021],
    "model_code": [1, 3],
    "description": "Cursor Position On Screen. 2 Bytes in LSB/MSB Order"
  },
  {
    "address": [0x4022],
    "model_code": [],
    "description": "Cursor (0=on)"
  },
  {
    "address": [0x4022],
    "model_code": [3],
    "description": "Cursor On/off Flag (z = Off Nz = Character Undercursor)"
  },
  {
    "address": [0x4023],
    "model_code": [],
    "description": "Cursor Character (in ASCII). Defaults: 176."
  },
  {
    "address": [0x4023],
    "model_code": [3],
    "description": "Tape RAM - Video DCB: Cursor Character (default 0b0h)"
  },
  {
    "address": [0x4024],
    "model_code": [],
    "description": "FLAG : 0 = Space compression; Not 0 = special character."
  },
  {
    "address": [0x4024],
    "model_code": 3,
    "description": "Character Set To Use:  0=Regular"
  },
  {
    "address": [0x4024],
    "model_code": [3],
    "description": "Tape RAM - Video DCB: Tabs/special Characters Switch (z = Tabs)"
  }
];
}