#include "http.h"

#include "esp_http_server.h"
#include "cJSON.h"

static httpd_handle_t server = NULL;

esp_err_t http_start()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    esp_err_t err = httpd_start(&server, &config);
    if (err)
        return err;

    // TODO: register uri handlers

    return ESP_OK;
}

esp_err_t http_stop()
{
    esp_err_t err = httpd_stop(&server);
    if (err)
        return err;
    return ESP_OK;
}

esp_err_t http_register_handler(const char *uri, const httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r), void *user_ctx)
{
    const httpd_uri_t http_handler = {
        .uri = uri,
        .method = method,
        .handler = handler,
        .user_ctx = user_ctx};
    return httpd_register_uri_handler(server, &http_handler);
}