export const numToHex = (num: number) => ((num <= 0xF ? "0" : "") + num.toString(16)).toUpperCase();
export const numToHex2 = (num: number) => {
  let n1 = numToHex((num & 0xFF00) >> 8);
  let n2 = numToHex((num & 0x00FF));
  return n1 + n2;
}
