#include <stdio.h>

#include <amqp.h>
#include <amqp_framing.h>
#include <amqp_tcp_socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

#include "remote_command.h"
#include "camera-capture-ffmpeg.h"
#include "config_env.h"
#include "logger.h"
#include "net_pack.h"

#define LOG_TAG "remote_control"
#define BUFFER_SIZE 256

static CameraDevice* dev = NULL;

static pthread_mutex_t static_device_mtx;

static pthread_t amqp_listen;
static pthread_t socket_listen;

int connect_socket(const char* ip, short port)
{
    struct addrinfo* result;
    int sock, error, yes = 1;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return -1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (getaddrinfo(ip, NULL, NULL, &result))
    {
        perror("getaddrinfo()");
        return -1;
    }

    ((struct sockaddr_in*) result->ai_addr)->sin_port = htons(port);
    ((struct sockaddr_in*) result->ai_addr)->sin_family = AF_INET;

    error = connect(sock, result->ai_addr, sizeof(struct sockaddr));
    freeaddrinfo(result);
    return (error == -1 ? error : sock);
}

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

/** Remote control from the VM
 *
 * We connect to the port 32500 on the virtual machine,
 * and we then receive video file switch instructions from the VM
 */

static void* socket_dispatch_thread(void* args)
{
    const char* const vmip = configvar_string("AIC_PLAYER_VM_HOST");
    while (1)
    {
        int sock = connect_socket(vmip, 32600);
        if (sock <= 0)
        {
            sleep(1);
            continue;
        }
        while (1)
        {
            struct timeval timeout = {5, 0};
            char buf[BUFFER_SIZE];

            fd_set fd_read;
            FD_ZERO(&fd_read);
            FD_SET(sock, &fd_read);

            if (select(sock + 1, &fd_read, 0, 0, &timeout) == -1)
                break;
            if (FD_ISSET(sock, &fd_read))
            {
                ssize_t len = recv(sock, buf, sizeof(buf), 0);
                if (len <= 0)
                {
                    W("Error received from the VM, reconnecting");
                    break;
                }
                else
                {
                    char filename[BUFFER_SIZE] = {0};
                    unpack_cam_data(filename, buf, len);
                    I("Received file name %s", filename);
                    pthread_mutex_lock(&static_device_mtx);
                    switch_video_files(dev, (const char* const) filename);
                    pthread_mutex_unlock(&static_device_mtx);
                }
            }
        }
        close(sock);
    }
    return NULL;
}

pthread_t* start_socket_thread()
{
    pthread_create(&socket_listen, NULL, &socket_dispatch_thread, NULL);
    return &socket_listen;
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
