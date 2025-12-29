/*
 * E-Paper Server for WeAct 2.9" EPD Display
 */

#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// Pin definitions
#define EPD_MOSI  11
#define EPD_SCK   12
#define EPD_CS    10
#define EPD_DC    9
#define EPD_RST   8
#define EPD_BUSY  7

#define DISPLAY_WIDTH  296
#define DISPLAY_HEIGHT 128

// 2.9'' EPD Module
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)); // GDEM029C90 128x296, SSD1680

const char* ssid = "HoneuuuCinnamonRoll";
const char* password = "Password1234";

AsyncWebServer server(80);

// Buffer to store uploaded image data
const size_t IMAGE_BUFFER_SIZE = (DISPLAY_WIDTH * DISPLAY_HEIGHT) / 8; // 4,736 bytes
uint8_t* imageBuffer = nullptr;
size_t totalReceived = 0;
bool imageReadyToDisplay = false;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
  <head>
    <title>Surprise Gift!</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" href="data:,">
    <style>
      * {
        margin: 0;
        padding: 0;
        box-sizing: border-box;
      }
      
      body {
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        min-height: 100vh;
        display: flex;
        justify-content: center;
        align-items: center;
        padding: 20px;
      }
      
      .container {
        background: white;
        padding: 40px;
        border-radius: 20px;
        box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        max-width: 500px;
        width: 100%;
        text-align: center;
      }
      
      h1 {
        color: #667eea;
        margin-bottom: 10px;
        font-size: 2em;
      }
      
      .subtitle {
        color: #666;
        margin-bottom: 30px;
        font-size: 0.9em;
      }
      
      .upload-area {
        border: 3px dashed #667eea;
        border-radius: 15px;
        padding: 40px 20px;
        margin: 20px 0;
        cursor: pointer;
        transition: all 0.3s ease;
        background: #f8f9ff;
      }
      
      .upload-area:hover {
        border-color: #764ba2;
        background: #f0f2ff;
        transform: translateY(-2px);
      }
      
      .upload-icon {
        font-size: 3em;
        margin-bottom: 10px;
      }
      
      .upload-text {
        color: #667eea;
        font-weight: 600;
        margin-bottom: 5px;
      }
      
      .upload-hint {
        color: #999;
        font-size: 0.85em;
      }
      
      input[type="file"] {
        display: none;
      }
      
      .preview-container {
        margin: 20px 0;
        display: none;
      }
      
      .preview-image {
        max-width: 100%;
        border-radius: 10px;
        box-shadow: 0 5px 15px rgba(0,0,0,0.2);
        image-rendering: pixelated;
      }
      
      .button {
        background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        color: white;
        border: none;
        padding: 15px 40px;
        border-radius: 25px;
        font-size: 1em;
        font-weight: 600;
        cursor: pointer;
        transition: all 0.3s ease;
        margin-top: 20px;
        width: 100%;
      }
      
      .button:hover {
        transform: translateY(-2px);
        box-shadow: 0 10px 20px rgba(102, 126, 234, 0.4);
      }
      
      .button:active {
        transform: translateY(0);
      }
      
      .button:disabled {
        background: #ccc;
        cursor: not-allowed;
      }
      
      .status {
        margin-top: 20px;
        padding: 10px;
        border-radius: 10px;
        display: none;
      }
      
      .status.success {
        background: #d4edda;
        color: #155724;
        display: block;
      }
      
      .status.error {
        background: #f8d7da;
        color: #721c24;
        display: block;
      }
      
      .status.processing {
        background: #fff3cd;
        color: #856404;
        display: block;
      }
      
      canvas {
        display: none;
      }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>Surprise Gift!</h1>
      <p class="subtitle">Upload an image to display on the e-paper screen</p>
      
      <div class="upload-area" id="uploadArea">
        <div class="upload-text">Click to upload image</div>
        <div class="upload-hint">or drag and drop here</div>
      </div>
      
      <input type="file" id="fileInput" accept="image/*">
      
      <div class="preview-container" id="previewContainer">
        <img id="previewImage" class="preview-image" alt="Preview">
      </div>
      
      <button class="button" id="uploadButton" disabled>Upload to Display</button>
      
      <div class="status" id="status"></div>
    </div>
    
    <canvas id="canvas"></canvas>
    
    <script>
      const DISPLAY_WIDTH = 296;
      const DISPLAY_HEIGHT = 128;
      
      const uploadArea = document.getElementById('uploadArea');
      const fileInput = document.getElementById('fileInput');
      const previewContainer = document.getElementById('previewContainer');
      const previewImage = document.getElementById('previewImage');
      const uploadButton = document.getElementById('uploadButton');
      const status = document.getElementById('status');
      const canvas = document.getElementById('canvas');
      const ctx = canvas.getContext('2d');
      
      let processedImageData = null;
      
      uploadArea.addEventListener('click', () => {
        fileInput.click();
      });
      
      uploadArea.addEventListener('dragover', (e) => {
        e.preventDefault();
        uploadArea.style.borderColor = '#764ba2';
        uploadArea.style.background = '#f0f2ff';
      });
      
      uploadArea.addEventListener('dragleave', () => {
        uploadArea.style.borderColor = '#667eea';
        uploadArea.style.background = '#f8f9ff';
      });
      
      uploadArea.addEventListener('drop', (e) => {
        e.preventDefault();
        uploadArea.style.borderColor = '#667eea';
        uploadArea.style.background = '#f8f9ff';
        
        const files = e.dataTransfer.files;
        if (files.length > 0) {
          handleFile(files[0]);
        }
      });
      
      fileInput.addEventListener('change', (e) => {
        if (e.target.files.length > 0) {
          handleFile(e.target.files[0]);
        }
      });
      
      function handleFile(file) {
        if (!file.type.startsWith('image/')) {
          showStatus('Please select an image file', 'error');
          return;
        }
        
        showStatus('Processing image...', 'processing');
        
        const reader = new FileReader();
        reader.onload = (e) => {
          const img = new Image();
          img.onload = () => {
            processImage(img);
          };
          img.src = e.target.result;
        };
        reader.readAsDataURL(file);
      }
      
      function processImage(img) {
        canvas.width = DISPLAY_WIDTH;
        canvas.height = DISPLAY_HEIGHT;
        
        const scale = Math.min(DISPLAY_WIDTH / img.width, DISPLAY_HEIGHT / img.height);
        const scaledWidth = img.width * scale;
        const scaledHeight = img.height * scale;
        const offsetX = (DISPLAY_WIDTH - scaledWidth) / 2;
        const offsetY = (DISPLAY_HEIGHT - scaledHeight) / 2;
        
        ctx.fillStyle = 'white';
        ctx.fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        ctx.drawImage(img, offsetX, offsetY, scaledWidth, scaledHeight);
        
        const imageData = ctx.getImageData(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        const dithered = floydSteinbergDithering(imageData);
        
        ctx.putImageData(dithered, 0, 0);
        previewImage.src = canvas.toDataURL();
        previewContainer.style.display = 'block';
        
        processedImageData = packImageData(dithered);
        
        uploadButton.disabled = false;
        showStatus('Image processed! Ready to upload', 'success');
      }
      
      function floydSteinbergDithering(imageData) {
        const data = imageData.data;
        const width = imageData.width;
        const height = imageData.height;
        
        for (let i = 0; i < data.length; i += 4) {
          const gray = 0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2];
          data[i] = data[i + 1] = data[i + 2] = gray;
        }
        
        for (let y = 0; y < height; y++) {
          for (let x = 0; x < width; x++) {
            const idx = (y * width + x) * 4;
            const oldPixel = data[idx];
            const newPixel = oldPixel < 128 ? 0 : 255;
            const error = oldPixel - newPixel;
            
            data[idx] = data[idx + 1] = data[idx + 2] = newPixel;
            
            if (x + 1 < width) {
              data[idx + 4] += error * 7 / 16;
            }
            if (y + 1 < height) {
              if (x > 0) {
                data[idx + (width * 4) - 4] += error * 3 / 16;
              }
              data[idx + (width * 4)] += error * 5 / 16;
              if (x + 1 < width) {
                data[idx + (width * 4) + 4] += error * 1 / 16;
              }
            }
          }
        }
        
        return imageData;
      }
      
      function packImageData(imageData) {
        const data = imageData.data;
        const width = imageData.width;
        const height = imageData.height;
        
        const bytesPerRow = Math.ceil(width / 8);
        const totalBytes = bytesPerRow * height;
        const packed = new Uint8Array(totalBytes);
        
        for (let y = 0; y < height; y++) {
          for (let x = 0; x < width; x++) {
            const idx = (y * width + x) * 4;
            const pixel = data[idx];
            
            if (pixel < 128) {
              const byteIdx = y * bytesPerRow + Math.floor(x / 8);
              const bitIdx = 7 - (x % 8);
              packed[byteIdx] |= (1 << bitIdx);
            }
          }
        }
        
        return packed;
      }
      
      uploadButton.addEventListener('click', async () => {
        if (!processedImageData) return;
        
        uploadButton.disabled = true;
        uploadButton.textContent = 'Uploading...';
        showStatus('Sending to display...', 'processing');
        
        try {
          const response = await fetch('/upload', {
            method: 'POST',
            headers: {
              'Content-Type': 'application/octet-stream'
            },
            body: processedImageData
          });
          
          if (response.ok) {
            showStatus('Image displayed successfully!', 'success');
            uploadButton.textContent = 'Uploaded!';
            setTimeout(() => {
              uploadButton.textContent = 'Upload Another';
              uploadButton.disabled = false;
            }, 2000);
          } else {
            showStatus('Upload failed. Please try again.', 'error');
            uploadButton.disabled = false;
            uploadButton.textContent = 'Upload to Display';
          }
        } catch (error) {
          showStatus('Connection error: ' + error.message, 'error');
          uploadButton.disabled = false;
          uploadButton.textContent = 'Upload to Display';
        }
      });
      
      function showStatus(message, type) {
        status.textContent = message;
        status.className = 'status ' + type;
      }
    </script>
  </body>
