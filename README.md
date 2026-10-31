# Switch Notes (Atmosphere homebrew)

A clean notes app with rounded cards, built with SDL2. Notes are saved as plain `.txt`
files in `sdmc:/switch/notes/`.

## Build
Push this folder to GitHub. The workflow in `.github/workflows/build.yml` builds
`switch-notes.nro` in the cloud; download it from the run's Artifacts section.

## Install
Copy `switch-notes.nro` to `/switch/switch-notes/switch-notes.nro` on the SD card.

## Controls
L/R switch between the Notes and Settings tabs, + quit.
- Notes tab: Up/Down select, A edit, X new note, Y delete (press twice), ZL/ZR or right stick scroll the open note.
- Settings tab: Up/Down choose a theme (Light, Dark, Amber, Forest). The choice applies instantly and is saved to `/config/switch-notes/settings.ini`.

## Font
Uses the Switch's built-in sans font. To use another font, put a `.ttf` file at
`sdmc:/config/switch-notes/font.ttf`.

Each note is limited to 1000 characters (`MAX_CHARS` in `main.cpp`).

## Overlay (Tesla)
`overlay/` builds `notes-overlay.ovl`, a read-only Tesla overlay for browsing your notes
while a game is running. Copy it to `/switch/.overlays/` on the SD card and open the
Tesla menu (default: L + D-pad Down + right stick click). Overlays can't open the system
keyboard, so writing and editing still happen in the main app. Requires Tesla Menu
(nx-ovlloader) to be installed.
