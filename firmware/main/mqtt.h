#pragma once

void mqtt_init(const char *device_id, const char *mac, const char *ip);

void mqtt_publish(const char *topic, const char *data, int len, int qos,
                  int retain);
