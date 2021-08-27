declare function isDebugMode(): boolean;
declare function getMemoryRegions(): MemoryRegions;

const numToHex = (num: number) => ((num <= 0xF ? "0" : "") + num.toString(16)).toUpperCase();

const CARRY_MASK = 0x1;
const SUBTRACT_MASK = 0x2;
const OVERFLOW_MASK = 0x4;
const UNDOC3_MASK = 0x8;
const HALF_CARRY_MASK = 0x10;
const UNDOC5_MASK = 0x20;
const ZERO_MASK = 0x40;
const	SIGN_MASK = 0x80;

// Rendering parameters for the memory map.
const BYTE_RENDER_GAP = 1;
const BYTE_SIZE_PX = 8;
const NUM_BYTES_X = 132;
const NUM_BYTES_Y = 132;

const M3_TO_UTF = [
  "\u0020", "\u00a3", "\u007c", "\u00e9", "\u00dc", "\u00c5", "\u00ac", "\u00f6",
  "\u00d8", "\u00f9", "\u00f1", "\u0060", "\u0101", "\ue00d", "\u00c4", "\u00c3",
  "\u00d1", "\u00d6", "\u00d8", "\u00d5", "\u00df", "\u00fc", "\u00f5", "\u00e6",
  "\u00e4", "\u00e0", "\u0227", "\ue01b", "\u00c9", "\u00c6", "\u00c7", "\u02dc",
  "\u0020", "\u0021", "\u0022", "\u0023", "\u0024", "\u0025", "\u0026", "\u0027",
  "\u0028", "\u0029", "\u002a", "\u002b", "\u002c", "\u002d", "\u002e", "\u002f",
  "\u0030", "\u0031", "\u0032", "\u0033", "\u0034", "\u0035", "\u0036", "\u0037",
  "\u0038", "\u0039", "\u003a", "\u003b", "\u003c", "\u003d", "\u003e", "\u003f",
  "\u0040", "\u0041", "\u0042", "\u0043", "\u0044", "\u0045", "\u0046", "\u0047",
  "\u0048", "\u0049", "\u004a", "\u004b", "\u004c", "\u004d", "\u004e", "\u004f",
  "\u0050", "\u0051", "\u0052", "\u0053", "\u0054", "\u0055", "\u0056", "\u0057",
  "\u0058", "\u0059", "\u005a", "\u005b", "\u005c", "\u005d", "\u005e", "\u005f",
  "\u0060", "\u0061", "\u0062", "\u0063", "\u0064", "\u0065", "\u0066", "\u0067",
  "\u0068", "\u0069", "\u006a", "\u006b", "\u006c", "\u006d", "\u006e", "\u006f",
  "\u0070", "\u0071", "\u0072", "\u0073", "\u0074", "\u0075", "\u0076", "\u0077",
  "\u0078", "\u0079", "\u007a", "\u007b", "\u007c", "\u007d", "\u007e", "\u00b1",
  "\ue080", "\ue081", "\ue082", "\ue083", "\ue084", "\ue085", "\ue086", "\ue087",
  "\ue088", "\ue089", "\ue08a", "\ue08b", "\ue08c", "\ue08d", "\ue08e", "\ue08f",
  "\ue090", "\ue091", "\ue092", "\ue093", "\ue094", "\ue095", "\ue096", "\ue097",
  "\ue098", "\ue099", "\ue09a", "\ue09b", "\ue09c", "\ue09d", "\ue09e", "\ue09f",
  "\ue0a0", "\ue0a1", "\ue0a2", "\ue0a3", "\ue0a4", "\ue0a5", "\ue0a6", "\ue0a7",
  "\ue0a8", "\ue0a9", "\ue0aa", "\ue0ab", "\ue0ac", "\ue0ad", "\ue0ae", "\ue0af",
  "\ue0b0", "\ue0b1", "\ue0b2", "\ue0b3", "\ue0b4", "\ue0b5", "\ue0b6", "\ue0b7",
  "\ue0b8", "\ue0b9", "\ue0ba", "\ue0bb", "\ue0bc", "\ue0bd", "\ue0be", "\ue0bf",
  "\u2660", "\u2665", "\u2666", "\u2663", "\u263a", "\u2639", "\u2264", "\u2265",
  "\u03b1", "\u03b2", "\u03b3", "\u03b4", "\u03b5", "\u03b6", "\u03b7", "\u03b8",
  "\u03b9", "\u03ba", "\u03bc", "\u03bd", "\u03be", "\u03bf", "\u03c0", "\u03c1",
  "\u03c2", "\u03c3", "\u03c4", "\u03c5", "\u03c6", "\u03c7", "\u03c8", "\u03c9",
  "\u2126", "\u221a", "\u00f7", "\u2211", "\u2248", "\u2206", "\u2307", "\u2260",
  "\u2301", "\ue0e9", "\u237e", "\u221e", "\u2713", "\u00a7", "\u2318", "\u00a9",
  "\u00a4", "\u00b6", "\u00a2", "\u00ae", "\ue0f4", "\ue0f5", "\ue0f6", "\u211e",
  "\u2105", "\u2642", "\u2640", "\ue0fb", "\ue0fc", "\ue0fd", "\ue0fe", "\u2302"
];

