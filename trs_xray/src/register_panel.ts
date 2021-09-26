import { ISUT_Registers} from "./data_structures";
import { numToHex } from "./utils";

const CARRY_MASK = 0x1;
const SUBTRACT_MASK = 0x2;
const OVERFLOW_MASK = 0x4;
const UNDOC3_MASK = 0x8;
const HALF_CARRY_MASK = 0x10;
const UNDOC5_MASK = 0x20;
const ZERO_MASK = 0x40;
const	SIGN_MASK = 0x80;

export class RegisterPanel {
  constructor() {
  }

  public update(registers: ISUT_Registers): void {
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
  }
}