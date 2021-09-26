export const numToHex = (num: number) => ((num <= 0xF ? "0" : "") + num.toString(16)).toUpperCase();
