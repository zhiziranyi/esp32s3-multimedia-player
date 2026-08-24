#include "wifi_stream.h"
#include "tft_display.h"

static WiFiServer *g_server = NULL;
static WiFiClient g_client;
static uint8_t *g_frame_buf = NULL;
static bool g_stream_active = false;
static int g_frame_pos = 0;

#define STREAM_PORT 8888
#define FRAME_SIZE  (240 * 240 * 2)

static char g_ip[16] = "---";

void wifi_stream_init(const char *ssid, const char *pass)
{
    WiFi.begin(ssid, pass);
}

bool wifi_stream_connected()
{
    if (WiFi.status() == WL_CONNECTED) {
        snprintf(g_ip, sizeof(g_ip), "%s", WiFi.localIP().toString().c_str());
        return true;
    }
    return false;
}

const char *wifi_stream_ip() { return g_ip; }

void wifi_stream_stop()
{
    g_stream_active = false;
    if (g_client) g_client.stop();
    if (g_server) { g_server->end(); delete g_server; g_server = NULL; }
    if (g_frame_buf) { free(g_frame_buf); g_frame_buf = NULL; }
}

void wifi_stream_show_frame()
{
    if (!g_stream_active) {
        if (WiFi.status() == WL_CONNECTED) {
            if (!g_server) {
                g_server = new WiFiServer(STREAM_PORT);
                g_server->begin();
                g_server->setNoDelay(true);
            }
            if (!g_frame_buf) {
                g_frame_buf = (uint8_t *)malloc(FRAME_SIZE);
            }
            g_client = g_server->available();
            if (g_client) {
                g_client.setNoDelay(true);
                g_stream_active = true;
                g_frame_pos = 0;
            }
        }
        if (!g_stream_active) return;
    }

    if (!g_client.connected()) {
        g_stream_active = false;
        g_client.stop();
        g_frame_pos = 0;
        return;
    }

    /* Drain all available data */
    while (g_client.available() > 0) {
        int space = FRAME_SIZE - g_frame_pos;
        int n = g_client.read(g_frame_buf + g_frame_pos, space);
        if (n <= 0) break;
        g_frame_pos += n;

        if (g_frame_pos >= FRAME_SIZE) {
            TFT_StreamStart();
            TFT_StreamPush((uint16_t *)g_frame_buf, 240 * 240);
            TFT_StreamEnd();
            g_frame_pos = 0;
        }
    }
}
