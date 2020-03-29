#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_clipboard.h>

/* Emulator specific variables */
static int charCount = 0;
static char *pasteString;
static int pasteStringLength = 0;

int PasteManagerGetChar(unsigned char *character)
{
  if (charCount) {
    *character = pasteString[pasteStringLength - charCount];
    charCount--;
    if (charCount)
      return 1;
  }
  return 0;
}

int PasteManagerStartPaste(void)
{ 
  pasteString = SDL_GetClipboardText();
  pasteStringLength = strlen(pasteString);
  charCount = pasteStringLength;

  if (charCount) {
    return 1;
  } else {
    free(pasteString);
    return 0;
  }
}


void PasteManagerStartCopy(const char *string)
{
  SDL_SetClipboardText(string);
}
