#ifndef WIFI_STREAM_H
#define WIFI_STREAM_H

#include <WiFi.h>

void wifi_stream_init(const char *ssid, const char *pass);
bool wifi_stream_connected();
const char *wifi_stream_ip();
void wifi_stream_stop();
void wifi_stream_show_frame();

#endif
