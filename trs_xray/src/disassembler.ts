import { Disasm, Instruction } from "z80-disasm";
import { numToHex2 } from "./utils";

const NUM_ITEMS = 16;

export class Disassembler {
  private pc: number;
  private root: JQuery<Element>;

  private instructions: Array<Instruction>;

  constructor(elementId: string) {
    this.pc = 0;
    this.root = $(`#${elementId}`);
    this.instructions = new Array();
  }

  /**
   * Trigger disassembly and update internal model.
   *
   * @param memory memory to be loaded into the disassembler.
   */
  public disassemble(memory: Uint8Array): Array<string> {
    this.runDisassembly(memory);

    const dataType = new Array<string>(memory.length);
    for (var i = 0; i < this.instructions.length; ++i) {
      dataType[this.instructions[i].address] = this.instructions[i].mnemonic;
    }
    this.updateView(this.instructions);
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

  /**
   * Calculates and returnes possible values for the PC on the next step.
   */
  public predictNextPC(): Array<number> {
    // Find instruction at current PC.
    const instr = this.getInstructionAtAddress(this.pc);
    if (!instr) return [];

    // If there is no jump target, the only possible next PC value is right
    // after the current instruction.
    var result = [this.pc + instr.bin.length];

    // If the instruction is a jump, then that target branch is a second option.
    if (instr.jumpTarget) result.push(instr.jumpTarget);

    return result;
  }

  /**
   * O(n) search for the instruction.
   */
  private getInstructionAtAddress(addr: number): Instruction | undefined {
    for (var i = 0; i < this.instructions.length; ++i) {
      if (this.instructions[i].address == addr) return this.instructions[i];
    }
    return undefined;
  }

  private runDisassembly(memory: Uint8Array): void {
    const dis = new Disasm();
    dis.addChunk(memory, 0);
    dis.addEntryPoint(0);
    dis.addEntryPoint(this.pc);
    this.instructions = dis.disassemble();
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