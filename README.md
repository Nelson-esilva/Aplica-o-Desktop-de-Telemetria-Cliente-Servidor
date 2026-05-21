# Telemetria TCP — cliente sensor + servidor concorrente

Miniprojeto em **C** para Linux que implementa um par cliente/servidor TCP
para envio contínuo de telemetria simulada (temperatura, luminosidade e
umidade). O servidor é concorrente e atende vários clientes em paralelo
usando `fork()`. A cada sessão encerrada, grava uma linha de log com os
metadados da conexão.

## Compilar

```bash
make
```

Gera dois binários: `servidor` e `cliente`.

## Executar

### Terminal 1 — servidor

```bash
./servidor
```

Saída esperada:

```
================================================================
  Servidor de telemetria TCP (concorrente, fork por cliente)
  Porta: 5000  |  PID pai: 12345  |  Ctrl+C para encerrar
================================================================
Aguardando clientes...
```

### Terminal 2 (e 3, 4, ...) — um ou mais clientes

```bash
./cliente                                  # ID padrão sensor-01
./cliente 127.0.0.1 5000 estufa-norte 700  # ID e intervalo customizados
./cliente 127.0.0.1 5000 estufa-sul 900
```

O servidor mostra as mensagens **intercaladas** dos vários clientes, com o
PID do filho que está atendendo cada um:

```
[11:37:02] [pid 73071] Cliente conectado de 127.0.0.1:59214
[11:37:02] msg #0001  estufa-norte  T= 21.0C  L= 70.2%  U= 63.0%
[11:37:02] [pid 73076] Cliente conectado de 127.0.0.1:59218
[11:37:02] msg #0001  estufa-sul    T= 28.7C  L= 27.6%  U= 51.0%
[11:37:03] msg #0002  estufa-norte  T= 33.0C  L= 63.6%  U= 86.8%
[11:37:03] msg #0002  estufa-sul    T= 31.8C  L= 29.8%  U= 75.5%
```

## Parâmetros

```bash
./servidor [porta]
# padrão: 5000

./cliente [host] [porta] [sensor_id] [intervalo_ms]
# padrão: 127.0.0.1 5000 sensor-01 1000
```

Exemplos:

```bash
./cliente 127.0.0.1 5000 sensor-rapido 250    # 4 envios por segundo
./cliente 127.0.0.1 5000 sensor-lento 3000    # 1 envio a cada 3s
./cliente 192.168.0.42 5000 sensor-remoto     # outra máquina na LAN
```

## Protocolo

Texto simples, uma leitura por linha:

```
sensor-id,temperatura,luminosidade,umidade\n
```

Exemplo do que trafega pela rede:

```
sensor-01,25.3,80.1,55.0
```

## Concorrência via `fork`

Quando `accept()` retorna uma nova conexão, o servidor chama `fork()`:

- O **processo filho** atende esse cliente (`recv` em loop) e termina quando
  o cliente desconecta.
- O **processo pai** volta para `accept()`, pronto para a próxima conexão.

Cada cliente vira um processo independente, com seu próprio PID. Por isso o
servidor consegue receber telemetria de vários sensores em paralelo.

`signal(SIGCHLD, SIG_IGN)` evita que os filhos virem processos zumbis depois
de terminarem.

## Log de sessões (`sessoes.log`)

Cada vez que um cliente desconecta, o servidor anexa uma linha em
`sessoes.log` com:

- data e hora da desconexão
- ID do sensor
- IP e porta do cliente
- PID do processo filho que o atendeu
- data e hora do início da conexão
- duração em segundos
- total de mensagens recebidas

Exemplo:

```
[2026-05-21 11:54:41] sensor=estufa-norte | de 127.0.0.1:36356 | pid=73952 | inicio=2026-05-21 11:54:37 | duracao=4s | mensagens=6
[2026-05-21 11:54:43] sensor=sensor-quintal | de 127.0.0.1:36372 | pid=73958 | inicio=2026-05-21 11:54:38 | duracao=5s | mensagens=5
[2026-05-21 11:54:44] sensor=estufa-sul | de 127.0.0.1:36358 | pid=73955 | inicio=2026-05-21 11:54:38 | duracao=6s | mensagens=8
```

Para acompanhar ao vivo:

```bash
tail -f sessoes.log
```

A escrita em modo append (`fopen("a")`) é atômica até 4 KB por linha em
sistemas POSIX, então os processos filhos não corrompem o arquivo um do
outro.

## Encerrar

`Ctrl+C` no servidor encerra o processo principal; filhos terminam quando
seus clientes fecham. `Ctrl+C` num cliente só fecha aquele cliente — o
servidor registra a sessão e continua aceitando novas conexões.

## Estrutura do projeto

```
.
├── Makefile
├── servidor.c    # servidor TCP concorrente
├── cliente.c     # cliente que simula um sensor
├── README.md
└── sessoes.log   # gerado em runtime (no .gitignore)
```
