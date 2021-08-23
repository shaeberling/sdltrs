declare function isDebugMode(): boolean;
declare function getMemoryRegions(): MemoryRegions;

const CARRY_MASK = 0x1;
const SUBTRACT_MASK = 0x2;
const OVERFLOW_MASK = 0x4;
const UNDOC3_MASK = 0x8;
const HALF_CARRY_MASK = 0x10;
const UNDOC5_MASK = 0x20;
const ZERO_MASK = 0x40;
const	SIGN_MASK = 0x80;

class TrsXray {
  private socket: WebSocket | null = null;
  private canvas: HTMLCanvasElement;
  private ctx: CanvasRenderingContext2D;
  private programCounter: number;
  private stackPointer: number;
  private memRegions: MemoryRegions;
  private memInfo: Map<number, number>;
  private memHeatmap: Map<number, number>;
  private memoryData: Uint8Array | null;
  private memoryPrevData: Uint8Array | null;
  private selectedMemoryRegion: number;

  private enableDataViz: boolean;

  constructor() {
    this.canvas = document.getElementById("memory-container") as HTMLCanvasElement;
    this.ctx = this.canvas.getContext("2d") as CanvasRenderingContext2D;
    this.programCounter = 0;
    this.stackPointer = 0;
    this.memRegions = getMemoryRegions();
    this.memInfo = new Map();
    this.memHeatmap = new Map();
    this.memoryData = null;
    this.memoryPrevData = null;
    this.selectedMemoryRegion = -1;

    this.enableDataViz = false;

    this.memRegions.map((region, idx) => {
      for (let i = region.address[0]; i<= region.address[region.address.length - 1]; ++i) {
        this.memInfo.set(i, idx);
      }
    });
  }

  private onMessageFromEmulator(json: IDataFromEmulator): void {
    if (!!json.context) this.onContextUpdate(json.context);
    if (!!json.registers) {
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
    $("#reset-btn").on("click", (ev) => {
      this.onControl(ev.shiftKey ? "hard_reset" : "soft_reset")
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

    const numToHex = (num: number) => (num <= 0xF ? "0" : "") + num.toString(16);

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
    // TODO: Compare previous and new data.
    //       Indicate changed bytes.
    this.memoryPrevData = this.memoryData;
    this.memoryData = memory;
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
    if (!!this.memoryData && this.memoryData[addr] == 0) color = "#000";

    if (this.enableDataViz && !!this.memoryData) {
      let hexValue = (this.memoryData[addr] >> 1).toString(16);
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
    if (!!this.memoryData && !!this.memoryPrevData) {
      if (this.memoryData[addr] !=
          this.memoryPrevData[addr]) color = "#FFA500";
    }

    // Mark special program and stack pointers.
    if (addr == this.programCounter) color = "#0F0";
    if (addr == this.stackPointer) color = "#FF0";

    return color;
  }

  private renderMemoryRegions(): void {
    const gap = 1;
    const byteSize = 6;
    const bytesWidth = 192;
    const bytesHeight = 192;
    this.canvas.width = bytesWidth * (byteSize + gap);
    this.canvas.height = bytesHeight * (byteSize + gap);
    this.canvas.style.width = this.canvas.width + "px";
    this.canvas.style.height = this.canvas.height + "px";
    this.ctx.beginPath();


    for (let y = 0; y < bytesHeight; ++y) {
      for (let x = 0; x < bytesWidth; ++x) {
        let addr = (y * bytesWidth) + x;
        this.ctx.fillStyle = this.getColorForByte(addr);
        this.ctx.fillRect(x*(byteSize+gap), y*(byteSize+gap), byteSize, byteSize);
      }
    }
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