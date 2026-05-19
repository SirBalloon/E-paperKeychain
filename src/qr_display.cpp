// Function that displays a QR-code for the IP Address
#include "qr_display.h"
#include <qrcode.h>
#include <Fonts/FreeMonoBold9pt7b.h>

#define QR_SCALE 4
#define QR_X_OFFSET 0
#define QR_Y_OFFSET ((128 - (29 * QR_SCALE)) / 2) // centers 116px QR vertically
#define TEXT_X 122

void Qr_display(GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> &display, const char *url, const char *dns, const char *rawIP, const char *instruction)
{
    uint8_t qrBytes[qrcode_getBufferSize(3)];

    QRCode qrcode;
    qrcode_initText(&qrcode, qrBytes, 3, ECC_MEDIUM, url);

    display.setFullWindow();
    display.firstPage();

    do
    {
        display.fillScreen(GxEPD_WHITE);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(TEXT_X, 45);
        display.println(instruction);
        display.setCursor(TEXT_X, 65);
        display.println(dns);
        display.setCursor(TEXT_X, 85);
        display.println(rawIP);

        for (int y = 0; y < qrcode.size; y++)
        {
            for (int x = 0; x < qrcode.size; x++)
            {
                if (qrcode_getModule(&qrcode, x, y))
                {
                    display.fillRect(QR_X_OFFSET + x * QR_SCALE, QR_Y_OFFSET + y * QR_SCALE, QR_SCALE, QR_SCALE, GxEPD_BLACK);
                }
            }
        }
    } while (display.nextPage());
}