#define _POSIX_C_SOURCE 200809L

/* Cliente TCP que envia telemetria simulada periodicamente.
   Uso: ./cliente [host] [porta] [sensor_id] [intervalo_ms] */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define HOST_PADRAO          "127.0.0.1"
#define PORTA_PADRAO         5000
#define SENSOR_ID_PADRAO     "sensor-01"
#define INTERVALO_PADRAO_MS  1000

static volatile sig_atomic_t parar = 0;

static void handler_sinal(int s)
{
    (void)s;
    parar = 1;
}

static int gerar_telemetria(const char *id, char *buf, size_t tam)
{
    float temp = 20.0f + (float)(rand() % 150) / 10.0f;
    float luz  = (float)(rand() % 1001) / 10.0f;
    float umid = 50.0f + (float)(rand() % 400) / 10.0f;
    return snprintf(buf, tam, "%s,%.1f,%.1f,%.1f\n", id, temp, luz, umid);
}

static void dormir_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int main(int argc, char *argv[])
{
    const char *host = (argc > 1) ? argv[1] : HOST_PADRAO;
    int porta = (argc > 2) ? atoi(argv[2]) : PORTA_PADRAO;
    const char *id = (argc > 3) ? argv[3] : SENSOR_ID_PADRAO;
    int intervalo_ms = (argc > 4) ? atoi(argv[4]) : INTERVALO_PADRAO_MS;

    int sock;
    struct sockaddr_in server;
    char msg[256];
    int contador = 0;

    if (porta <= 0 || porta > 65535) {
        fprintf(stderr, "Porta invalida: %d\n", porta);
        return 1;
    }
    if (intervalo_ms < 10) {
        intervalo_ms = 10;
    }

    signal(SIGINT, handler_sinal);
    signal(SIGTERM, handler_sinal);
    signal(SIGPIPE, SIG_IGN);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons((uint16_t)porta);
    if (inet_pton(AF_INET, host, &server.sin_addr) <= 0) {
        fprintf(stderr, "Host invalido: %s\n", host);
        close(sock);
        return 1;
    }

    printf("================================================================\n");
    printf("  Cliente de telemetria TCP\n");
    printf("  Destino: %s:%d  |  ID: %s  |  intervalo: %d ms\n",
           host, porta, id, intervalo_ms);
    printf("================================================================\n");
    printf("Conectando...\n");
    fflush(stdout);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect");
        fprintf(stderr,
                "Dica: o servidor esta rodando em %s:%d?\n", host, porta);
        close(sock);
        return 1;
    }
    printf("Conectado! Pressione Ctrl+C para parar.\n\n");
    fflush(stdout);

    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    while (!parar) {
        int len = gerar_telemetria(id, msg, sizeof(msg));

        ssize_t enviado = send(sock, msg, (size_t)len, 0);
        if (enviado < 0) {
            perror("send");
            break;
        }

        contador++;

        if (len > 0 && msg[len - 1] == '\n') {
            msg[len - 1] = '\0';
        }
        printf("#%04d enviado (%zd bytes): %s\n", contador, enviado, msg);
        fflush(stdout);

        dormir_ms(intervalo_ms);
    }

    printf("\nEncerrando cliente. Total enviado: %d mensagens.\n", contador);
    close(sock);
    return 0;
}