enum MouseAction {
  MOVE = 1,
  UP = 2,
  DOWN = 3,
}

class TrsXray {
  private socket: WebSocket | null = null;
  private canvas: HTMLCanvasElement;
  private ctx: CanvasRenderingContext2D;
  private programCounter: number;
  private stackPointer: number;
  private memRegions: MemoryRegions;
  private memInfo: Map<number, number>;
  private memoryData: Uint8Array | null;
  private memoryChanged: Uint8Array;
  private lastByteColor: Array<string>;

  private selectedMemoryRegion: number;
  private hoveredByte: number;
  private selectedByte: number;

  private enableDataViz: boolean;

  constructor() {
    this.canvas = document.getElementById("memory-container") as HTMLCanvasElement;
    this.ctx = this.canvas.getContext("2d") as CanvasRenderingContext2D;
    this.programCounter = 0;
    this.stackPointer = 0;
    this.memRegions = getMemoryRegions();
    this.memInfo = new Map();
    this.memoryData = null;
    this.memoryChanged = new Uint8Array(0);
    this.lastByteColor = new Array(0xFFFFFF);
    this.selectedMemoryRegion = -1;
    this.hoveredByte = -1;
    this.selectedByte = -1;

    this.enableDataViz = false;

    this.memRegions.map((region, idx) => {
      for (let i = region.address[0]; i<= region.address[region.address.length - 1]; ++i) {
        this.memInfo.set(i, idx);
      }
    });

    this.initCanvas();
  }

  private initCanvas(): void {
    const totalByteSize = BYTE_SIZE_PX + BYTE_RENDER_GAP;
    this.canvas.width = NUM_BYTES_X * totalByteSize;
    this.canvas.height = NUM_BYTES_Y * totalByteSize;
    this.canvas.style.width = this.canvas.width + "px";
    this.canvas.style.height = this.canvas.height + "px";
  }

  private onMessageFromEmulator(json: IDataFromEmulator): void {
    if (json.context) this.onContextUpdate(json.context);
    if (json.registers) {
      this.onRegisterUpdate(json.registers);
    }
  }

  private onControl(action: string): void {
    if (this.socket != undefined) this.socket.send("action/" + action);
  }

  public onLoad(): void {
    $('input:text').on("keydown", (evt) => {evt.stopPropagation();});
    document.addEventListener("keydown", (evt) => {
      switch (evt.key) {
        case 'j':
          this.onControl("step");
          this.onControl("get_memory/0/65536");
          break;
        case 't':
          this.debug_insertTestData();
          break;
        case 'd':
          this.enableDataViz = !this.enableDataViz;
          break;
        default:
          console.log(`Unhandled key event: ${evt.key}`);
      }
    });

    $("#step-btn").on("click", () => {
      this.onControl("step");
      this.onControl("get_memory/0/65536");
    });
    $("#step-over-btn").on("click", () => { this.onControl("step-over") });
    $("#play-btn").on("click", () => { this.onControl("continue") });
    $("#stop-btn").on("click", () => { this.onControl("stop") });
    $("#reset-btn").on("click", (ev) => {
      this.onControl(ev.shiftKey ? "hard_reset" : "soft_reset")
    });
    $("#memory-container").on("mousemove", (evt) => {
      this.onMouseActionOnCanvas(evt.offsetX, evt.offsetY, MouseAction.MOVE);
    });
    $("#memory-container").on("mouseup", (evt) => {
      this.onMouseActionOnCanvas(evt.offsetX, evt.offsetY, MouseAction.UP);
    });

    this.createMemoryRegions();

    if (!isDebugMode()) this.keepConnectionAliveLoop();
  }

