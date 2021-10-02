/** Context about the system under test (SUT) */
export interface ISUT_Context {
  system_name: string;
  model: number;
  running: boolean;
  alt_single_step_mode: boolean;
}

/** Register information from the SUT. */
export interface ISUT_Registers {
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

/** Breakpoint information. */
export interface ISUT_Breakpoint {
  id: number,
  address: number,
  type: number,
}

export interface TRS80GP_Import {
  AF: number,
  AFp: number,
  BC: number,
  BCp: number,
  DE: number,
  DEp: number,
  HL: number,
  HLp: number,
  I: number,
  IFF1: number,
  IFF2: number,
  IM: number,
  IRQ: number,
  IRQon: number,
  IX: number,
  IY: number,
  PC: number,
  R: number,
  SP: number,
  mem: Array<number>,
  port236: number
}
