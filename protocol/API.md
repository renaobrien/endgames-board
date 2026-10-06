# Endgames Board Device API, v1

Base URL: `https://endgam.es/.netlify/functions`

Every response carries the header `X-Endgames-Board-Api: 1`. Bodies are JSON.

## Pairing

The board shows a code. The owner types it on endgam.es. The board then receives a device token it keeps in flash.

### 1. Start: `POST /board-pair-start`

No auth, no body.

```json
200 { "code": "K7XQ2M", "pollSecret": "egp_...", "expiresAt": "2026-10-03T20:00:00Z", "claimUrl": "https://endgam.es/board/pair?code=K7XQ2M" }
429 { "error": "Too many pairing attempts" }
```

Show `code` as text and `claimUrl` as a QR code (and as text). The URL carries the code, so scanning it prefills the pairing page. Keep `pollSecret` private. Codes expire after 10 minutes.

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
    "timeControl": "5+0",
    "clock": { "incrementMs": 0, "yourMs": 241500, "opponentMs": 300000, "running": "you" },
    "endReason": null,
    "opponent": { "name": "magnus_fan", "isAi": false, "difficulty": null, "pfpUrl": "https://.../avatar.png" },
    "updatedAt": "2026-10-03T19:30:00Z"
  }
}
```

`moves` is every move so far in SAN, oldest first (up to 500), for the move list. **Clocks.** A game is timed or untimed. `timeControl` is the preset (`3+2`, `5+0`, `10+0`, `15+10`: minutes plus seconds added per move), or `null`. When it is `null`, `clock` is `null` too.

`clock.yourMs` and `clock.opponentMs` are the time each side has left **at the moment of the response**. The server has already subtracted the time the running clock has used. Start counting down when the response arrives, and replace your numbers on every response. `clock.running` is `"you"`, `"opponent"` or `null` (not started yet, or the game is over). `incrementMs` is added to a player's clock after each of their moves.

Neither clock runs until both players have made their first move. If either first move has not happened within 2 minutes, the game ends as `ABANDONED` with no rating change.

When a clock reaches zero the server ends the game: the player out of time loses, unless the other player cannot possibly checkmate, which is a draw. The board does not send anything for this. When your own countdown reaches zero, call `board-game` right away: the server settles the game and the response shows the result. The server gives a mover 1 second of grace per move for network delay.

`endReason` is `null` while the game is in progress. Afterwards it is one of `checkmate`, `draw` (stalemate and other draws), `resign`, `timeout`, `abandoned`. Games finished before clocks existed have `null`.

`opponent.isAi` is true in games against the computer, with `difficulty` set (`beginner`, `intermediate`, `advanced`, `expert`; computer games started on the website before difficulty was stored read as `intermediate`). For people, `difficulty` is `null`.

`game` is `null` when there is nothing to play. `status` is one of `IN_PROGRESS`, `WHITE_WON`, `BLACK_WON`, `DRAW`, `ABANDONED`.

`legalMoves` lists your moves in UCI form (`e2e4`, `e7e8q`) when it's your turn, and is empty otherwise. Use it to highlight squares after a tap.

Polling: every 10 seconds while waiting for the opponent (every 5 seconds in a timed game), every 60 seconds when idle. Redraw only when `moveCount` or `status` changes.

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

## Home, new games and sets (v1 additions)

The board is a full Endgames client: it starts games, lists them, and switches piece sets, the same as the website. These are additive v1 endpoints.

### Home screen: `GET /board-home`

One call for the home screen.

```json
200 {
  "apiVersion": 1,
  "profile": { "name": "deltajuliet", "pfpUrl": "https://.../avatar.png", "elo": 1240, "ranked": true,
               "wins": 31, "losses": 18, "draws": 3, "winRate": 60 },
  "games": [
    {
      "id": "uuid",
      "opponent": { "name": "queenbee", "isAi": false, "difficulty": null, "pfpUrl": "https://.../avatar.png" },
      "yourColor": "white",
      "yourTurn": true,
      "status": "IN_PROGRESS",
      "moveCount": 14,
      "timeControl": "5+0",
      "yourMs": 241500,
      "opponentMs": 300000,
      "running": "you",
      "updatedAt": "2026-10-04T19:30:00Z"
    }
  ],
  "incomingChallenges": [ { "id": "uuid", "from": { "name": "queenbee", "elo": 1820, "pfpUrl": "https://.../avatar.png" } } ],
  "activeSetId": "uuid-or-default",
  "sets": [ { "id": "uuid-or-default", "name": "Default", "kind": "default",
              "preview": { "wk": "https://.../wk.png", "wn": "https://.../wn.png" } } ]
}
```

In timed games the list items carry `timeControl`, `yourMs`, `opponentMs` and `running` (same meaning as `clock` in `board-game`); they are `null` in untimed games.

`games` holds your in-progress games (against people and the computer), your-turn first, then most recently updated, up to 20. You can have at most 20 in progress; `board-new-game` returns `429` above that. `incomingChallenges` lists direct challenges waiting for your answer (see `board-challenge-respond`). `sets` lists the built-in set plus sets you made. `elo` is `null` and `ranked` false before the first ranked game.

`profile.wins`, `losses` and `draws` count your finished games. `winRate` is wins over all finished games as a whole percent (the number the website's You screen shows), and `null` before your first finished game, when the three counts are `0`. Older servers leave all four out: treat a missing field as no stats.

`pfpUrl` is the player's profile picture: a public https link to a 256x256 PNG for pictures set on the website (Account, Profile picture). It is `null` when the player has not set one, and always `null` for the computer. Old accounts may hold a picture from another site (often a JPEG), so check the content type before decoding. Cache by URL: a new picture gets a new URL.

### Start a game: `POST /board-new-game`

Against the computer:

```json
{ "mode": "ai", "difficulty": "beginner", "first": "me" }
```

`difficulty` is `beginner`, `intermediate`, `advanced` or `expert` (shown as Easy, Medium, Hard, Expert). `first` is `me`, `computer` or `random` (who moves first). The older `color` field (`white`, `black`, `random`) is still accepted. Returns `200 { "apiVersion": 1, "game": { ... } }` in the `board-game` shape. If the computer plays white, its first move is already made.

Challenge a friend:

```json
{ "mode": "challenge", "color": "random" }
```

Creates an open challenge with your active set. `color` is the color you play (`random` picks one). Add `"timeControl": "5+0"` for a timed game (`3+2`, `5+0`, `10+0`, `15+10`); leave it out for untimed. Returns:

```json
200 { "apiVersion": 1, "challenge": { "id": "uuid", "url": "https://endgam.es/?challenge=uuid", "expiresAt": "..." } }
```

The board shows `url` as a QR. When someone accepts, the game appears in `board-home` and `board-game`.

Challenge a specific player (find them with `board-users`):

```json
{ "mode": "challenge", "opponentId": "uuid", "first": "me", "timeControl": "5+0" }
```

`timeControl` is optional (untimed when left out). It is rejected with `400` for computer games, which are always untimed, and for any value outside the list above.

`first` is `me`, `computer` or `random`; here `computer` means the other player moves first. Returns:

```json
200 { "apiVersion": 1, "challenge": { "id": "uuid", "opponent": "queenbee", "expiresAt": "..." } }
```

`404` if the player does not exist or is the computer, `400` if you challenge yourself, `409` if you already have a pending challenge with that player. The other player sees it in their `board-home` `incomingChallenges` (and on the website's Play tab). Challenges expire after 48 hours.

### Find a player: `GET /board-users?q=`

Username search for "Challenge a player". `q` needs at least 2 characters (shorter returns an empty list).

```json
200 { "apiVersion": 1, "users": [ { "id": "uuid", "name": "queenbee", "elo": 1820 } ] }
```

At most 10, highest rating first. Excludes you and the computer.

### Answer a challenge: `POST /board-challenge-respond`

```json
{ "id": "challenge-uuid", "accept": true }
```

Accept returns `200 { "apiVersion": 1, "game": { ... } }` in the `board-game` shape. Decline returns `200 { "apiVersion": 1, "declined": true }`. Only the challenged player can respond: anything else is `404`. `409` means the challenge already expired, was answered, or was withdrawn: refresh `board-home`.

### Quick match: `POST` and `GET /board-quick-match`

Pairs you with the next waiting player. Quick match games are timed (`10+0`) unless you send `"timeControl"` with `join`: a preset (`3+2`, `5+0`, `10+0`, `15+10`), or `null` (or the string `"untimed"`) for an untimed game. Leaving the field out means `10+0`. You are only paired with a player who asked for the same control, so untimed players are paired with untimed players. Anything else is rejected with `400`.

```json
POST { "action": "join" }    or    { "action": "cancel" }
```

Both `POST` and `GET` return the same shape:

```json
200 { "apiVersion": 1, "status": "waiting" }
200 { "apiVersion": 1, "status": "matched", "game": { ... } }
200 { "apiVersion": 1, "status": "idle" }
```

`join` pairs you with the oldest waiting player right away (`matched`), or queues you (`waiting`). Poll `GET` every 3 seconds while waiting. The first `GET` that returns `matched` delivers the `game` once and clears your queue row, so the next poll says `idle`. Queue entries last 5 minutes (then `idle`). Send `cancel` when the player backs out.

### Games against the computer

`board-game` and `board-game?gameId=` include computer games. After you move with `board-move`, the server makes the computer's reply before responding, so the returned `game` already includes it (or, if that takes longer than the request allows, the reply appears on the next `board-game` poll). Difficulty matches the website.

`sets[].preview` holds the white king and knight PNGs (the same 60x60 images `board-pieces` serves for that set) for the Sets tab.

### Leaderboard: `GET /board-leaderboard`

The same ranking the website shows, top 50.

```json
200 {
  "apiVersion": 1,
  "entries": [ { "rank": 1, "name": "queenbee", "elo": 1820, "isYou": false } ],
  "you": { "rank": 42, "elo": 1240 }
}
```

`you` is `null` before your first ranked game.

### Make a set

The board has no camera, so its Make tab shows a QR for `https://endgam.es/make`, which opens the website's Make tab (after sign-in) on the phone. New sets show up in `board-home`.

### Resign: `POST /board-resign`

```json
{ "gameId": "uuid" }
```

Returns the finished `game`. Resigning a game that's already over returns `409`.

### Switch piece set: `POST /board-set`

```json
{ "setId": "uuid-or-default" }
```

Sets your active set (the same one the website uses). `board-pieces` then returns it.

### Rename or delete a set: `POST /board-set-edit`

```json
{ "setId": "uuid", "action": "rename", "name": "Vaporwave Rena" }
{ "setId": "uuid", "action": "delete" }
```

Only your own sets. `"default"` can't be edited.

- `rename`: `name` is trimmed, 1 to 32 characters.
- `delete`: hides the set (`is_hidden = true`), the same as deleting on the website, so it can be restored there. If it was your active set, your active set goes back to the default.

Returns `200 { "ok": true, "activeSetId": "uuid-or-default" }`. `404` if the set doesn't exist, is already deleted, or isn't yours. `400` for a bad body or name.

## Versioning

Breaking changes get a new version number and run alongside the old one for at least 6 months. Adding a new field doesn't count as breaking, so ignore fields you don't recognize.