  private keepConnectionAliveLoop() {
    if (!this.socket || this.socket.readyState != WebSocket.OPEN) {
      $("h1").addClass("errorTitle");
      if (!this.socket ||
          this.socket.readyState == WebSocket.CLOSED ||
          this.socket.readyState == WebSocket.CLOSING) {
        this.createNewSocket();
      }
      setTimeout(() => this.keepConnectionAliveLoop(), 200);
    } else {
      $("h1").removeClass("errorTitle");
      setTimeout(() => this.keepConnectionAliveLoop(), 500);
    }
  }

  private createNewSocket() {
    console.log("Creating new Websocket");
    this.socket = new WebSocket("ws://" + location.host + "/channel");
    this.socket.onerror = (evt) => {
      console.log("Unable to connect to websocket.");
      $("h1").addClass("errorTitle");
    };
    this.socket.onopen = () => {
      console.log("Socked opened.");
      this.onControl("refresh");
    }
    this.socket.onmessage = (evt) =>  {
      if (evt.data instanceof Blob) {
        evt.data.arrayBuffer().then((data) => {
          this.onMemoryUpdate(new Uint8Array(data));
        });
      } else {
        var json = JSON.parse(evt.data);
        this.onMessageFromEmulator(json);
      }
      this.renderDisplay();
      this.renderMemoryRegions();
    };
  }

  private onContextUpdate(ctx: ISUT_Context): void {
    $("#sut-name").text(ctx.system_name);
    $("#sut-model-no").text(ctx.model);
  }

  private onRegisterUpdate(registers: ISUT_Registers): void {
    /* Main registers. */
    let a = (registers.af & 0xFF00) >> 8;
    let f = (registers.af & 0x00FF);
    let b = (registers.bc & 0xFF00) >> 8;
    let c = (registers.bc & 0x00FF);
    let d = (registers.de & 0xFF00) >> 8;
    let e = (registers.de & 0x00FF);
    let h = (registers.hl & 0xFF00) >> 8;
    let l = (registers.hl & 0x00FF);

    /* Alternate registers. */
    let ap = (registers.af_prime & 0xFF00) >> 8;
    let fp = (registers.af_prime & 0x00FF);
    let bp = (registers.bc_prime & 0xFF00) >> 8;
    let cp = (registers.bc_prime & 0x00FF);
    let dp = (registers.de_prime & 0xFF00) >> 8;
    let ep = (registers.de_prime & 0x00FF);
    let hp = (registers.hl_prime & 0xFF00) >> 8;
    let lp = (registers.hl_prime & 0x00FF);

    /* Index registers. */
    let ix1 = (registers.ix & 0xFF00) >> 8;
    let ix2 = (registers.ix & 0x00FF);
    let iy1 = (registers.iy & 0xFF00) >> 8;
    let iy2 = (registers.iy & 0x00FF);
    let sp1 = (registers.sp & 0xFF00) >> 8;
    let sp2 = (registers.sp & 0x00FF);

    /* Program counter. */
    let pc1 = (registers.pc & 0xFF00) >> 8;
    let pc2 = (registers.pc & 0x00FF);

    /** Extract flag values from 'f' register. */
    let flag_sign	=	f & SIGN_MASK;
    let flag_zero = f & ZERO_MASK;
    let flag_undoc5 = f & UNDOC5_MASK;
    let flag_half_carry = f & HALF_CARRY_MASK;
    let flag_undoc3 = f & UNDOC3_MASK;
    let flag_overflow = f & OVERFLOW_MASK;
    let flag_subtract = f & SUBTRACT_MASK;
    let flag_carry = f & CARRY_MASK;

    $("#reg-af-1").val(numToHex(a));
    $("#reg-af-2").val(numToHex(f));
    $("#reg-bc-1").val(numToHex(b));
    $("#reg-bc-2").val(numToHex(c));
    $("#reg-de-1").val(numToHex(d));
    $("#reg-de-2").val(numToHex(e));
    $("#reg-hl-1").val(numToHex(h));
    $("#reg-hl-2").val(numToHex(l));

    $("#reg-afp-1").val(numToHex(ap));
    $("#reg-afp-2").val(numToHex(fp));
    $("#reg-bcp-1").val(numToHex(bp));
    $("#reg-bcp-2").val(numToHex(cp));
    $("#reg-dep-1").val(numToHex(dp));
    $("#reg-dep-2").val(numToHex(ep));
    $("#reg-hlp-1").val(numToHex(hp));
    $("#reg-hlp-2").val(numToHex(lp));

    $("#reg-ix-1").val(numToHex(ix1));
    $("#reg-ix-2").val(numToHex(ix2));
    $("#reg-iy-1").val(numToHex(iy1));
    $("#reg-iy-2").val(numToHex(iy2));
    $("#reg-sp-1").val(numToHex(sp1));
    $("#reg-sp-2").val(numToHex(sp2));

    $("#reg-pc-1").val(numToHex(pc1));
    $("#reg-pc-2").val(numToHex(pc2));

    /* Fun with Flags */
    let setFlag = (id: string, value: number) => {
      if (value) {
        $(id).addClass("flag-enabled");
      } else {
        $(id).removeClass("flag-enabled");
      }
    };

    setFlag("#flag-s",  flag_sign);
    setFlag("#flag-z",  flag_zero);
    setFlag("#flag-u5", flag_undoc5);
    setFlag("#flag-h",  flag_half_carry);
    setFlag("#flag-u3", flag_undoc3);
    setFlag("#flag-pv", flag_overflow);
    setFlag("#flag-n",  flag_subtract);
    setFlag("#flag-c",  flag_carry);

    this.programCounter = registers.pc;
    this.stackPointer = registers.sp;
  }

