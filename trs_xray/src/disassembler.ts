import { Disasm, Instruction } from "z80-disasm";
import { numToHex2 } from "./utils";

const NUM_ITEMS = 16;

export class Disassembler {
  private pc: number;
  private root: JQuery<Element>;

  constructor(elementId: string) {
    this.pc = 0;
    this.root = $(`#${elementId}`);
  }

  /**
   * Trigger disassembly.
   *
   * @param memory memory to be loaded into the disassembler.
   */
  public disassemble(memory: Uint8Array): Array<string> {
    const dis = new Disasm();
    dis.addChunk(memory, 0);
    dis.addEntryPoint(0);
    dis.addEntryPoint(this.pc);
    const result = dis.disassemble();

    const dataType = new Array<string>(memory.length);
    for (var i = 0; i < result.length; ++i) {
      dataType[result[i].address] = result[i].mnemonic;
    }
    this.updateView(result);
    // console.log(result);
    return dataType;
  }

  /**
   * Update program counter.
   *
   * @param addr program counter address.
   */
  public updatePC(addr: number): void {
    this.pc = addr;
  }

  private updateView(instructions: Instruction[]): void {
    var id = 0;
    for (; id < instructions.length; ++id) {
      if (instructions[id].address == this.pc) break;
    }
    if (id >= instructions.length) {
      console.error(`Cannot find instruction with given PC: ${this.pc}`);
      return;
    }
    const startId = Math.max(id - 4, 0);
    var result = "";
    for (var i = 0; i < NUM_ITEMS; ++i) {
      const instr = instructions[startId + i];
      result += instr.address == this.pc ? "> " : "  ";
      result += numToHex2(instr.address) + " ";
      const cmd = instr.mnemonic;
      result += cmd + this.spaces(6 - cmd.length);
      if (!instr.mnemonic.startsWith(".")) {
        result += instr.args.toString();
      } else {
        result += "[data]";
      }
      result += "\n";
    }
    this.root.text(result);
  }

  private spaces(n: number) {
    var result = "";
    for (var i = 0; i < n; ++i) {
      result += " ";
    }
    return result;
  }
}