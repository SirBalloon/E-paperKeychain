# E-Paper Server — Initial Code Documentation

This document describes the architecture and behaviour of the initial `E-PaperServer.ino` as written before any improvements were made. It is intended as a reference point for understanding the baseline before iterating.

---

## Libraries & Dependencies

| Library | Purpose |
|---|---|
| `GxEPD2_3C` | Driver for 3-colour GxEPD2 e-paper displays |
| `Fonts/FreeMonoBold12pt7b` | Bitmap font used on the startup screen |
| `WiFi.h` | ESP32 WiFi stack (access point mode) |
| `ESPAsyncWebServer` | Non-blocking async HTTP server |
| `AsyncTCP` | Async TCP layer required by ESPAsyncWebServer |

---

## Pin Definitions

| Macro | GPIO | E-Paper Signal |
|---|---|---|
| `EPD_MOSI` | 11 | DIN (SPI data) |
| `EPD_SCK` | 12 | CLK (SPI clock) |
| `EPD_CS` | 10 | CS (chip select) |
| `EPD_DC` | 9 | DC (data/command) |
| `EPD_RST` | 8 | RST (reset) |
| `EPD_BUSY` | 7 | BUSY |

> Note: The wiring table in `DetailedCreation.md` lists slightly different GPIO numbers. The `.ino` file is authoritative for the firmware; cross-check before wiring.

---

## Display Configuration

```cpp
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(
    GxEPD2_290_C90c(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);
```

- Driver: `GxEPD2_290_C90c` (GDEM029C90, SSD1680 controller)
- Resolution: 296 × 128 pixels
- Colour support: black, white, red (3-colour), though the initial code only uses black/white
- Rotation: set to `1` in `setup()` (landscape)

---

## Global State

| Variable | Type | Description |
|---|---|---|
| `imageBuffer` | `uint8_t*` | Heap-allocated buffer holding the received 1-bit image |
| `IMAGE_BUFFER_SIZE` | `const size_t` | `(296 × 128) / 8 = 4,736 bytes` |
| `totalReceived` | `size_t` | Running byte count during an active upload |
| `imageReadyToDisplay` | `bool` | Flag set when a complete, valid image has been received |

The buffer is allocated once in `setup()` via `malloc`. There is no free/realloc — the same buffer is reused for every upload.

---

## WiFi — Access Point Mode

```cpp
WiFi.softAP(ssid, password);
// SSID:     "YOUR_SSID_HERE"
// Password: "YOUR_PASSWORD_HERE"
// IP:       192.168.4.1 (ESP32 default soft-AP address)
```

The device creates its own WiFi network rather than joining an existing one. Any client that connects to this network and navigates to `http://192.168.4.1` reaches the upload interface.

---

## HTTP Server Routes

### `GET /`
Returns the embedded HTML page (`index_html`, stored in `PROGMEM`).

### `POST /upload`
Receives a raw binary payload (1-bit packed image data) via chunked body callbacks.

**Body handler logic:**
1. On `index == 0` — resets `totalReceived` to 0 and logs the expected size.
2. On each chunk — copies the chunk into `imageBuffer` (provided the buffer exists and no overflow would occur).
3. On final chunk (`index + len == total`) — if exactly `IMAGE_BUFFER_SIZE` bytes were received, sets `imageReadyToDisplay = true`.

The completion handler (called after body processing) immediately sends `200 OK` with body `"OK"`.

---

## Web Interface (Embedded HTML/JS)

The page is a single self-contained HTML file stored in `PROGMEM` as a raw string literal.

### UI Flow

1. User clicks the upload area (or drag-and-drops) to select an image file.
2. The image is read via `FileReader` and drawn onto a hidden `<canvas>` (296 × 128).
3. The canvas image is processed client-side and a preview is shown.
4. User clicks **Upload to Display** — the packed binary is POSTed to `/upload`.

### Client-Side Image Processing

**Step 1 — Scale & centre (`processImage`)**
- The image is scaled to fit within 296 × 128 while preserving aspect ratio (`Math.min` of width/height scale factors).
- The canvas is filled white first, then the image is drawn centred.

**Step 2 — Floyd-Steinberg dithering (`floydSteinbergDithering`)**
- Converts to greyscale: `gray = 0.299R + 0.587G + 0.114B` (standard luminance weights).
- Applies Floyd-Steinberg error diffusion:
  - Threshold: pixels < 128 → black (0), ≥ 128 → white (255).
  - Error distributed to right (7/16), bottom-left (3/16), below (5/16), bottom-right (1/16).
- Returns a 1-bit-equivalent RGBA `ImageData` (pixels are either 0 or 255).

**Step 3 — Bit packing (`packImageData`)**
- Iterates over every pixel; black pixels (< 128) set their corresponding bit in a `Uint8Array`.
- Bit order: MSB first within each byte (`bitIdx = 7 - (x % 8)`).
- Output size: `ceil(296/8) × 128 = 37 × 128 = 4,736 bytes`.

**Step 4 — Upload**
- POSTs the `Uint8Array` as `application/octet-stream` using `fetch`.
- Button is disabled during transfer; success/error state is shown in a status banner.

---

## `displayUploadedImage(uint8_t* data, size_t len)`

Called from `loop()` when `imageReadyToDisplay` is true.

1. Validates that `len == IMAGE_BUFFER_SIZE` (4,736 bytes); aborts with a serial log if not.
2. Calls `display.setFullWindow()` and enters the GxEPD2 page loop (`firstPage` / `nextPage`).
3. Fills the screen white, then iterates every pixel:
   - Calculates `byteIdx` and `bitIdx` from `(x, y)`.
   - Draws a black pixel if the corresponding bit is set.
   - Calls `yield()` every 10 rows to prevent watchdog resets.

---

## `setup()`

1. Starts `Serial` at 115200 baud.
2. Initialises SPI (`EPD_SCK`, MISO=-1, `EPD_MOSI`, `EPD_CS`).
3. Initialises and rotates the display.
4. Allocates `imageBuffer` on the heap.
5. Starts the WiFi soft-AP.
6. Registers the `/` and `/upload` route handlers and calls `server.begin()`.
7. Renders a startup screen showing the SSID and IP address so the user knows how to connect.

---

## `loop()`

```cpp
void loop() {
    if (imageReadyToDisplay) {
        imageReadyToDisplay = false;
        displayUploadedImage(imageBuffer, totalReceived);
    }
    delay(100);
}
```

Polls the `imageReadyToDisplay` flag every 100 ms. When set, clears the flag and triggers the display update. All HTTP handling happens asynchronously in the background via `ESPAsyncWebServer` interrupt/task callbacks.

---

## Data Flow Summary

```
Browser                          ESP32
───────                          ─────
Select image
  │
  ├─ Scale & centre (canvas)
  ├─ Floyd-Steinberg dither
  ├─ Pack to 1-bit (4736 bytes)
  │
  POST /upload  ──────────────►  Body callback (chunked)
                                   memcpy → imageBuffer
                                   imageReadyToDisplay = true
  200 OK        ◄──────────────  Completion handler
                                   (loop picks up flag)
                                   displayUploadedImage()
                                   GxEPD2 page render
```

---

## Known Limitations (as of initial version)

- **Black/white only** — the 3-colour display's red channel is unused.
- **Hardcoded credentials** — SSID and password are plaintext in the source.
- **No error recovery** — if the buffer allocation fails, the server still starts but uploads silently corrupt.
- **Single image** — no persistent storage; the image is lost on power cycle.
- **No size validation on the client** — the browser will send whatever `packImageData` produces; a mismatch only errors on the ESP32 side.
