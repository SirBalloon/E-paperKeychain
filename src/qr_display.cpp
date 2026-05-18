// Function that displays a QR-code for the IP Address
#include "qr_display.h"
#include "config.h"
#include <qrcode.h>
#include <Fonts/FreeMonoBold12pt7b.h>

void Qr_display(GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> &display)
{
    uint8_t qrBytes[qrcode_getBufferSize(3)];

    QRCode qrcode;
    qrcode_initText(&qrcode, qrBytes, 3, ECC_MEDIUM, "http://192.168.4.1");

    display.setFullWindow();
    display.firstPage();

    do
    {
        display.fillScreen(GxEPD_WHITE);
        display.setFont(&FreeMonoBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(100, 30);
        display.println("Connect to:");
        display.setCursor(100, 60);
        display.println(ssid);

        for (int y = 0; y < qrcode.size; y++)
        {
            for (int x = 0; x < qrcode.size; x++)
            {
                if (qrcode_getModule(&qrcode, x, y))
                {
                    display.fillRect(x * 3, y * 3, 3, 3, GxEPD_BLACK);
                }
            }
        }
    } while (display.nextPage());
}