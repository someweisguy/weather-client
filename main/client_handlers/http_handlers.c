#include "http_handlers.h"

#include "client_handlers.h"

#define HTTPD_507 "507 Insufficient Storage"

esp_err_t http_about_handler(httpd_req_t *r)
{
    // send json back to the client
    httpd_resp_set_type(r, HTTPD_TYPE_JSON);

    // generate the response to the client
    char *response = about_handler();

    // send the response to the client
    httpd_resp_set_status(r, HTTPD_200);
    httpd_resp_sendstr(r, response);

    // free the response
    free(response);

    return ESP_OK;
}

esp_err_t http_config_handler(httpd_req_t *r)
{
    // set the response type
    httpd_resp_set_type(r, HTTPD_TYPE_TEXT);

    // read the body of the request into memory
    char *request = malloc(r->content_len + 1); // content + null terminator
    if (request == NULL)
    {
        httpd_resp_set_status(r, HTTPD_507);
        httpd_resp_sendstr(r, "NO MEMORY");
        return ESP_ERR_NO_MEM;
    }
    httpd_req_recv(r, request, r->content_len + 1);

    esp_err_t err = config_handler(request);
    free(request);

    // send a response to the client
    if (err)
    {
        if (err == ESP_ERR_INVALID_ARG)
            httpd_resp_set_status(r, HTTPD_400);
        else
            httpd_resp_set_status(r, HTTPD_500);
        httpd_resp_sendstr(r, "FAIL");
    }
    else
    {
        httpd_resp_set_status(r, HTTPD_200);
        httpd_resp_sendstr(r, "OK");
    }

    return ESP_OK;
}

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