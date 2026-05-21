#define _POSIX_C_SOURCE 200809L

/* Servidor TCP concorrente (fork por cliente). Uso: ./servidor [porta] */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORTA_PADRAO 5000
#define BUFFER_SIZE  1024
#define ARQUIVO_LOG  "sessoes.log"

static volatile sig_atomic_t parar = 0;

static void handler_sinal(int s)
{
    (void)s;
    parar = 1;
}

static void hora_atual(char *buf, size_t tam)
{
    time_t t = time(NULL);
    struct tm tm_local;
    localtime_r(&t, &tm_local);
    strftime(buf, tam, "%H:%M:%S", &tm_local);
}

static void data_hora(time_t t, char *buf, size_t tam)
{
    struct tm tm_local;
    localtime_r(&t, &tm_local);
    strftime(buf, tam, "%Y-%m-%d %H:%M:%S", &tm_local);
}

static void processar_linha(const char *linha, int contador,
                            char *sensor_id_out, size_t id_tam)
{
    char id[64], hora[16];
    float temp, luz, umid;

    if (sscanf(linha, "%63[^,],%f,%f,%f", id, &temp, &luz, &umid) == 4) {
        hora_atual(hora, sizeof(hora));
        printf("[%s] msg #%04d  %-12s  T=%5.1fC  L=%5.1f%%  U=%5.1f%%\n",
               hora, contador, id, temp, luz, umid);
        fflush(stdout);

        if (sensor_id_out && id_tam > 0) {
            snprintf(sensor_id_out, id_tam, "%s", id);
        }
    } else {
        printf("  [linha mal formada]: %s\n", linha);
        fflush(stdout);
    }
}

/* Append atomico por linha (POSIX O_APPEND ate PIPE_BUF = 4 KB). */
static void registrar_sessao(const char *sensor_id, const char *ip, int porta,
                              pid_t pid, time_t inicio, time_t fim, int total)
{
    FILE *f;
    char data_ini[32], data_fim[32];
    long duracao_s;

    data_hora(inicio, data_ini, sizeof(data_ini));
    data_hora(fim, data_fim, sizeof(data_fim));
    duracao_s = (long)(fim - inicio);

    f = fopen(ARQUIVO_LOG, "a");
    if (!f) {
        perror("Erro ao abrir log");
        return;
    }
    fprintf(f,
            "[%s] sensor=%s | de %s:%d | pid=%d | inicio=%s | duracao=%lds | mensagens=%d\n",
            data_fim,
            (sensor_id && *sensor_id) ? sensor_id : "(desconhecido)",
            ip, porta, (int)pid,
            data_ini, duracao_s, total);
    fclose(f);
}

static void atender_cliente(int cliente_fd, struct sockaddr_in *cliente_addr)
{
    char ip[INET_ADDRSTRLEN];
    char hora[16];
    char buffer[BUFFER_SIZE];
    char linha[BUFFER_SIZE];
    char sensor_id[64] = "";
    size_t lin_len = 0;
    int contador = 0;
    int porta_cliente = ntohs(cliente_addr->sin_port);
    time_t inicio = time(NULL);
    time_t fim;

    inet_ntop(AF_INET, &cliente_addr->sin_addr, ip, sizeof(ip));
    hora_atual(hora, sizeof(hora));
    printf("[%s] [pid %d] Cliente conectado de %s:%d\n",
           hora, getpid(), ip, porta_cliente);
    fflush(stdout);

    while (!parar) {
        ssize_t n = recv(cliente_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            break;
        }

        for (ssize_t i = 0; i < n; i++) {
            if (buffer[i] == '\n') {
                linha[lin_len] = '\0';
                contador++;
                processar_linha(linha, contador, sensor_id, sizeof(sensor_id));
                lin_len = 0;
            } else if (lin_len < sizeof(linha) - 1) {
                linha[lin_len++] = buffer[i];
            }
        }
    }

    fim = time(NULL);
    hora_atual(hora, sizeof(hora));
    printf("[%s] [pid %d] Cliente %s:%d desconectado (%d mensagens, %lds) -> %s\n",
           hora, getpid(), ip, porta_cliente,
           contador, (long)(fim - inicio), ARQUIVO_LOG);
    fflush(stdout);

    registrar_sessao(sensor_id, ip, porta_cliente, getpid(),
                     inicio, fim, contador);
}

int main(int argc, char *argv[])
{
    int porta = (argc > 1) ? atoi(argv[1]) : PORTA_PADRAO;
    int servidor_fd;
    struct sockaddr_in server_addr;
    int opt = 1;

    if (porta <= 0 || porta > 65535) {
        fprintf(stderr, "Porta invalida: %d\n", porta);
        return 1;
    }

    signal(SIGINT, handler_sinal);
    signal(SIGTERM, handler_sinal);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN); /* evita filhos zumbis sem precisar de waitpid */

    servidor_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor_fd < 0) {
        perror("socket");
        return 1;
    }

    setsockopt(servidor_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((uint16_t)porta);

    if (bind(servidor_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(servidor_fd);
        return 1;
    }

    if (listen(servidor_fd, 8) < 0) {
        perror("listen");
        close(servidor_fd);
        return 1;
    }

    printf("================================================================\n");
    printf("  Servidor de telemetria TCP (concorrente, fork por cliente)\n");
    printf("  Porta: %d  |  PID pai: %d  |  Ctrl+C para encerrar\n",
           porta, getpid());
    printf("================================================================\n");
    printf("Aguardando clientes...\n");
    fflush(stdout);

    while (!parar) {
        struct sockaddr_in cliente_addr;
        socklen_t cliente_len = sizeof(cliente_addr);
        int cliente_fd = accept(servidor_fd,
                                (struct sockaddr *)&cliente_addr, &cliente_len);
        if (cliente_fd < 0) {
            if (parar) {
                break;
            }
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(cliente_fd);
            continue;
        }

        if (pid == 0) {
            close(servidor_fd);
            atender_cliente(cliente_fd, &cliente_addr);
            close(cliente_fd);
            _exit(0);
        }

        close(cliente_fd);
    }

    close(servidor_fd);
    printf("\nServidor encerrado.\n");
    return 0;
}
