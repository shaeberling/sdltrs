declare function isDebugMode(): boolean;
declare function getMemoryRegions(): MemoryRegions;

import { RegisterPanel } from "./register_panel"
import { ISUT_Context, ISUT_Registers, ISUT_Breakpoint } from "./data_structures";
import { numToHex } from "./utils";
import { Screen } from "./screen";

// Rendering parameters for the memory map.
const BYTE_RENDER_GAP = 1;
const BYTE_SIZE_PX = 8;
const NUM_BYTES_X = 256; //132;
const NUM_BYTES_Y = 128; // 132;

// See web_debugger.h for definitions.
const BP_TYPE_TEXT = ["Program Counter", "Memory Watch", "IO Watch"];

enum MouseAction {
  MOVE = 1,
  UP = 2,
  DOWN = 3,
}

class TrsXray {
  private socket: WebSocket | null = null;
  private canvas: HTMLCanvasElement;
  private ctx: CanvasRenderingContext2D;
  private screen: Screen;
  private registers: RegisterPanel;
  private programCounter: number;
  private prevProgramCounter: number;
  private stackPointer: number;
  private memRegions: MemoryRegions;
  private memInfo: Map<number, number>;
  private memoryData: Uint8Array;
  private memoryChanged: Uint8Array;
  private lastByteColor: Array<string>;

  private selectedMemoryRegion: number;
  private hoveredByte: number;
  private selectedByte: number;

  private enableDataViz: boolean;
  private enableLineViz: boolean;
  // If false, only screen and registers are updated.
  private enableFullMemoryUpdate: boolean;
  private memoryUpdateStartAddress: number;

  constructor() {
    this.canvas = document.getElementById("memory-container") as HTMLCanvasElement;
    this.ctx = this.canvas.getContext("2d") as CanvasRenderingContext2D;
    this.screen = new Screen("#screen");
    this.registers = new RegisterPanel();
    this.programCounter = 0;
    this.prevProgramCounter = 0;
    this.stackPointer = 0;
    this.memRegions = getMemoryRegions();
    this.memInfo = new Map();
    this.memoryData = new Uint8Array(0xFFFF);
    this.memoryChanged = new Uint8Array(0xFFFF);
    this.lastByteColor = new Array(0xFFFF);
    this.selectedMemoryRegion = -1;
    this.hoveredByte = -1;
    this.selectedByte = -1;
    this.enableDataViz = false;
    this.enableLineViz = false;
    this.enableFullMemoryUpdate = true;
    this.memoryUpdateStartAddress = 0;

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
    if (json.breakpoints) this.onBreakpointUpdate(json.breakpoints);
    if (json.registers) this.onRegisterUpdate(json.registers);
  }

  private onControl(action: string): void {
    if (this.socket != undefined) this.socket.send("action/" + action);
  }

  private requestMemoryUpdate(): void {
    if (this.enableFullMemoryUpdate) {
      this.memoryUpdateStartAddress = 0;
      this.onControl("get_memory/0/65536");
    } else {
      this.memoryUpdateStartAddress = 0x3C00;
      this.onControl(`get_memory/${0x3C00}/${0x3FFF-0x3C00}`);
    }
  }

  /** Key pressed that is to be send to SUT. */
  private onKeyPressForSut(direction: string, evt: KeyboardEvent): void {
    evt.preventDefault();
    if (evt.repeat) return;

    let shift = evt.shiftKey ? "1" : "0";
    let dir = direction == "down" ? "1" : "0";
    this.onControl(`key_event/${dir}/${shift}/${evt.key}`);
  }

