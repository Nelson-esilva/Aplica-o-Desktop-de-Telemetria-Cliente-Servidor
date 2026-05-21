# Telemetria TCP — cliente sensor + servidor concorrente

Atividade Aula 5 — Microprocessadores e Microcontroladores.  
Aplicação em **C** para Linux que une dois exemplos da aula:

- **Servidor TCP concorrente** (`socket`, `bind`, `listen`, `accept`, `fork`, `recv`)
- **Sensor simulado** (temperatura, luminosidade, umidade)

O cliente representa um dispositivo embarcado enviando leituras periódicas
para um servidor central. O servidor usa `fork()` e atende **vários clientes
ao mesmo tempo**, cada um num processo filho separado. A rede aparece como
**periférico de comunicação**.

## Compilar

```bash
make
```

Gera dois binários: `servidor` e `cliente`.

## Executar — múltiplos terminais

### Terminal 1: o servidor

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

### Terminal 2 (e 3, 4, ...): um ou mais clientes

```bash
./cliente                                  # ID padrão sensor-01
./cliente 127.0.0.1 5000 estufa-norte 700  # outro ID, intervalo 700ms
./cliente 127.0.0.1 5000 estufa-sul 900    # outro ID, intervalo 900ms
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

## Parâmetros opcionais

```bash
./servidor 5000                              # porta (padrão 5000)
./cliente 127.0.0.1 5000 sensor-01 1000      # host, porta, id, intervalo_ms
```

Exemplos úteis na demonstração:

```bash
./cliente 127.0.0.1 5000 sensor-02 500       # mais rápido (500 ms)
./cliente 127.0.0.1 5000 estufa-norte 2000   # ID diferente, intervalo de 2s
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

## Como o servidor lida com vários clientes (`fork`)

Quando `accept()` retorna uma nova conexão, o servidor chama `fork()`:

- O **processo filho** atende esse cliente (`recv` em loop) e termina quando
  o cliente desconecta.
- O **processo pai** volta para `accept()`, pronto para a próxima conexão.

Cada cliente vira um processo independente, com seu próprio PID. Por isso o
servidor consegue receber telemetria de vários sensores em paralelo.

`signal(SIGCHLD, SIG_IGN)` evita que os filhos virem processos zumbis depois
de terminarem.

## Log de sessões (`sessoes.log`)

Cada vez que um cliente desconecta (Ctrl+C no terminal do cliente, ou
qualquer outro motivo), o servidor anexa uma linha em `sessoes.log` com:

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

Para acompanhar ao vivo enquanto faz a demonstração:

```bash
tail -f sessoes.log
```

A escrita em modo append (`fopen("a")`) é atômica até 4 KB por linha em
sistemas POSIX, então os processos filhos não corrompem o arquivo um do
outro.

## Relação com a Aula 5

- **Sockets TCP** — exemplo `servidor_tcp.py` / `cliente_tcp.py` do PDF, agora em C.
- **Periférico de rede** — placa de rede aparece como um periférico do sistema.
- **Sensor simulado** — analogia direta com ADC/UART de um microcontrolador.
- **Cliente embarcado** — o `cliente` representa um MCU mandando telemetria
  para um servidor central (como ESP32 → broker MQTT, ou Arduino → PC).
- **Processos e `fork`** — modelo clássico de servidor Unix para concorrência.

## Roteiro para apresentação (~3 min)

1. Explicar a ideia: vários sensores (clientes) → rede → um servidor central
   que recebe e registra cada sessão.
2. Abrir terminal 1: `./servidor` (mostra que aguarda clientes).
3. Abrir terminal 2: `tail -f sessoes.log` (vai aparecendo cada desconexão).
4. Abrir terminais 3 e 4: dois clientes com IDs diferentes
   (`estufa-norte` e `estufa-sul`).
5. Mostrar no servidor as mensagens dos dois chegando intercaladas, com PIDs
   diferentes para cada filho.
6. Fechar um cliente (Ctrl+C) — aparece a linha de log no `tail` e o servidor
   continua recebendo do outro.
7. Subir um novo cliente com outro ID e repetir.
8. Ao final, mostrar o `sessoes.log` completo com todas as sessões registradas.

## Encerrar

`Ctrl+C` no servidor encerra tudo (pai mata o processo principal; filhos
terminam quando seus clientes fecham). `Ctrl+C` num cliente só fecha aquele
cliente.