</html>
)rawliteral";

void displayUploadedImage(uint8_t* data, size_t len) {
  Serial.println("Displaying image on e-paper...");
  
  if (len != IMAGE_BUFFER_SIZE) {
    Serial.printf("Error: Expected %d bytes, got %d bytes\n", IMAGE_BUFFER_SIZE, len);
    return;
  }
  
  display.setFullWindow();
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
      for (int x = 0; x < DISPLAY_WIDTH; x++) {
        int byteIdx = y * ((DISPLAY_WIDTH + 7) / 8) + (x / 8);
        int bitIdx = 7 - (x % 8);
        
        if (data[byteIdx] & (1 << bitIdx)) {
          display.drawPixel(x, y, GxEPD_BLACK);
        }
      }
      
      if (y % 10 == 0) {
        yield();
      }
    }
  } while (display.nextPage());
  
  Serial.println("Image displayed successfully!");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Project E-paper Gift ===");
  
  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  
  display.init(115200);
  display.setRotation(1);

  imageBuffer = (uint8_t*)malloc(IMAGE_BUFFER_SIZE);
  if (imageBuffer == nullptr) {
    Serial.println("Failed to allocate image buffer!");
  }

  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.print("Connect to WiFi: ");
  Serial.println(ssid);
  Serial.println("Then visit: http://192.168.4.1");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.on("/upload", HTTP_POST, 
    [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", "OK");
    }, 
    NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
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
      }
    }
  );

  server.begin();

  Serial.println("Updating display...");
  display.setFullWindow();
  display.firstPage();
  do {
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

void loop() {
  if (imageReadyToDisplay) {
    imageReadyToDisplay = false;
    displayUploadedImage(imageBuffer, totalReceived);
  }
  delay(100);
}