  public onLoad(): void {
    $('input:text').on("keydown", (evt) => {evt.stopPropagation();});
    document.addEventListener("keydown", (evt) => {
      if (this.screen.isMouseOnScreen()) this.onKeyPressForSut("down", evt);
      else {
        switch (evt.key) {
          case 'j':
            this.onControl("step");
            this.requestMemoryUpdate();
            break;
          case 't':
            this.debug_insertTestData();
            break;
          case 'd':
            this.enableDataViz = !this.enableDataViz;
            break;
          case 'e':
            this.enableLineViz = !this.enableLineViz;
            break;
          case 'm':
            this.enableFullMemoryUpdate = !this.enableFullMemoryUpdate;
            break;
          case 'r':
            this.onControl("get_memory/force_update");
            break;
          default:
            console.log(`Unhandled key event: ${evt.key}`);
        }
      }
    });
    document.addEventListener("keyup", (evt) => {
      if (this.screen.isMouseOnScreen()) this.onKeyPressForSut("up", evt);
    });

    $("#step-btn").on("click", () => {
      this.onControl("step");
      this.requestMemoryUpdate();
    });
    $("#step-over-btn").on("click", () => { this.onControl("step-over") });
    $("#play-btn").on("click", () => { this.onControl("continue") });
    $("#stop-btn").on("click", () => { this.onControl("stop") });
    $("#reset-btn").on("click", (ev) => {
      // this.onControl(ev.shiftKey ? "hard_reset" : "soft_reset")
      this.onControl("get_memory/force_update");
    });

    const addBreakpointHandler = (ev: JQuery.ClickEvent) => {
      const type = $(ev.currentTarget).attr("data");
      console.log(`Click data: ${ev.currentTarget} -> ${type}`);
      const addr1 = $("#selected-addr-1").val() as string;
      const addr2 = $("#selected-addr-2").val() as string;
      console.log(`Add breakpoint for: ${addr1}/${addr2}`);

      if (!addr1 || addr1.length != 2 || !addr2 || addr2.length != 2) {
        alert("Invalid address");
        return;
      }
      const addr = parseInt(`${addr1}${addr2}`, 16);
      this.onControl(`add_breakpoint/${type}/${addr}`)
    };
    $("#add-breakpoint-pc").on("click", addBreakpointHandler);
    $("#add-breakpoint-memory").on("click", addBreakpointHandler);

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
          this.screen.render(this.memoryData);
        });
      } else {
        var json = JSON.parse(evt.data);
        this.onMessageFromEmulator(json);
        this.renderMemoryRegions();
      }
    };
  }

  private onContextUpdate(ctx: ISUT_Context): void {
    $("#sut-name").text(ctx.system_name);
    $("#sut-model-no").text(ctx.model);
  }

  private onRegisterUpdate(registers: ISUT_Registers): void {
    this.registers.update(registers);
    this.prevProgramCounter = this.programCounter;
    this.programCounter = registers.pc;
    this.stackPointer = registers.sp;
  }

  lastBreakpoints = Array<ISUT_Breakpoint>(0);
  private onBreakpointUpdate(breakpoints: Array<ISUT_Breakpoint>): void {
    if (JSON.stringify(this.lastBreakpoints) == JSON.stringify(breakpoints)) return;

    this.lastBreakpoints = breakpoints;
    console.log(JSON.stringify(this.lastBreakpoints));
    console.log(JSON.stringify(breakpoints));
    $("#breakpoints").empty();
    for (var bp of breakpoints) {
      let r1 = (bp.address & 0xFF00) >> 8;
      let r2 = (bp.address & 0x00FF);
      $(`<div class="register">${numToHex(r1)}</div>`).appendTo("#breakpoints");
      $(`<div class="register">${numToHex(r2)}</div>`).appendTo("#breakpoints");
      $(`<div class="breakpoint-type">${BP_TYPE_TEXT[bp.type]}</div>`).appendTo("#breakpoints");
      $(`<div class="remove-breakpoint" data="${bp.id}"></div>`)
          .on("click", (evt) => {
            const id = $(evt.currentTarget).attr("data");
            this.onControl(`remove_breakpoint/${id}`);
          })
          .appendTo("#breakpoints");
    }
  }

  private onMemoryUpdate(memory: Uint8Array): void {
    // The first two bytes are the start offset address.
    let startAddr = (memory[0] << 8) + memory[1];
    console.log(`Starting Addr: ${startAddr}`);
    this.memoryChanged = new Uint8Array(0xFFFF);
    for (let i = 2; i < memory.length; ++i) {
      let addr = startAddr + i - 2;
      if (this.memoryData[addr] != memory[i]) {
        this.memoryChanged[addr] = 1;
        this.memoryData[addr] = memory[i];
      }
    }
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

  private renderEffects(): void {
    const totalByteSize = BYTE_SIZE_PX + BYTE_RENDER_GAP;
    const addrPc0Y = Math.floor(this.prevProgramCounter / NUM_BYTES_X);
    const addrPc0X = this.prevProgramCounter % NUM_BYTES_X;
    const addrPc1Y = Math.floor(this.programCounter / NUM_BYTES_X);
    const addrPc1X = this.programCounter % NUM_BYTES_X;

    const pc0Y = addrPc0Y * totalByteSize + BYTE_SIZE_PX/2;
    const pc0X = addrPc0X * totalByteSize + BYTE_SIZE_PX/2;
    const pc1Y = addrPc1Y * totalByteSize + BYTE_SIZE_PX/2;
    const pc1X = addrPc1X * totalByteSize + BYTE_SIZE_PX/2;

    console.log(`Line: ${pc0X},${pc0Y} -> ${pc1X},${pc1Y}`);

    this.ctx.beginPath();
    this.ctx.strokeStyle = "yellow";
    this.ctx.moveTo(pc0X, pc0Y);
    this.ctx.lineTo(pc1X, pc1Y);
    this.ctx.stroke();
  }

  private renderMemoryRegions(): void {
    // console.time("renderMemoryRegions");
    for (let y = 0; y < NUM_BYTES_Y; ++y) {
      for (let x = 0; x < NUM_BYTES_X; ++x) {
        this.renderByte(x, y);
      }
    }
    if (this.enableLineViz) this.renderEffects();
    // console.timeEnd("renderMemoryRegions");
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

  private debug_insertTestData(): void {
    console.log("Inserting test data for debugging...");
    const data: IDataFromEmulator = {"context":{"system_name":"sdlTRS","model":3},"breakpoints":[{"id":0,"address":4656,"type":0},{"id":1,"address":6163,"type":0},{"id":2,"address":9545,"type":0}],"registers":{"pc":2,"sp":65535,"af":68,"bc":0,"de":0,"hl":0,"af_prime":0,"bc_prime":0,"de_prime":0,"hl_prime":0,"ix":0,"iy":0,"i":0,"r_1":0,"r_2":2,"z80_t_state_counter":8,"z80_clockspeed":2.0299999713897705,"z80_iff1":0,"z80_iff2":0,"z80_interrupt_mode":0}};
    this.onMessageFromEmulator(data);
  }
}


interface IDataFromEmulator {
  context: ISUT_Context,
  breakpoints: Array<ISUT_Breakpoint>,
  registers: ISUT_Registers
}

interface MemoryRegionMetadata {
  address: Array<number>,
  model_code: Array<number>,
  description: string
}

type MemoryRegions = Array<MemoryRegionMetadata>;

// Hook up our code to site onload event.
// Note: This is important to add as an entry point as webpack would otherwise
//       remove most of our code as it appears unused.
window.onload = () => {new TrsXray().onLoad()};
console.log("Hooking up window.onload");