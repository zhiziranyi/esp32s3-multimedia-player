#include "net_services.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

static bool g_time_ok = false;
static char g_weather[64] = "--";
static int g_weather_temp = 0;
static uint32_t g_weather_last = 0;

// Delay first fetch to avoid blocking UI during startup
static bool g_first_fetch_done = false;

// ---------- NTP ----------

void net_services_init()
{
    configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org");
}

bool net_is_connected() { return (WiFi.status() == WL_CONNECTED); }
bool net_time_synced() { return g_time_ok; }

void net_time_get(int *h, int *m, int *s, int *y, int *mo, int *d, int *wday)
{
    struct tm ti;
    if (!getLocalTime(&ti)) return;
    *h = ti.tm_hour; *m = ti.tm_min; *s = ti.tm_sec;
    *y = ti.tm_year + 1900; *mo = ti.tm_mon + 1; *d = ti.tm_mday;
    *wday = ti.tm_wday;
    g_time_ok = true;
}

// ---------- Weather ----------

const char *net_weather_text() { return g_weather; }
int net_weather_temp() { return g_weather_temp; }

static void fetch_weather()
{
    if (!net_is_connected()) return;
    HTTPClient http;
    http.begin("http://wttr.in/?format=%t+%C");
    http.setTimeout(3000);
    int code = http.GET();
    if (code == 200) {
        String p = http.getString();
        p.trim();
        int sp = p.indexOf(' ');
        if (sp > 0) {
            String ts = p.substring(0, sp);
            ts.replace("+", ""); ts.replace("\302\260C", "");
            g_weather_temp = ts.toInt();
            snprintf(g_weather, sizeof(g_weather), "%dC %s", g_weather_temp, p.substring(sp + 1).c_str());
        } else {
            snprintf(g_weather, sizeof(g_weather), "%s", p.c_str());
        }
    }
    http.end();
    g_weather_last = millis();
}

void net_services_loop()
{
    uint32_t now = millis();
    // Delay first fetch 30s to avoid blocking UI at startup
    if (!g_first_fetch_done) {
        if (now > 30000) g_first_fetch_done = true;
        else return;
    }
    if (net_is_connected() && (g_weather_last == 0 || now - g_weather_last > 1800000))
        fetch_weather();
}
