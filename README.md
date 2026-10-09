# 🚜 Robótica Trator 2012

<p align="center">
  <img src="https://img.shields.io/badge/MCU-PIC16F84-blue?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Clock-4%20MHz-green?style=for-the-badge" />
  <img src="https://img.shields.io/badge/RAM-68%20bytes-orange?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Flash-1%20KB-red?style=for-the-badge" />
  <img src="https://img.shields.io/badge/RTOS-não-critical?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Ano-2012-yellow?style=for-the-badge" />
</p>

<p align="center">
  <b>Código-fonte e documentação de um robô trator que venceu uma competição de robótica em 2012.</b><br>
  Implementado sobre um <b>PIC16F84</b> — 1 KB de flash, 68 bytes de RAM, 4 MHz.
</p>

<p align="center">
  <b>📊 Métricas reais de compilação</b><br>
  <code>ROM used:    498 words (49%)</code><br>
  <code>Largest free fragment: 526</code><br>
  <code>RAM used:    41 bytes (60%) at main() level</code><br>
  <code>             42 bytes (62%) worst case</code><br>
  <code>Stack used:  1 location (0 in main + 1 for interrupts)</code><br>
  <code>Stack size:  8</code>
</p>

---

## 💡 A ideia central

> **Determinismo em sistemas embarcados não depende de hardware caro — depende de método.**

Em 2012, muito antes de ESP32, RTOS acessíveis e bibliotecas prontas, este projeto
foi construído com um microcontrolador de **8 bits** e **68 bytes de RAM**.
Ainda assim, entregou:

- ⏱️ Escalonamento determinístico com prazos conhecidos
- ⚡ PWM por software para dois motores independentes
- 🕒 Relógio de tempo real sem hardware dedicado
- 🛡️ Proteção de ponte H com *dead-time* em software
- 🧭 Máquina de estados para navegação autônoma

Tudo isso **sem RTOS**, **sem preempção** e **sem um único mutex pesado**.

---

## 📂 Conteúdo do repositório

### 💻 [`source/`](source/) — Código-fonte

Código em **linguagem C** para o compilador **CCS**, escrito originalmente para
o PIC16F84. A versão disponibilizada aqui é **didática**: mantém o comportamento
original do robô, mas com comentários pedagógicos explicando cada decisão de
projeto.

**O que você vai encontrar lá dentro:**

- 🧠 **Escalonamento cíclico** — padrão *cyclic executive* com 6 tarefas periódicas, todas múltiplas de 1 ms
- ⏱️ **Base de tempo** — ISR do Timer0 disparando a cada 1 ms — coração determinístico do sistema
- ⚡ **PWM por software** — sem módulo CCP no PIC16F84: PWM gerado na ISR com resolução de 1 ms
- 🕒 **Relógio de tempo real** — cascata segundo → minuto → hora → dia → ano, tudo em software
- 🔒 **Semáforos por bit** — exclusão mútua simples, sem mutex pesado, ocupando bits de um único byte
- 🧭 **Máquina de estados** — padrão de patrulhamento alternando esquerda → frente → direita → frente
- 🛡️ **Dead-time na ponte H** — 10 µs entre desligar um lado e ligar o outro — evita *shoot-through*
- 🚨 **Watchdog lógico** — parada automática após 20 s, cumprindo regra da competição

> 💡 **Detalhe que impressiona**: tudo isso convive em **68 bytes de RAM**.
> É menos memória do que uma única linha de log de um sistema moderno.

---

### 📚 [`docs/`](docs/) — Diagramas de tempo

Diagramas em **PNG** que ilustram visualmente o funcionamento temporal do sistema,
perfeitos para aulas, apresentações e estudo do código.

**Diagramas disponíveis:**

- ⏱️ **ISR do Timer0** — base de tempo de 1 ms e tudo que depende dela
- 🔄 **Escalonamento cíclico** — os períodos fixos das 6 tarefas
- ⚡ **PWM por software** — duty cycles para diferentes velocidades
- 🛡️ **Dead-time** — proteção da ponte H na troca de sentido
- 🧭 **Máquina de estados** — padrão de navegação do robô

---

## 🎯 Por que este projeto importa hoje

Vivemos em uma era de **abstração fácil**: `delay()`, RTOS, frameworks e placas
com dezenas de núcleos. Isso é ótimo — mas esconde os fundamentos.

Este repositório é um **contra-exemplo didático**:

- Mostra que **método importa mais que hardware**.
- Prova que **determinismo é uma decisão de projeto**, não um recurso de placa.
- Ensina que **restrição de recursos força clareza arquitetural**.

Ideal para:

- 🎓 **Aulas** de sistemas embarcados e tempo real
- 🧪 **Laboratórios** de microcontroladores
- 🏆 **Equipes de competição** que querem entender o que há por baixo 
- 🤓 **Curiosos** que querem ver código "de verdade" com 68 bytes de RAM

---

## 🧠 Frase-síntese

> *"Em 2012 não tínhamos ESP32, não tínhamos RTOS, não tínhamos bibliotecas.
> Tínhamos um PIC16F84, um timer de 1 ms e método.
> Foi o suficiente para vencer. Determinismo não se compra — se projeta."*

---

## 👤 Autor

**Marcos Roberto Braga** — código original de 2012.
