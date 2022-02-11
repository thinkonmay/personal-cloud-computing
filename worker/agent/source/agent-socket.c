/**
 * @file agent-socket.c
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2021-12-01
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include <agent-socket.h>
#include <agent-type.h>
#include <agent-session-initializer.h>
#include <agent-device.h>
#include <agent-server.h>
#include <agent-port-forward.h>

#include <logging.h>
#include <message-form.h>
#include <global-var.h>

#include <glib-object.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>


#define WRITE_TOKEN_TO_FILE FALSE


/**
 * @brief 
 * contain information about websocket socket with host
 */
struct _Socket
{
    SoupSession* session;
};








gboolean
send_message_to_cluster(AgentServer* object,
                        gchar* endpoint,
                        gchar* message)
{
    Socket* socket = agent_get_socket(object);
    GString* messsage_url = g_string_new(CLUSTER_URL);
    g_string_append(messsage_url,endpoint);
    gchar* url = g_string_free(messsage_url,FALSE);

    SoupMessage* soupMessage = soup_message_new(SOUP_METHOD_POST,url);
    soup_message_headers_append(soupMessage->request_headers,"Authorization",DEVICE_TOKEN);

    if(message)
    {
        soup_message_set_request(soupMessage,"application/json",SOUP_MEMORY_COPY,
            message,strlen(message));
    }
    else
    {
        soup_message_set_request(soupMessage,"application/json",SOUP_MEMORY_COPY,"",0);
    }

    soup_session_send_message(socket->session,soupMessage);
}


void
register_with_managed_cluster(AgentServer* agent, 
                              PortForward* port)
{
    Socket* socket = agent_get_socket(agent);
    gchar* package = get_registration_message(TRUE,
        portforward_get_agent_instance_port(port));
    GString* register_url = g_string_new(CLUSTER_URL);
    g_string_append(register_url,"/worker/register");
    gchar* final_url = g_string_free(register_url,FALSE);

    SoupMessage* soupMessage = soup_message_new(SOUP_METHOD_POST,final_url);
    soup_message_headers_append(soupMessage->request_headers,"Authorization",token);
    soup_message_set_request(soupMessage,"application/json", SOUP_MEMORY_COPY, package,strlen(package));

    worker_log_output("Registering with host");
    soup_session_send_message(socket->session,soupMessage);

    if(soupMessage->status_code != SOUP_STATUS_OK)
    {
        g_printerr("Fail to register device\n");
        agent_finalize(agent);
        return;
    }
    else
    {
        GError* error = NULL;
        JsonParser* parser = json_parser_new();
        JsonObject* result_json = get_json_object_from_string(soupMessage->response_body->data,&error,parser);
        gchar* token_result = json_object_get_string_member(result_json,"token");
        memcpy(DEVICE_TOKEN,token_result,strlen(token_result));
        g_object_unref(parser); 

        g_print("Worker is registered...\n");
        return FALSE; 
    }
}


void
register_with_selfhosted_cluster(AgentServer* agent, 
                                 gchar* token)
{
    GError* error = NULL;
    Socket* socket = agent_get_socket(agent);

    gchar* package = get_registration_message(FALSE,NULL,NULL);
    GString* register_url = g_string_new(CLUSTER_URL);
    g_string_append(register_url,"/worker/register");
    gchar* final_url = g_string_free(register_url,FALSE);

    SoupMessage* soupMessage = soup_message_new(SOUP_METHOD_POST,final_url);
    soup_message_headers_append(soupMessage->request_headers,"Authorization", token);
    soup_message_set_request(soupMessage,"application/json", SOUP_MEMORY_COPY, package,strlen(package));

    worker_log_output("Registering with host");
    soup_session_send_message(socket->session,soupMessage);

    if(soupMessage->status_code != SOUP_STATUS_OK)
    {
        g_printerr("Fail to register device and get worker token\n");
        agent_finalize(agent);
    }
    else
    {
        JsonParser* parser = json_parser_new();
        JsonObject* result_json = get_json_object_from_string(soupMessage->response_body->data,&error,parser);
        gchar* token_result = json_object_get_string_member(result_json,"token");
        memcpy(DEVICE_TOKEN,token_result,strlen(token_result));
        g_object_unref(parser); 

        g_print("Worker is registered...\n");
    }
}







Socket*
initialize_socket()
{
    Socket* socket = malloc(sizeof(Socket));
    memset(socket,0,sizeof(Socket)); 
    const gchar* http_aliases[] = { "http", NULL };

    socket->session = soup_session_new_with_options(
            SOUP_SESSION_SSL_STRICT, FALSE,
            SOUP_SESSION_SSL_USE_SYSTEM_CA_FILE, TRUE,
            SOUP_SESSION_HTTPS_ALIASES, http_aliases, NULL);

    return socket;
}