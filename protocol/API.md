# Endgames Board Device API, v1

Base URL: `https://endgam.es/.netlify/functions`

Every response carries the header `X-Endgames-Board-Api: 1`. Bodies are JSON.

## Pairing

The board shows a code. The owner types it on endgam.es. The board then receives a device token it keeps in flash.

### 1. Start: `POST /board-pair-start`

No auth, no body.

```json
200 { "code": "K7XQ2M", "pollSecret": "egp_...", "expiresAt": "2026-10-03T20:00:00Z", "claimUrl": "https://endgam.es/board/pair" }
429 { "error": "Too many pairing attempts" }
```

Show `code` and `claimUrl` on screen. Keep `pollSecret` private. Codes expire after 10 minutes.

### 2. Owner claims the code

The owner signs in at endgam.es and enters the code. The board does nothing in this step.

### 3. Poll: `POST /board-pair-poll`

```json
{ "pollSecret": "egp_..." }
```

```json
202 { "status": "pending" }
200 { "status": "paired", "deviceToken": "egb_...", "deviceId": "uuid" }
410 { "error": "Pairing expired. Start again." }
```

Poll every 3 seconds. `deviceToken` is returned once. Store it, then stop polling.

## Playing

Send the device token on every call:

```
Authorization: Bearer egb_...
```

A `401` means the token was revoked or is unknown. Wipe it and start pairing again.

### Get the current game: `GET /board-game`

Optional query: `?gameId=uuid` to fetch a specific game (any status).

```json
200 {
  "apiVersion": 1,
  "game": {
    "id": "uuid",
    "fen": "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
    "yourColor": "white",
    "turn": "black",
    "yourTurn": false,
    "status": "IN_PROGRESS",
    "moveCount": 1,
    "lastMove": { "from": "e2", "to": "e4", "san": "e4" },
    "moves": ["e4"],
    "legalMoves": [],
    "opponent": { "name": "magnus_fan" },
    "updatedAt": "2026-10-03T19:30:00Z"
  }
}
```

`moves` is every move so far in SAN, oldest first (up to 500), for the move list. There are no clock fields: Endgames games are untimed today. If clocks are added they arrive as new optional fields.

`game` is `null` when there is nothing to play. `status` is one of `IN_PROGRESS`, `WHITE_WON`, `BLACK_WON`, `DRAW`, `ABANDONED`.

`legalMoves` lists your moves in UCI form (`e2e4`, `e7e8q`) when it's your turn, and is empty otherwise. Use it to highlight squares after a tap.

Polling: every 10 seconds while waiting for the opponent, every 60 seconds when idle. Redraw only when `moveCount` or `status` changes.

### Make a move: `POST /board-move`

```json
{ "gameId": "uuid", "from": "e7", "to": "e5", "moveCount": 1 }
```

Add `"promotion": "q"` (`q`, `r`, `b`, or `n`) when a pawn reaches the last rank. `moveCount` is the value from your last `board-game` response.

```json
200 { "apiVersion": 1, "game": { ... } }
409 { "error": "Board is out of date", "apiVersion": 1, "game": { ... } }
409 { "error": "Not your turn", ... }
409 { "error": "Game is over", ... }
422 { "error": "Illegal move", ... }
```

Every `409` and `422` includes the current `game`. Redraw from it.

The server checks legality on every move. `legalMoves` is there so the board can show where a piece can go.

### Get piece images: `GET /board-pieces`

Optional query: `?gameId=uuid` to use the piece set you have in that game. Without it, the board gets your active set, or the default set if you have none.

```json
200 {
  "apiVersion": 1,
  "set": { "id": "uuid-or-default-hash", "name": "My set", "kind": "custom" },
  "size": 60,
  "pieces": {
    "wk": "https://.../board/<set>/wk.png", "bk": "https://.../board/<set>/bk.png",
    "wq": "...", "bq": "...", "wr": "...", "br": "...",
    "wb": "...", "bb": "...", "wn": "...", "bn": "...", "wp": "...", "bp": "..."
  }
}
502 { "error": "Could not prepare piece images" }
```

Twelve PNGs, 60x60, transparent background. Keys are color (`w`/`b`) plus piece (`k q r b n p`). Black pieces match the website: the same art darkened with a thin light halo. The URLs are public and permanent per set, so download once per set and cache on flash or SD. The first call for a set can take a few seconds while the server resizes and caches the images. Fetch again when `set.id` changes.

## Versioning

Breaking changes get a new version number and run alongside the old one for at least 6 months. Adding a new field doesn't count as breaking, so ignore fields you don't recognize.
