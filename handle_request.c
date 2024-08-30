#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

#define MAX_QUERY_LENGTH 1024
#define MAX_BUFFER 512

// Function prototypes
void handle_post_request();
void handle_get_request() {
    printf("{\"message\":\"GET request handled\"}\n");
}

void execute_command(const char *command, char *output, size_t max_size);

char *json_get_string_value_by_field(struct json_object *json, const char *p_field) {
    struct json_object *string_json = NULL;

    json_object_object_get_ex(json, p_field, &string_json);
    if (string_json == NULL) {
        return NULL;
    }

    if (json_object_get_type(string_json) == json_type_string) {
        return (char *)json_object_get_string(string_json);
    }

    return NULL;
}

int json_get_int_value_by_field(struct json_object *json, const char *p_field) {
    struct json_object *int_json = NULL;

    json_object_object_get_ex(json, p_field, &int_json);
    if (int_json == NULL) {
        return -1;
    }

    if (json_object_get_type(int_json) == json_type_int) {
        return json_object_get_int(int_json);
    }

    return -1;
}

const char *json_get_string_value(struct json_object *json) {
    if (json_object_get_type(json) == json_type_string) {
        return json_object_get_string(json);
    }

    return NULL;
}

struct json_object *json_get_json_object_by_field(struct json_object *json, const char *p_field) {
    struct json_object *json_obj = NULL;

    json_object_object_get_ex(json, p_field, &json_obj);
    return json_obj;
}

int json_is_array(struct json_object *json) {
    return (json_object_get_type(json) == json_type_array) ? 0 : -1;
}

void execute_command(const char *command, char *output, size_t max_size) {
    FILE *fp;
    char buffer[MAX_BUFFER];
    size_t current_size = 0;
    fp = popen(command, "r");
    if (fp == NULL) {
        perror("popen failed");
        output[0] = '\0';
        return;
    }
    while (fgets(buffer, sizeof(buffer) - 1, fp) != NULL) {
        size_t len = strlen(buffer);
        if (current_size + len < max_size - 1) {
            strcpy(output + current_size, buffer);
            current_size += len;
        }
    }
    output[current_size] = '\0'; // Ensure null termination
    if (pclose(fp) == -1) {
        perror("pclose failed");
    }
}

