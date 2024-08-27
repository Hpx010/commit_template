#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

// Define constants
#define MAX_QUERY_LENGTH 2048

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

    struct json_object *myjson = json_tokener_parse(query);
    if (json_object_get_type(myjson) != json_type_object) {
        printf("{\"error\":1,\"message\":\"Invalid JSON\"}\n");
        json_object_put(myjson);
        return;
    }

    struct json_object *param;
    const char *action = NULL;

    // Get the action from the JSON object
    json_object_object_get_ex(myjson, "ACT", &action);
    
    if (strcmp(action, "Login") == 0) {
        json_object_object_get_ex(myjson, "param", &param);
        const char *admin = NULL;
        const char *pwd = NULL;
        json_object_object_get_ex(param, "admin", &admin);
        json_object_object_get_ex(param, "pwd", &pwd);
        if (admin != NULL && pwd != NULL && strcmp(admin, "admin") == 0 && strcmp(pwd, "12345678") == 0) {
            printf("{\"error\":0}\n");
        } else {
            printf("{\"error\":1,\"message\":\"admin or pwd error!\"}\n");
        }
    } else if (strcmp(action, "dhcp") == 0) {
        // Handle dhcp action
    }

    // Free the JSON object
    json_object_put(myjson);
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

void parse_get_query(const char *query, char *module, char *action) {
    // Simple query string parser, assuming format is module=action
    char *module_start = strstr(query, "module=");
    if (module_start) {
        module_start += 7; // Skip "module="
        char *module_end = strchr(module_start, '&');
        if (module_end) {
            strncpy(module, module_start, module_end - module_start);
            module[module_end - module_start] = '\0';
        } else {
            strcpy(module, module_start);
        }
    }

    char *action_start = strstr(query, "action=");
    if (action_start) {
        action_start += 7; // Skip "action="
        char *action_end = strchr(action_start, '&');
        if (action_end) {
            strncpy(action, action_start, action_end - action_start);
            action[action_end - action_start] = '\0';
        } else {
            strcpy(action, action_start);
        }
    }
}

void execute_uci_command(const char *action, struct json_object *response_json) {
    // Placeholder for executing UCI commands and populating the response_json
    json_object_object_add(response_json, "result", json_object_new_string("Command executed"));
}

int main() {
    // Check request method
    const char *method = getenv("REQUEST_METHOD");

    // Print HTTP header
    printf("Content-Type: application/json\n\n");

    if (method != NULL && strcmp(method, "POST") == 0) {
        handle_post_request();
    } else if (method != NULL && strcmp(method, "GET") == 0) {
        //handle_get_request(); 
        printf("{\"error\":1,\"message\":\"GET Method not supported\"}\n");
    } else {
        // Method not supported
        printf("{\"error\":1,\"message\":\"Method not supported\"}\n");
    }

    return 0;
}
