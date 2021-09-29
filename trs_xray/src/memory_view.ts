import { MemoryRegionMetadata, MemoryRegions } from "./memory_regions";
import { numToHex } from "./utils";

// Rendering parameters for the memory map.
const BYTE_RENDER_GAP = 1;
const BYTE_SIZE_PX = 8;
const NUM_BYTES_X = 256; //132;
const NUM_BYTES_Y = 256; // 132;
const NUM_DATA_VIZ = 3;

enum MouseAction {
  MOVE = 1,
  UP = 2,
  DOWN = 3,
}

type SelectionUpdater = (selectedByte: number) => void;

export class MemoryView {
  private canvas: HTMLCanvasElement;
  private ctx: CanvasRenderingContext2D;
  private selectionUpdater: SelectionUpdater;

  private memoryData: Uint8Array;
  private memoryChanged: Uint8Array;
  private memoryType: Array<string>;

  private memRegions: Array<MemoryRegionMetadata>;
  private memInfo: Map<number, number>;
  private lastByteColor: Array<string>;
  private selectedMemoryRegion: number;
  private hoveredByte: number;
  private selectedByte: number;

  private programCounter: number;
  private prevProgramCounter: number;
  private stackPointer: number;

  private enableDataViz: number;
  private enableLineViz: boolean;

  constructor(elementId: string, memoryData: Uint8Array,
              memoryChanged: Uint8Array, selectionUpdater: SelectionUpdater) {
    this.canvas = document.getElementById(elementId) as HTMLCanvasElement;
    this.ctx = this.canvas.getContext("2d") as CanvasRenderingContext2D;
    this.selectionUpdater = selectionUpdater;

    this.memoryData = memoryData;
    this.memoryChanged = memoryChanged;
    this.memoryType = new Array<string>(memoryData.length);

    this.memRegions = MemoryRegions.getMemoryRegions();
    this.memInfo = new Map();
    this.lastByteColor = new Array(0xFFFF);
    this.selectedMemoryRegion = -1;
    this.hoveredByte = -1;
    this.selectedByte = -23;
    this.enableDataViz = 0;
    this.enableLineViz = false;
    this.programCounter = 0;
    this.prevProgramCounter = 0;
    this.stackPointer = 0;

    this.memRegions.map((region, idx) => {
      for (let i = region.address[0]; i<= region.address[region.address.length - 1]; ++i) {
        this.memInfo.set(i, idx);
      }
    });

    $(`#${elementId}`).on("mousemove", (evt) => {
      this.onMouseActionOnCanvas(evt.offsetX, evt.offsetY, MouseAction.MOVE);
    });
    $(`#${elementId}`).on("mouseup", (evt) => {
      this.onMouseActionOnCanvas(evt.offsetX, evt.offsetY, MouseAction.UP);
    });

    this.initCanvas();
  }

  public onMemoryRegionSelect(region: number): void {
    this.selectedMemoryRegion = region;
    this.renderMemoryRegions();
  }

  public onSelectedByteChanged(addr: number): void {
    this.selectedByte = addr;
    this.renderMemoryRegions();
  }

  public onMemoryTypeUpdate(typeData: Array<string>): void {
    this.memoryType = typeData;
  }

  public renderMemoryRegions(): void {
    // console.time("renderMemoryRegions");
    for (let y = 0; y < NUM_BYTES_Y; ++y) {
      for (let x = 0; x < NUM_BYTES_X; ++x) {
        this.renderByte(x, y);
      }
    }
    if (this.enableLineViz) this.renderEffects();
    // console.timeEnd("renderMemoryRegions");
  }

  public toggleDataViz(): void {
    this.enableDataViz = (this.enableDataViz + 1) % NUM_DATA_VIZ;
  }

  public toggleLineViz(): void {
    this.enableLineViz = !this.enableLineViz;
  }

  public onRegisterUpdate(pc: number, sp: number) {
    this.prevProgramCounter = this.programCounter;
    this.programCounter = pc;
    this.stackPointer = sp;
  }

  private initCanvas(): void {
    const totalByteSize = BYTE_SIZE_PX + BYTE_RENDER_GAP;
    this.canvas.width = NUM_BYTES_X * totalByteSize;
    this.canvas.height = NUM_BYTES_Y * totalByteSize;
    this.canvas.style.width = this.canvas.width + "px";
    this.canvas.style.height = this.canvas.height + "px";
  }

  private getColorForByte(addr: number): string {
    // Set base color for addresses we have data for
    let color = !!this.memInfo.get(addr) ? "#666" : "#444";
    // Any byte with value zero get blacked out.
    if (this.memoryData && this.memoryData[addr] == 0) color = "#000";

    if (this.memoryData) {
      if (this.enableDataViz == 1) {
        // #1: Value as shade of gray.
        let hexValue = numToHex(this.memoryData[addr] >> 1);
        if (hexValue.length == 1) hexValue = `0${hexValue}`;
        color = `#${hexValue}${hexValue}${hexValue}`;
      } else if (this.enableDataViz == 2) {
        // #2: Use disassembled data type.
        if (this.memoryType[addr] &&
            this.memoryType[addr] != "" &&
            !this.memoryType[addr].startsWith(".") &&
            this.memoryData[addr] != 0) {
          color = "#5053cc";
        }
      }
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
      console.log(`Selected: ${this.selectedByte}`);
      this.selectionUpdater(this.selectedByte);
    }
  }
}