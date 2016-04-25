#include <stdio.h>

#include <amqp.h>
#include <amqp_framing.h>
#include <amqp_tcp_socket.h>

#include "amqp_rc.h"
#include "camera-capture-ffmpeg.h"
#include "config_env.h"
#include "logger.h"
#include "net_pack.h"

#define LOG_TAG "amqp"
#define BUFFER_SIZE 256

static CameraDevice* dev = NULL;

static pthread_mutex_t static_device_mtx;

static pthread_t amqp_listen;

static void unpack_cam_data(char* cam_filename, void* envelope_bytes, size_t envelope_len)
{
    if (envelope_len > BUFFER_SIZE)
    {
        C("Got a filename > 256 bytes (%d)", envelope_len);
        return;
    }
    unsigned char buf[BUFFER_SIZE] = {0};
    memcpy(buf, envelope_bytes, envelope_len);
    unpack(buf, "s", cam_filename);
}

static int check_amqp_error(amqp_rpc_reply_t x, char const* context)
{
    switch (x.reply_type)
    {
    case AMQP_RESPONSE_NORMAL:
        return 0;
    default:
        C("Error: %d %s", x.reply_type, context);
        return 1;
    }
}

static int amqp_connect(amqp_connection_state_t* conn, const char* const username,
                        const char* const password, const char* const vhost,
                        const char* const hostname, const int port)
{
    int status;
    amqp_socket_t* socket;
    amqp_rpc_reply_t reply_status;

    *conn = amqp_new_connection();
    socket = amqp_tcp_socket_new(*conn);
    if (!socket)
    {
        E("Unable to create socket");
        return 1;
    }
    status = amqp_socket_open(socket, hostname, port);
    if (status)
    {
        E("Unable to open socket");
        return 1;
    }
    reply_status =
        amqp_login(*conn, vhost, 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, username, password);
    if (check_amqp_error(reply_status, "login"))
        return 1;
    amqp_channel_open(*conn, 1);
    reply_status = amqp_get_rpc_reply(*conn);
    if (check_amqp_error(reply_status, "channel open"))
        return 1;
    return 0;
}

static int amqp_run_consume(amqp_connection_state_t* conn, const char* const queue)
{
    amqp_basic_consume(*conn, 1, amqp_cstring_bytes(queue), amqp_empty_bytes, 0, 0, 0,
                       amqp_empty_table);
    if (check_amqp_error(amqp_get_rpc_reply(*conn), "consume"))
        return 1;
    I("Listening to queue \"%s\"", queue);
    return 0;
}

static void* dispatch_amqp(void* ignored)
{
    amqp_connection_state_t conn;
    int status;
    const char* const username = configvar_string("AIC_PLAYER_AMQP_USERNAME");
    const char* const password = configvar_string("AIC_PLAYER_AMQP_PASSWORD");
    const char* const host = configvar_string("AIC_PLAYER_AMQP_HOST");
    const char* const vmid = configvar_string("AIC_PLAYER_VM_ID");
    char queue[256];
    snprintf(queue, sizeof(queue), "android-events.%s.camera", vmid);

    status = amqp_connect(&conn, username, password, "/", host, 5672);
    if (status)
        ERR("Failed to connect to AMQP");
    status = amqp_run_consume(&conn, queue);
    if (status)
        ERR("Failed to consume AMQP messages");

    char filename[BUFFER_SIZE] = {0};
    while (1)
    {
        amqp_envelope_t env;
        amqp_rpc_reply_t reply = amqp_consume_message(conn, &env, NULL, 0);
        if (reply.reply_type == AMQP_RESPONSE_NORMAL)
        {
            unpack_cam_data(filename, env.message.body.bytes, env.message.body.len);
            I("Received file id %s", filename);
            pthread_mutex_lock(&static_device_mtx);
            switch_video_files(dev, (const char* const) filename);
            pthread_mutex_unlock(&static_device_mtx);
        }
        amqp_destroy_envelope(&env);
    }
    return 0;
}

pthread_t* start_amqp_thread()
{
    pthread_mutex_init(&static_device_mtx, NULL);
    pthread_create(&amqp_listen, NULL, &dispatch_amqp, NULL);
    return &amqp_listen;
}

/**
 * Update the camera device pointer
 */
void switch_device(CameraDevice* new_dev)
{
    pthread_mutex_lock(&static_device_mtx);
    dev = new_dev;
    pthread_mutex_unlock(&static_device_mtx);
}
