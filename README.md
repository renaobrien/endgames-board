# Endgames Board

An open-source touchscreen chess board you can print and build at home. It plays your live games from [endgam.es](https://endgam.es) in full color, with your custom piece set on the board.

> Status: early. The reference board is on order and the device API is built. The case, wiring and firmware land here as the first build comes together.

## How it works

1. The board shows a 6-character pairing code.
2. You enter the code at endgam.es/board/pair while signed in.
3. The board picks up your current game. Tap a piece, tap where it goes.

Your opponent can be on the web, the app, or another board. Games against the in-app AI stay in the app.

## What's in this repo

| Folder | What it holds |
|---|---|
| [`docs/`](docs/) | Build guide |
| [`hardware/`](hardware/) | Printable case files (STL/3MF), source CAD, print settings |
| [`electronics/`](electronics/) | Parts list and wiring |
| [`firmware/`](firmware/) | Code that runs on the board |
| [`protocol/`](protocol/) | The device API the board uses to talk to endgam.es |

## The reference build

This repo documents one build: the one I made. Every section of the build guide says what I used, then what changes if you use something else. If you get it working on different parts, open a pull request and add your notes.

## Licenses

- Hardware (case, CAD, wiring, parts list): [CERN-OHL-S v2](LICENSE-HARDWARE)
- Firmware and code: [MIT](LICENSE)
- The Endgames name and logo: see [TRADEMARK.md](TRADEMARK.md)
