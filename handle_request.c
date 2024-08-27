#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

#define MAX_QUERY_LENGTH 1024

// Function prototypes
void parse_post_data(const char *data, char *module, char *action);
void parse_get_query(const char *query, char *module, char *action);
void get_uci_params(const char *config_path, const char **params, size_t num_params, struct json_object *json_response);
void execute_uci_command(const char *action, struct json_object *response_json);
void handle_post_request();
void handle_get_request();

int main() {
    // Check request method
    const char *method = getenv("REQUEST_METHOD");

    // Print HTTP header
    printf("Content-Type: application/json\n\n");

    if (method != NULL && strcmp(method, "POST") == 0) {
        handle_post_request();
    } else if (method != NULL && strcmp(method, "GET") == 0) {
        handle_get_request();
    } else {
        // Method not supported
        printf("{\"error\":1,\"message\":\"Method not supported\"}\n");
    }

    return 0;
}

// Function to handle POST request
void handle_post_request() {
    char query[MAX_QUERY_LENGTH];
    size_t content_length;
    char *content_length_str = getenv("CONTENT_LENGTH");

    if (content_length_str == NULL) {
        printf("{\"error\":1,\"message\":\"Missing CONTENT_LENGTH\"}\n");
        return;
    }

    content_length = (size_t)atoi(content_length_str);
    if (content_length >= MAX_QUERY_LENGTH) {
        printf("{\"error\":1,\"message\":\"Request too large\"}\n");
        return;
    }

    // Read POST data
    fread(query, 1, content_length, stdin);
    query[content_length] = '\0'; // Null-terminate

    char module[50];
    char action[50];

    // Parse the POST data
    parse_post_data(query, module, action);

    // Create a JSON object for the response
    struct json_object *response_json = json_object_new_object();

    // Execute UCI command and get the output
    execute_uci_command(action, response_json);

    // Add the module and error fields
    json_object_object_add(response_json, "module", json_object_new_string(module));
    json_object_object_add(response_json, "error", json_object_new_int(0));

    // Print the JSON response
    printf("%s\n", json_object_to_json_string(response_json));

    // Free the JSON object
    json_object_put(response_json);
}

// Function to handle GET request
void handle_get_request() {
    char *query = getenv("QUERY_STRING");

    if (query == NULL) {
        printf("{\"error\":1,\"message\":\"Missing QUERY_STRING\"}\n");
        return;
    }

    char module[50];
    char action[50];

    // Parse the GET query string
    parse_get_query(query, module, action);

    // Create a JSON object for the response
    struct json_object *response_json = json_object_new_object();

    // Execute UCI command and get the output
    execute_uci_command(action, response_json);

    // Add the module and error fields
    json_object_object_add(response_json, "module", json_object_new_string(module));
    json_object_object_add(response_json, "error", json_object_new_int(0));

    // Print the JSON response
    printf("%s\n", json_object_to_json_string(response_json));

    // Free the JSON object
    json_object_put(response_json);
}

// Function to parse POST data (JSON format)
void parse_post_data(const char *data, char *module, char *action) {
    // Parse JSON input using json-c
    struct json_object *parsed_json;
    struct json_object *json_module;
    struct json_object *json_action;

    parsed_json = json_tokener_parse(data);

    if (json_object_object_get_ex(parsed_json, "module", &json_module)) {
        strncpy(module, json_object_get_string(json_module), sizeof(module) - 1);
    }

    if (json_object_object_get_ex(parsed_json, "param", &json_action)) {
        struct json_object *json_action_value;
        if (json_object_object_get_ex(json_action, "action", &json_action_value)) {
            strncpy(action, json_object_get_string(json_action_value), sizeof(action) - 1);
        }
    }

    json_object_put(parsed_json); // Free the memory of JSON object
}

// Function to parse GET query string
void parse_get_query(const char *query, char *module, char *action) {
    // Decode URL-encoded query string
    char decoded_query[MAX_QUERY_LENGTH];
    size_t len = strlen(query);
    size_t j = 0;

    for (size_t i = 0; i < len; i++) {
        if (query[i] == '%') {
            // Decode %XX format
            int value;
            sscanf(query + i + 1, "%2x", &value);
            decoded_query[j++] = (char)value;
            i += 2;
        } else if (query[i] == '+') {
            // Replace '+' with space
            decoded_query[j++] = ' ';
        } else {
            decoded_query[j++] = query[i];
        }
    }
    decoded_query[j] = '\0'; // Null-terminate

    // Parse key-value pairs
    char *pair = strtok(decoded_query, "&");
    while (pair != NULL) {
        char *key = strtok(pair, "=");
        char *value = strtok(NULL, "=");
        if (key && value) {
            if (strcmp(key, "module") == 0) {
                strncpy(module, value, sizeof(module) - 1);
            } else if (strcmp(key, "param.action") == 0) {
                strncpy(action, value, sizeof(action) - 1);
            }
        }
        pair = strtok(NULL, "&");
    }
}

// Function to get multiple UCI parameters and populate JSON object
void get_uci_params(const char *config_path, const char **params, size_t num_params, struct json_object *json_response) {
    FILE *fp;
    char path[1035];
    char command[256];
    struct json_object *params_json = json_object_new_object();

    for (size_t i = 0; i < num_params; i++) {
        snprintf(command, sizeof(command), "uci get %s.%s", config_path, params[i]);

        // Execute the command
        fp = popen(command, "r");
        if (fp == NULL) {
            continue;
        }

        // Read the command output
        while (fgets(path, sizeof(path) - 1, fp) != NULL) {
            // Remove newline character from the end of the string
            path[strcspn(path, "\n")] = '\0';
            json_object_object_add(params_json, params[i], json_object_new_string(path));
        }

        // Close the pipe
        pclose(fp);
    }

    // Add the params_json to the response JSON
    json_object_object_add(json_response, "lan_param", params_json);
}

// Function to execute UCI command and populate the response JSON
void execute_uci_command(const char *action, struct json_object *response_json) {
    // Define parameters to fetch
    const char *params[] = {
        "ipaddr",
        "netmask"
    };
    size_t num_params = sizeof(params) / sizeof(params[0]);

    // Get UCI parameters and add them to JSON response
    get_uci_params("network.lan", params, num_params, response_json);
}

