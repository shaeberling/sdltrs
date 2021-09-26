
const VIDEO_RAM_OFFSET =  0x3C00;
const M3_TO_UTF = [
  "\u0020", "\u00a3", "\u007c", "\u00e9", "\u00dc", "\u00c5", "\u00ac", "\u00f6",
  "\u00d8", "\u00f9", "\u00f1", "\u0060", "\u0101", "\ue00d", "\u00c4", "\u00c3",
  "\u00d1", "\u00d6", "\u00d8", "\u00d5", "\u00df", "\u00fc", "\u00f5", "\u00e6",
  "\u00e4", "\u00e0", "\u0227", "\ue01b", "\u00c9", "\u00c6", "\u00c7", "\u02dc",
  "\u0020", "\u0021", "\u0022", "\u0023", "\u0024", "\u0025", "\u0026", "\u0027",
  "\u0028", "\u0029", "\u002a", "\u002b", "\u002c", "\u002d", "\u002e", "\u002f",
  "\u0030", "\u0031", "\u0032", "\u0033", "\u0034", "\u0035", "\u0036", "\u0037",
  "\u0038", "\u0039", "\u003a", "\u003b", "\u003c", "\u003d", "\u003e", "\u003f",
  "\u0040", "\u0041", "\u0042", "\u0043", "\u0044", "\u0045", "\u0046", "\u0047",
  "\u0048", "\u0049", "\u004a", "\u004b", "\u004c", "\u004d", "\u004e", "\u004f",
  "\u0050", "\u0051", "\u0052", "\u0053", "\u0054", "\u0055", "\u0056", "\u0057",
  "\u0058", "\u0059", "\u005a", "\u005b", "\u005c", "\u005d", "\u005e", "\u005f",
  "\u0060", "\u0061", "\u0062", "\u0063", "\u0064", "\u0065", "\u0066", "\u0067",
  "\u0068", "\u0069", "\u006a", "\u006b", "\u006c", "\u006d", "\u006e", "\u006f",
  "\u0070", "\u0071", "\u0072", "\u0073", "\u0074", "\u0075", "\u0076", "\u0077",
  "\u0078", "\u0079", "\u007a", "\u007b", "\u007c", "\u007d", "\u007e", "\u00b1",
  "\ue080", "\ue081", "\ue082", "\ue083", "\ue084", "\ue085", "\ue086", "\ue087",
  "\ue088", "\ue089", "\ue08a", "\ue08b", "\ue08c", "\ue08d", "\ue08e", "\ue08f",
  "\ue090", "\ue091", "\ue092", "\ue093", "\ue094", "\ue095", "\ue096", "\ue097",
  "\ue098", "\ue099", "\ue09a", "\ue09b", "\ue09c", "\ue09d", "\ue09e", "\ue09f",
  "\ue0a0", "\ue0a1", "\ue0a2", "\ue0a3", "\ue0a4", "\ue0a5", "\ue0a6", "\ue0a7",
  "\ue0a8", "\ue0a9", "\ue0aa", "\ue0ab", "\ue0ac", "\ue0ad", "\ue0ae", "\ue0af",
  "\ue0b0", "\ue0b1", "\ue0b2", "\ue0b3", "\ue0b4", "\ue0b5", "\ue0b6", "\ue0b7",
  "\ue0b8", "\ue0b9", "\ue0ba", "\ue0bb", "\ue0bc", "\ue0bd", "\ue0be", "\ue0bf",
  "\u2660", "\u2665", "\u2666", "\u2663", "\u263a", "\u2639", "\u2264", "\u2265",
  "\u03b1", "\u03b2", "\u03b3", "\u03b4", "\u03b5", "\u03b6", "\u03b7", "\u03b8",
  "\u03b9", "\u03ba", "\u03bc", "\u03bd", "\u03be", "\u03bf", "\u03c0", "\u03c1",
  "\u03c2", "\u03c3", "\u03c4", "\u03c5", "\u03c6", "\u03c7", "\u03c8", "\u03c9",
  "\u2126", "\u221a", "\u00f7", "\u2211", "\u2248", "\u2206", "\u2307", "\u2260",
  "\u2301", "\ue0e9", "\u237e", "\u221e", "\u2713", "\u00a7", "\u2318", "\u00a9",
  "\u00a4", "\u00b6", "\u00a2", "\u00ae", "\ue0f4", "\ue0f5", "\ue0f6", "\u211e",
  "\u2105", "\u2642", "\u2640", "\ue0fb", "\ue0fc", "\ue0fd", "\ue0fe", "\u2302"
];

/** Renders contents of a TRS-80 screen */
export class Screen {
  /** Number of characters horizontally. */
  private width: number = 64;

  /** Number of characters vertically. */
  private height: number = 16;

  /** Root element of the screen. */
  private root: JQuery<Element>;

  /** Whether the mouse is currently hover on the screen area. */
  private mouseOnScreen: boolean = false;
  constructor(elementId: string) {
    this.root = $(`#${elementId}`);
    this.root.on("mouseenter", () => {this.mouseOnScreen = true})
             .on("mouseleave", () => {this.mouseOnScreen = false});
  }

  /**
   * Renders/updates the screen contents.
   *
   * @param memory total TRS memory, used to extract screen contents
   */
  public render(memory: Uint8Array): void {
    // console.time("renderDisplay");
    if (!memory) return;

    // TODO: Need to determine when we have 64x16 or 80x24
    let screenStr = "";
    for (let y = 0; y < this.height; ++y) {
      for (let x = 0; x < this.width; ++x) {
        screenStr += M3_TO_UTF[memory[y * this.width + x + VIDEO_RAM_OFFSET]];
      }
      screenStr += "\n";
    }
    this.root.text(screenStr);
    // console.timeEnd("renderDisplay");
  }

  public isMouseOnScreen(): boolean {
    return this.mouseOnScreen;
  }
}