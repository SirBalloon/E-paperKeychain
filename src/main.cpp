/*
 * E-Paper Server for WeAct 2.9" EPD Display
 */

#include "config.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// 2.9'' EPD Module - Considered a Template Class
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)); // GDEM029C90 128x296, SSD1680

// Sets up the Server on port 80
AsyncWebServer server(80);

// Buffer to store uploaded image data. Each pixel is 1 bit, we can pack 8 bits into a byte so the image (296*128 = 37,888)/8 = 4736 bytes.
// Which is much more efficient then storing each pixel as a full byte or 4-byte RGBA Value
const size_t IMAGE_BUFFER_SIZE = (DISPLAY_WIDTH * DISPLAY_HEIGHT) / 8; // 4,736 bytes
uint8_t *imageBuffer = nullptr;
size_t totalReceived = 0;
bool imageReadyToDisplay = false;

// This Function uploads the image onto the e-paper display
void displayUploadedImage(uint8_t *data, size_t len)
{
  Serial.println("Displaying image on e-paper...");

  if (len != IMAGE_BUFFER_SIZE)
  {
    Serial.printf("Error: Expected %d bytes, got %d bytes\n", IMAGE_BUFFER_SIZE, len);
    return;
  }

  display.setFullWindow();
  display.firstPage();

  do
  {
    display.fillScreen(GxEPD_WHITE);

    for (int y = 0; y < DISPLAY_HEIGHT; y++)
    {
      for (int x = 0; x < DISPLAY_WIDTH; x++)
      {
        int byteIdx = y * ((DISPLAY_WIDTH + 7) / 8) + (x / 8);
        int bitIdx = 7 - (x % 8);

        if (data[byteIdx] & (1 << bitIdx))
        {
          display.drawPixel(x, y, GxEPD_BLACK);
        }
      }

      if (y % 10 == 0)
      {
        yield();
      }
    }
  } while (display.nextPage());

  Serial.println("Image displayed successfully!");
}

// This Function sets up the initial E-paper display allowing the user experience to start
void setup()
{
  Serial.begin(115200);
  Serial.println("\n=== Project E-paper Gift ===");

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);

  display.init(115200);
  display.setRotation(1);

  imageBuffer = (uint8_t *)malloc(IMAGE_BUFFER_SIZE);
  if (imageBuffer == nullptr)
  {
    Serial.println("Failed to allocate image buffer!");
  }

  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.print("Connect to WiFi: ");
  Serial.println(ssid);
  Serial.println("Then visit: http://192.168.4.1");

  LittleFS.begin();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html", "text/html"); });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/style.css", "text/css"); });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/script.js", "application/javascript"); });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "OK"); }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
      if (index == 0) {
        totalReceived = 0;
        Serial.printf("Upload started, expecting %d bytes\n", total);
      }

      if (imageBuffer != nullptr && (totalReceived + len) <= IMAGE_BUFFER_SIZE) {
        memcpy(imageBuffer + totalReceived, data, len);
        totalReceived += len;
        Serial.printf("Received chunk: %d bytes (total: %d / %d)\n", len, totalReceived, total);
      }

      if (index + len == total) {
        Serial.printf("Upload complete! Total size: %d bytes\n", totalReceived);

        if (totalReceived == IMAGE_BUFFER_SIZE) {
          imageReadyToDisplay = true;
        }
      } });

  server.begin();

  Serial.println("Updating display...");
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(10, 30);
    display.println("Connect to:");
    display.setCursor(10, 60);
    display.println(ssid);
    display.setCursor(10, 90);
    display.println("192.168.4.1");
  } while (display.nextPage());

  Serial.println("Setup complete!");
}

// This is the server loop which looks for if there are images to display
void loop()
{
  if (imageReadyToDisplay)
  {
    imageReadyToDisplay = false;
    displayUploadedImage(imageBuffer, totalReceived);
  }
  delay(100);
}