  private onMemoryUpdate(memory: Uint8Array): void {
    this.memoryChanged = new Uint8Array(memory.length);
    for (let i = 0; i < memory.length; ++i) {
      if (this.memoryData && this.memoryData.length == memory.length) {
        this.memoryChanged[i] = memory[i] == this.memoryData[i] ? 0 : 1;
      } else {
        this.memoryChanged[i] = 1;
      }
    }
    this.memoryData = memory;
    this.onSelectionUpdate();
  }

  private onSelectionUpdate(): void {
    let hi = (this.selectedByte & 0xFF00) >> 8;
    let lo = (this.selectedByte & 0x00FF);
    $("#selected-addr-1").val(numToHex(hi));
    $("#selected-addr-2").val(numToHex(lo));
    if (this.memoryData && this.selectedByte >= 0) {
      $("#selected-val").val(numToHex(this.memoryData[this.selectedByte]));
    }
  }

  private createMemoryRegions(): void {
    const regions = getMemoryRegions();
    const container = $("#memory-regions");
    regions.map((region, idx) => {
      $("<div></div>")
          .addClass("map-region-list-entry")
          .text(region.description)
          .on("mouseover", () => {this.onMemoryRegionSelect(idx)})
          .appendTo(container);
    });
    this.renderMemoryRegions();
  }

  private onMemoryRegionSelect(region: number): void {
    this.selectedMemoryRegion = region;
    this.renderMemoryRegions();
  }

  private getColorForByte(addr: number): string {
    // Set base color for addresses we have data for
    let color = !!this.memInfo.get(addr) ? "#666" : "#444";
    // Any byte with value zero get blacked out.
    if (this.memoryData && this.memoryData[addr] == 0) color = "#000";

    if (this.enableDataViz && !!this.memoryData) {
      let hexValue = numToHex(this.memoryData[addr] >> 1);
      if (hexValue.length == 1) hexValue = `0${hexValue}`;
      color = `#${hexValue}${hexValue}${hexValue}`;
    }

    // Mark selected range bytes.
    if (this.selectedMemoryRegion >= 0) {
      let addressRange = this.memRegions[this.selectedMemoryRegion].address
      const highlightStart = addressRange[0];
      const highlightEnd = addressRange[addressRange.length - 1];
      color = (addr >= highlightStart && addr <= highlightEnd) ? "#F00" : color;
    }

    // Highlight bytes that have changed with the most recent update.
    if (this.memoryChanged.length > addr && this.memoryChanged[addr] == 1) {
      color = "#FFA500";
    }

    // Mark special program and stack pointers.
    if (addr == this.programCounter) color = "#0F0";
    if (addr == this.stackPointer) color = "#FF0";

    return color;
  }

