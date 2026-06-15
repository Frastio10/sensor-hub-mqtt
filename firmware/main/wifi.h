#pragma once

typedef void (*wifi_connected_cb)(const char *ip);

void wifi_init(wifi_connected_cb on_connected);