void handle_post_request() {
    char query[MAX_QUERY_LENGTH];
    size_t content_length;
    char *content_length_str = getenv("CONTENT_LENGTH");

    if (content_length_str == NULL) {
        fprintf(stderr, "Error: CONTENT_LENGTH not set\n");
        printf("{\"error\":1,\"message\":\"Missing CONTENT_LENGTH\"}\n");
        return;
    }

    content_length = (size_t)atoi(content_length_str);
    if (content_length >= MAX_QUERY_LENGTH) {
        fprintf(stderr, "Error: Content length %zu exceeds maximum %d\n", content_length, MAX_QUERY_LENGTH);
        printf("{\"error\":1,\"message\":\"Request too large\"}\n");
        return;
    }

    // Read POST data
    size_t bytes_read = fread(query, 1, content_length, stdin);
    if (bytes_read != content_length) {
        fprintf(stderr, "Error: Expected %zu bytes, read %zu bytes\n", content_length, bytes_read);
        printf("{\"error\":1,\"message\":\"Failed to read POST data\"}\n");
        return;
    }
    query[content_length] = '\0'; // Null-terminate

    // Parse JSON
    struct json_object *myjson = json_tokener_parse(query);
    if (myjson == NULL) {
        fprintf(stderr, "Error: Invalid JSON\n");
        printf("{\"error\":1,\"message\":\"Invalid JSON\"}\n");
        return;
    }

    char *action = json_get_string_value_by_field(myjson, "ACT");
    if (action == NULL) {
        fprintf(stderr, "Error: Missing action field\n");
        printf("{\"error\":1,\"message\":\"Missing action\"}\n");
        json_object_put(myjson);
        return;
    }

    if (strcmp(action, "Login") == 0) {
        struct json_object *param = json_get_json_object_by_field(myjson, "param");
        if (param == NULL) {
            fprintf(stderr, "Error: Missing parameters\n");
            printf("{\"error\":1,\"message\":\"Missing parameters\"}\n");
            json_object_put(myjson);
            return;
        }

        char *admin = json_get_string_value_by_field(param, "admin");
        char *pwd = json_get_string_value_by_field(param, "pwd");

        if (admin && pwd && strcmp(admin, "admin") == 0 && strcmp(pwd, "123456") == 0) {
            printf("{\"error\":0}\n");
        } else {
            printf("{\"error\":1,\"message\":\"admin or pwd error\"}\n");
        }
        json_object_put(param);
    } else if (strcmp(action, "GetDHCP") == 0) {
        char ipaddr[MAX_BUFFER] = {0};
        char netmask[MAX_BUFFER] = {0};
        char start[MAX_BUFFER] = {0};
        char limit[MAX_BUFFER] = {0};
        char leasetime[MAX_BUFFER] = {0};

        execute_command("uci get network.lan.ipaddr", ipaddr, MAX_BUFFER);
        execute_command("uci get network.lan.netmask", netmask, MAX_BUFFER);
        execute_command("uci get dhcp.lan.start", start, MAX_BUFFER);
        execute_command("uci get dhcp.lan.limit", limit, MAX_BUFFER);
        execute_command("uci get dhcp.lan.leasetime", leasetime, MAX_BUFFER);

        struct json_object *response = json_object_new_object();
        json_object_object_add(response, "ipaddr", json_object_new_string(ipaddr));
        json_object_object_add(response, "netmask", json_object_new_string(netmask));
        json_object_object_add(response, "start", json_object_new_string(start));
        json_object_object_add(response, "limit", json_object_new_string(limit));
        json_object_object_add(response, "leasetime", json_object_new_string(leasetime));
        json_object_object_add(response, "error", json_object_new_int(0)); // Use json_object_new_int for integer values

        printf("%s\n", json_object_to_json_string(response));
        json_object_put(response);
    } else if (strcmp(action, "GetWiFi") == 0) {
        char ssid[MAX_BUFFER] = {0};
        char password[MAX_BUFFER] = {0};

        // 读取 WiFi 配置信息
        execute_command("uci get wireless.@wifi-iface[0].ssid", ssid, MAX_BUFFER);
        execute_command("uci get wireless.@wifi-iface[0].key", password, MAX_BUFFER);

        struct json_object *response = json_object_new_object();
        json_object_object_add(response, "ssid", json_object_new_string(ssid));
        json_object_object_add(response, "password", json_object_new_string(password));
        json_object_object_add(response, "error", json_object_new_int(0));

        printf("%s\n", json_object_to_json_string(response));
        json_object_put(response);
    } else if (strcmp(action, "GetVersion") == 0) {
        char openwrt[MAX_BUFFER] = {0};
        char kernel[MAX_BUFFER] = {0};
        char fw_version[MAX_BUFFER] = {0};
        char full_fw_version[MAX_BUFFER] = {0};
        char vendor_version[MAX_BUFFER] = {0};

        // 读取系统信息
        execute_command("cat /etc/openwrt_version", openwrt, MAX_BUFFER);
        execute_command("uname -r", kernel, MAX_BUFFER);

        get_value_from_config("/etc/system_version.info", "FW_VERSION", fw_version, MAX_BUFFER);
        get_value_from_config("/etc/system_version.info", "FULL_FW_VERSION", full_fw_version, MAX_BUFFER);
        get_value_from_config("/etc/system_version.info", "VENDOR_ASKEY_VERSION", vendor_version, MAX_BUFFER);

        struct json_object *response = json_object_new_object();

        json_object_object_add(response, "openwrt", json_object_new_string(openwrt));
        json_object_object_add(response, "kernel", json_object_new_string(kernel));
        json_object_object_add(response, "fw_version", json_object_new_string(fw_version)); // 修正为 json_object_new_string
        json_object_object_add(response, "full_fw_version", json_object_new_string(full_fw_version));
        json_object_object_add(response, "vendor_version", json_object_new_string(vendor_version));
        json_object_object_add(response, "error", json_object_new_int(0));

        printf("%s\n", json_object_to_json_string(response));
        json_object_put(response);
    } else if (strcmp(action, "SetDHCP") == 0) {
        char cmd[512] = {0};
        char *ipaddr = json_get_string_value_by_field(myjson, "ipaddr");
        char *netmask = json_get_string_value_by_field(myjson, "netmask");
        char *start = json_get_string_value_by_field(myjson, "start");
        char *limit = json_get_string_value_by_field(myjson, "limit");
        char *leasetime = json_get_string_value_by_field(myjson, "leasetime");

        if (ipaddr) {
            sprintf(cmd, "uci set network.lan.ipaddr=%s", ipaddr);
            system(cmd);
        }
        if (netmask) {
            sprintf(cmd, "uci set network.lan.netmask=%s", netmask);
            system(cmd);
        }
        if (start) {
            sprintf(cmd, "uci set dhcp.lan.start=%s", start);
            system(cmd);
        }
        if (limit) {
            sprintf(cmd, "uci set dhcp.lan.limit=%s", limit);
            system(cmd);
        }
        if (leasetime) {
            sprintf(cmd, "uci set dhcp.lan.leasetime=%s", leasetime);
            system(cmd);
        }

        system("uci commit");
        

        // Set error to 0 for successful execution
        printf("{\"error\":0}\n");
        system("/etc/init.d/network restart");

    }else if (strcmp(action, "SetWIFI") == 0) {
        char cmd[512] = {0};
        char *ssid = json_get_string_value_by_field(myjson, "ssid");
        char *key = json_get_string_value_by_field(myjson, "key");

        if (ssid) {
            sprintf(cmd, "uci set  wireless.@wifi-iface[0].ssid=%s", ssid);
            system(cmd);
        }
        if (key) {
            sprintf(cmd, "uci set wireless.@wifi-iface[0].key=%s", key);
            system(cmd);
        }
        
        system("uci commit");
        

        // Set error to 0 for successful execution
        printf("{\"error\":0}\n");
        system("wifi &");
    }

    json_object_put(myjson); // 移动到正确的位置
}

int get_value_from_config(const char *filename, const char *key, char *value, size_t value_size) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("error opening file");
        return -1;
    }
    char line[256];
    int found = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        char *delimiter_pos = strchr(line, '=');
        if (delimiter_pos != NULL) {
            *delimiter_pos = '\0';
            char *current_key = line;
            char *current_value = delimiter_pos + 1;

            char *newline_pos = strchr(current_value, '\n');
            if (newline_pos != NULL) {
                *newline_pos = '\0';
            }
            if (strcmp(current_key, key) == 0) {
                if (strlen(current_value) < value_size) {
                    strncpy(value, current_value, value_size - 1);
                    value[value_size - 1] = '\0';
                    found = 1;
                } else {
                    found = 0;
                }
                break;
            }
        }
    }
    fclose(file);

    return found ? 0 : -1;
}

int main() {
    const char *method = getenv("REQUEST_METHOD");

    printf("Content-Type: application/json\n\n");

    if (method != NULL && strcmp(method, "POST") == 0) {
        handle_post_request();
    } else if (method != NULL && strcmp(method, "GET") == 0) {
        handle_get_request(); // 实现 GET 请求处理逻辑
    } else {
        printf("{\"error\":1,\"message\":\"Method not supported\"}\n");
    }

    return 0;
}
