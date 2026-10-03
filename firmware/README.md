# Firmware

Runs on the board. Connects to Wi-Fi, pairs with endgam.es, draws the position, and sends moves using the API in [`../protocol/API.md`](../protocol/API.md).

Never commit your Wi-Fi password or device token. `secrets.h` and `.env` files under `firmware/` are gitignored.

Build and flash steps land here with the first working version.
