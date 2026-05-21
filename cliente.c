#define _POSIX_C_SOURCE 200809L

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

volatile int parar = 0;

void handler_sinal(int s)
{
    parar = 1;
}

int gerar_telemetria(const char *id, char *buf, int tam)
{
    float temp = 20.0f + (rand() % 150) / 10.0f;
    float luz  = (rand() % 1001) / 10.0f;
    float umid = 50.0f + (rand() % 400) / 10.0f;
    
    return snprintf(buf, tam, "%s,%.1f,%.1f,%.1f\n", id, temp, luz, umid);
}

void dormir_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
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

    if (porta <= 0 || porta > 65535) return 1;
    if (intervalo_ms < 10) intervalo_ms = 10;

    signal(SIGINT, handler_sinal);
    signal(SIGTERM, handler_sinal);
    signal(SIGPIPE, SIG_IGN);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 1;

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    
    server.sin_port = htons(porta);
    
    if (inet_pton(AF_INET, host, &server.sin_addr) <= 0) return 1;

    printf("Conectando em %s:%d...\n", host, porta);
    
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("Falha ao conectar.\n");
        return 1;
    }
    
    printf("Conectado! Pressione Ctrl+C para parar.\n\n");

    srand(time(NULL) ^ getpid());

    while (!parar) {
        int len = gerar_telemetria(id, msg, sizeof(msg));

        int enviado = send(sock, msg, len, 0);
        
        if (enviado < 0) break;

        contador++;

        if (len > 0 && msg[len - 1] == '\n') {
            msg[len - 1] = '\0';
        }
        
        printf("#%04d enviado (%d bytes): %s\n", contador, enviado, msg);
        
        dormir_ms(intervalo_ms);
    }

    printf("\nEncerrando. Total: %d mensagens.\n", contador);
    close(sock);
    return 0;
}