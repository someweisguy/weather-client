#include "http_handlers.h"

#include "client_handlers.h"

esp_err_t http_data_handler(httpd_req_t *r)
{
    // send json back to the client
    httpd_resp_set_type(r, HTTPD_TYPE_JSON);

    bool clear_data = (bool) r->user_ctx;
    char *response = data_handler(clear_data);
    
    // send the response to the client
    httpd_resp_set_status(r, HTTPD_200);
    httpd_resp_sendstr(r, response);

    // free resources
    free(response);

    return ESP_OK;
}

esp_err_t http_restart_handler(httpd_req_t *r)
{
    httpd_resp_set_type(r, HTTPD_TYPE_TEXT);
    httpd_resp_set_status(r, HTTPD_200);
    httpd_resp_sendstr(r, "OK");
    restart_handler();
    return ESP_OK;
}