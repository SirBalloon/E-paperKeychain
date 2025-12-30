# E-paperKeychain Installation Guide with Explaination

## Step 1 - 
Purchase the parts, for the keychain I chose to buy these parts:

- WeAct 2.9 inch e-paper display
    https://www.aliexpress.com/item/1005004644515880.html?spm=a2g0o.order_list.order_list_main.5.1a821802x8uvDu
- Esp32-s3-Zero
    https://core-electronics.com.au/esp32-s3-mini-development-board-retired.html
- Right Angled Pin headers


## Step 2 - 
Follow the wiring table below. This will allow the e-paper display and esp32 to talk to each other. 

| E-Paper Display | ESP32-S3-Zero |
|------------------|---------------|
| VCC              | 3V3 (pin 16)  |
| GND              | GND (pin 14)  |
| DIN              | IO8 (pin 8)   |
| CLK              | IO7 (pin 7)   |
| CS               | IO10 (pin 10) |
| DC               | IO11 (pin 11) |
| RST              | IO12 (pin 12) |
| BUSY             | IO13 (pin 13) |

## Step 3 - 
Once everything is wired, you can now install arduino IDE or any other alternative. After arduino IDE is installed, open the E-paperServer.ino file. You can tweak both the ssid and password for better and tailored security. Also feel free to edit anything allowing for further improvements. Once done editing, you can click the upload button with the esp32 connected to your PC via usb-c. This will upload the code onto the esp32. 

## Step 4 - 
Now everything should be ready, test the software and hardware. Ensure everything works smoothly! 

Things to be aware off:
- The keychain is not meant to have power for long. 
- When connecting to the wifi access point, you may need to turn off both the wifi and 5g data for it to work. 