#ifndef NET_SERVICES_H
#define NET_SERVICES_H

void net_services_init();
void net_services_loop();
bool net_is_connected();

bool net_time_synced();
void net_time_get(int *h, int *m, int *s, int *y, int *mo, int *d, int *wday);

const char *net_weather_text();
int net_weather_temp();

#endif
