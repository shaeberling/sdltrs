import { addModel3RomEntryPoints, disasmForTrs80 } from "trs80-disasm";
import { Disasm, Instruction } from "z80-disasm";
import { numToHex2 } from "./utils";

const NUM_ITEMS = 16;

var counter = 0;

declare global {
  interface Window { dbg_memory: Uint8Array; }
}

export class Disassembler {
  private pc: number;
  private root: JQuery<Element>;

  private instructions: Array<Instruction>;
  private labelToAddr: Map<String, number>;

  constructor(elementId: string) {
    this.pc = 0;
    this.root = $(`#${elementId}`);
    this.instructions = new Array();
    this.labelToAddr = new Map();
  }

  /**
   * Trigger disassembly and update internal model.
   *
   * @param memory memory to be loaded into the disassembler.
   */
  public disassemble(memory: Uint8Array): Array<string> {
    // console.log("runDisassembly ..." + counter++);
    // window.dbg_memory = memory;
    // console.time("runDisassembly");
    this.runDisassembly(memory);
    // console.timeEnd("runDisassembly");

    const dataType = new Array<string>(memory.length);
    for (var instr of this.instructions) {
      const addr = instr.address;
      dataType[addr] = instr.mnemonic;
      // Manually mark video ram as data.
      if (addr >= 0x3C00 && addr <= 0x3FFF) dataType[addr] = ".byte";
      if (instr.label) this.labelToAddr.set(instr.label, instr.address);
    }
    this.updateView(this.instructions);
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
    if (instr == undefined) {
      console.error(`BUG: No instruction at current PC ${numToHex2(this.pc)}`);
      return [];
    }

    var result = [];

    // If this instruction would continue to the next one, add that to the list.
    if (instr.continues()) result.push(this.pc + instr.bin.length);

    // If the instruction is a jump, then that target branch is a second option.
    if (instr.jumpTarget != undefined) result.push(instr.jumpTarget);

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
    const disasm = disasmForTrs80();
    addModel3RomEntryPoints(disasm);

    disasm.addChunk(memory, 0);
    disasm.addEntryPoint(0);
    disasm.addEntryPoint(this.pc);
    this.instructions = disasm.disassemble();
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
      if (!instr) {
        result += " ...\n"
        continue;
      }

      result += instr.address == this.pc ? ">" : " ";
      result += numToHex2(instr.address) + " ";
      const cmd = instr.mnemonic;
      result += cmd + this.spaces(6 - cmd.length);
      if (!instr.mnemonic.startsWith(".")) {
        // Replace labels with addresses.
        for (var a = 0; a < instr.args.length; ++a) {
          if (this.labelToAddr.has(instr.args[a])) {
            instr.args[a] =
                "0x"+ numToHex2(this.labelToAddr.get(instr.args[a]) as number);
          }
        }
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