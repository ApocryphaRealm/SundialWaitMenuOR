# Sundial Wait Menu (Oblivion Remastered)

The game's wait and sleep menu drawn as a round dial. A parchment face of its own, the hours you wait lit along the rim
from the current hour, the day's four marks on the rim (sunrise, noon, sunset, midnight) and a gilt sun at the current
hour. While the game waits, the lit hours tick away one by one from the start of the arc as the sun moves round.

It is a graphic replacer inside the game's own menu. The title, the hours slider, the texts and the buttons are the
game's own, so the controls and the text in every language are unchanged. Only the menu's flat panel and its straight
bar are hidden, and the dial is laid in under the texts.

## Requirements

- OBSE64 and Address Library for OBSE Plugins.

## Settings

`OBSE\Plugins\SundialWaitMenu.ini`: on/off, the dial's size, the gaps above and below it, brown ink for the text on the
face, and the log level. There is no settings page.

## How it works

- The menu is `/Game/UI/Modern/ModalLayer/WBP_ModernMenu_SleepWait`, with the view model `VSleepWaitMenuViewModel`. Both
  were read in game on 2026-10-01.
- When the view model says it is up and the widget is on screen, the plugin hides the drop shadow, the background
  tile's picture and the slider's bar (render opacity 0). The slider keeps working, unseen. The plugin then adds the
  dial (a size box holding an overlay of images) to the background tile's overlay, under the texts. The texts are
  spaced so the hours, the time and the date sit centred on the face, with the buttons below the dial.
- The hours on the rim come from the slider while you choose. Once the wait begins (the game hour jumps, or
  TimeToWait leaves its settled value), they come from TimeToWait, which counts down one hour per second.
- Hour `h` is drawn at `(h - 12) * 15` degrees clockwise from the top (noon at the top). Each lit hour is one copy of
  `Hour.png`, turned.
- The art is drawn by `tools/gen-art.py` (Pillow) and imported at run time through `ImportFileAsTexture2D`. No other
  author's art is used.

## TestBench

- `sundial.status`: the live state.
- `sundial.drive`:
  - `reapply` builds the dial and lays it out again.
  - `reloadArt` imports the pictures again.
  - `hours {value}` shows that many hours on the rim; `-1` returns to the menu's own hours.

## Building

```
git submodule update --init --recursive
xmake f -m releasedbg -y
xmake -y
python tools/gen-art.py
```

## Licence

GPL-3.0-or-later (it links CommonLibOB64, GPL-3.0). See `LICENSE`, `dist/NOTICE.md` and `dist/THIRD_PARTY_NOTICES.md`.
