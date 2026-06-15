#include "json.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

void create_status_json(char *status_str, int status) {
  cJSON *status_root = cJSON_CreateObject();
  cJSON_AddNumberToObject(status_root, "status", status);
  char *json = cJSON_PrintUnformatted(status_root);
  strcpy(status_str, json);

  free(json);
  cJSON_Delete(status_root);
}

void create_value_json(char *value_str, int value) {
  cJSON *value_root = cJSON_CreateObject();
  cJSON_AddNumberToObject(value_root, "value", value);
  char *json = cJSON_PrintUnformatted(value_root);
  strcpy(value_str, json);

  free(json);
  cJSON_Delete(value_root);
}
