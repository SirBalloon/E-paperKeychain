// Header file for QrCode

#pragma once
#include <GxEPD2_3C.h>

void Qr_display(GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> &display, const char *url, const char *dns, const char *rawIP, const char *instruction);