  private renderByte(x: number, y: number, removeHighlight = false): void {
    const addr = (y * NUM_BYTES_X) + x;
    const totalByteSize = BYTE_SIZE_PX + BYTE_RENDER_GAP;

    const changeHighlight = addr == this.selectedByte ||
                            addr == this.hoveredByte ||
                            removeHighlight;
    if (changeHighlight) {
      this.ctx.fillStyle = addr == this.hoveredByte ? "#88A" :
                          (addr == this.selectedByte ? "#FFF" : "#000");
      this.ctx.fillRect(x * totalByteSize - 1, y * totalByteSize - 1,
                        BYTE_SIZE_PX + 2, BYTE_SIZE_PX + 2);
    }

    // Optimize rendering by only updating changed bytes.
    // ~10x speed improvement.
    const color = this.getColorForByte(addr);
    if (color != this.lastByteColor[addr] || changeHighlight) {
      this.ctx.fillStyle = color;
      this.ctx.fillRect(x * totalByteSize, y * totalByteSize,
                        BYTE_SIZE_PX, BYTE_SIZE_PX);
      this.lastByteColor[addr] = color;
    }
  }

  private renderMemoryRegions(): void {
    console.time("renderMemoryRegions");
    // this.ctx.beginPath();

    for (let y = 0; y < NUM_BYTES_Y; ++y) {
      for (let x = 0; x < NUM_BYTES_X; ++x) {
        this.renderByte(x, y);
      }
    }
    console.timeEnd("renderMemoryRegions");
  }

  private onMouseActionOnCanvas(x: number, y: number, a: MouseAction): void {
    let setter = a == MouseAction.MOVE ?
                 (newAddr: number) => {this.hoveredByte = newAddr} :
                 (newAddr: number) => {this.selectedByte = newAddr};
    let getter = a == MouseAction.MOVE ?
                 () => {return this.hoveredByte} :
                 () => {return this.selectedByte};

    const totalByteSize = BYTE_SIZE_PX + BYTE_RENDER_GAP;
    const byteX = Math.floor(x / totalByteSize);
    const byteY = Math.floor(y / totalByteSize);

    const prevByte = getter();
    setter(byteY * NUM_BYTES_X + byteX);

    const prevByteY = Math.floor(prevByte / NUM_BYTES_X);
    const prevByteX = prevByte - (prevByteY * NUM_BYTES_X);
    this.renderByte(prevByteX, prevByteY, true);
    this.renderByte(byteX, byteY);

    if (a == MouseAction.UP) {
      this.onSelectionUpdate();
    }
  }

  private renderDisplay(): void {
    console.time("renderDisplay");
    if (!this.memoryData) return;
    // FIXME: Need to determine when we have 64x16 or 80x24
    const width = 64;
    const height = 16;

    const VIDEO_RAM_OFFSET =  0x3C00;

    let screenStr = "";
    for (let y = 0; y < height; ++y) {
      for (let x = 0; x < width; ++x) {
        screenStr += M3_TO_UTF[this.memoryData[y * width + x + VIDEO_RAM_OFFSET]];
      }
      screenStr += "\n";
    }
    $("#screen").text(screenStr);
    console.timeEnd("renderDisplay");
  }

  private debug_insertTestData(): void {
    console.log("Inserting test data for debugging...");
    const data: IDataFromEmulator = {"context":{"system_name":"sdlTRS","model":3},"registers":{"pc":1,"sp":65535,"af":65535,"bc":0,"de":0,"hl":0,"af_prime":0,"bc_prime":0,"de_prime":0,"hl_prime":0,"ix":0,"iy":0,"i":0,"r_1":0,"r_2":1,"z80_t_state_counter":4,"z80_clockspeed":2.0299999713897705,"z80_iff1":0,"z80_iff2":0,"z80_interrupt_mode":0}};
    this.onMessageFromEmulator(data);
  }
}

interface ISUT_Context {
  system_name: string;
  model: number;
}

interface ISUT_Registers {
  ix: number,
  iy: number,
  pc: number,
  sp: number,
  af: number,
  bc: number,
  de: number,
  hl: number,
  af_prime: number,
  bc_prime: number,
  de_prime: number,
  hl_prime: number,
  i: number,
  r_1: number,
  r_2: number,

  z80_t_state_counter: number,
  z80_clockspeed: number,
  z80_iff1: number,
  z80_iff2: number,
  z80_interrupt_mode: number,
}

interface IDataFromEmulator {
  context: ISUT_Context,
  registers: ISUT_Registers
}

interface MemoryRegionMetadata {
  address: Array<number>,
  model_code: Array<number>,
  description: string
}

type MemoryRegions = Array<MemoryRegionMetadata>;