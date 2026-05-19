/*
 * E-Paper Server for WeAct 2.9" EPD Display
 */

#include "config.h"
#include "qr_display.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <GxEPD2_3C.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

// 2.9'' EPD Module - Considered a Template Class
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)); // GDEM029C90 128x296, SSD1680

// Sets up the Server on port 80
AsyncWebServer server(80);
DNSServer dnsServer;
Preferences pref;
bool isAPMode = false;
bool shouldRestart = false;

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

// Saving Web Credentials
void saveCreds(const String &ssid, const String &pass)
{
  pref.begin("wifi", false);
  pref.putString("ssid", ssid);
  pref.putString("pass", pass);
  pref.end();
}

// Loading Web Credentials
bool loadCreds(String &ssid, String &pass)
{
  pref.begin("wifi", true);
  ssid = pref.getString("ssid", "");
  pass = pref.getString("pass", "");
  pref.end();
  return ssid.length() > 0;
}

// Clearing Web Credentials
void clearCreds()
{
  pref.begin("wifi", false);
  pref.clear();
  pref.end();
  Serial.println("Credentials cleared");
}

// This Function sets up the initial E-paper display allowing the user experience to start
void setup()
{
  Serial.begin(115200);
  Serial.println("\n=== Project E-paper Gift ===");

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);

  display.init(115200);
  display.setRotation(1);

  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  if (digitalRead(BOOT_BUTTON) == LOW)
  {
    clearCreds();
  }

  imageBuffer = (uint8_t *)malloc(IMAGE_BUFFER_SIZE);
  if (imageBuffer == nullptr)
  {
    Serial.println("Failed to allocate image buffer!");
  }

  LittleFS.begin();

  // Saved Web Credentials
  String savedSSID, savedPass;

  // loads the credentials and starts the wifi, gives 20 attempts before failing
  if (loadCreds(savedSSID, savedPass))
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
      delay(500);
      attempts++;
      Serial.print(".");
    }
    Serial.println();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    isAPMode = false;

    MDNS.begin("epaper");
    MDNS.addService("http", "tcp", 80);

    Qr_display(display, "http://epaper.local", "epaper.local", "Visit:");

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
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
  }
  else
  {
    isAPMode = true;
    Serial.println("No credentials or connection failed — starting captive portal");

    // Starts AP and DNS Server
    WiFi.mode(WIFI_AP);
    WiFi.softAP("KeychainSetup");
    dnsServer.start(53, "*", WiFi.softAPIP());

    Qr_display(display, "http://192.168.4.1", "KeychainSetup", "Connect to:");

    // Captive portal detection endpoints for Android, IOS/MacOS, Windows
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->redirect("/"); });

    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->redirect("/"); });

    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->redirect("/"); });

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html",
                              "<!DOCTYPE html><html><head><title>Keychain Setup</title></head>"
                              "<body><h2>Keychain Setup</h2>"
                              "<form action='/save' method='POST'>"
                              "Hotspot Name:<br><input name='ssid' type='text'><br><br>"
                              "Password:<br><input name='pass' type='password'><br><br>"
                              "<input type='submit' value='Connect'>"
                              "</form></body></html>") });
    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request)
              {
      if(request->hasParam("ssid", true) && request->hasParam("pass", true)){
        String newSSID = request->getParam("ssid", true)->value();
        String newPass = request->getParam("pass", true)->value();
        saveCreds(newSSID, newPass);
        request->send(200, "text/html", "<h2>Saved! Device is restarting...</h2>");
        shouldRestart = true;
      }else{
        request->send(400, "text/plain", "Missing fields");
      } });
  }
  server.begin();
}

// This is the server loop which looks for if there are images to display
void loop()
{
  if (isAPMode)
  {
    dnsServer.processNextRequest();
  }

  if (shouldRestart)
  {
    delay(1000);
    ESP.restart();
  }

  if (imageReadyToDisplay)
  {
    imageReadyToDisplay = false;
    displayUploadedImage(imageBuffer, totalReceived);
  }
  delay(100);
}
