///////////////////////////////////////////////////////////////////////////////
//                        ROBÓTICA TRATOR — VERSÃO DIDÁTICA                  //
//                                                                           //
//  Autor: Marcos Roberto Braga                                              //
//  Código original: 28/05/2012                                              //
//  Ajustes finais : 22/06/2012                                              //
//  Versão didática: comentada para fins pedagógicos                         //
//                                                                           //
//  Objetivo desta versão: mostrar que DETERMINISMO em sistemas embarcados   //
//  não depende de hardware caro, e sim de MÉTODO.                           //
//                                                                           //
//  Hardware: PIC16F84 @ 4 MHz — 1 KB flash, 68 bytes RAM.                   //
//  Técnica:  escalonamento cíclico (cyclic executive) + ISR de 1 ms.        //
///////////////////////////////////////////////////////////////////////////////

#include <16F84.h>

// [DIDÁTICO] O clock é a "régua" de tempo do sistema.
// O compilador usa esse valor para calcular delay_us/delay_ms.
// Se você mudar o cristal, precisa mudar aqui.
#if __device__ == 84
#use delay(clock=4000000)
#endif

// [DIDÁTICO] Trava de compilação: impede gerar binário para outro PIC.
// Isso evita erros silenciosos ao trocar de hardware.
#if __device__ != 84
#error MCU errada! Inclua *.h correto.
#endif

#opt 5                       // [DIDÁTICO] Otimização máxima: essencial com 1 KB.
#include "Hardware_robo_trator.h"  // [DIDÁTICO] Camada de abstração do hardware.
#priority timer0             // [DIDÁTICO] Única fonte de IRQ: Timer0.
#ignore_warnings 203,202,201 // [DIDÁTICO] Silencia warnings comuns do CCS.

//=============================================================================
// [DIDÁTICO] PERÍODOS DAS TAREFAS — a espinha dorsal do determinismo.
// Cada tarefa roda em múltiplo exato de 1 ms (base do Timer0).
// Isso garante jitter conhecido e previsível.
// [EXERCÍCIO] Por que 2000 (T4) precisa ser múltiplo de 1 (T1)?
//=============================================================================
#define tempo_task_1 1
#define tempo_task_2 1
#define tempo_task_3 1
#define tempo_task_4 2000
#define tempo_task_5 1
#define tempo_task_6 100

//=============================================================================
// [DIDÁTICO] POTÊNCIAS DOS MOTORES — valores de duty cycle.
// O PWM tem período de 50 ms (20 Hz) e resolução de 1 ms.
// Logo, duty máximo útil = 50. O autor usou 48 (margem de segurança).
// [EXERCÍCIO] Calcule a resolução do PWM em % (1/50 = 2% por passo).
//=============================================================================
#define max_pt_d 48
#define max_pt_e 48
#define media_pt_d 10
#define media_pt_e 10
#define minima_pt_d 6
#define minima_pt_e 6
#define nula_pt_d 0
#define nula_pt_e 0
#define media_alta_pt_d 15
#define media_alta_pt_e 15
#define ini_gradiente 0
#define ini_freq_pwm 50        // [DIDÁTICO] 50 ms → 20 Hz
#define tempo_pa 35            // [DIDÁTICO] 35 × 100 ms = 3,5 s de movimento da pá

//=============================================================================
// [DIDÁTICO] VARIÁVEIS DO ESCALONADOR
// Cada tarefa tem um contador regressivo. Quando chega a 0, a tarefa roda.
// O contador é rearmado imediatamente no main() — isso mantém o período fixo.
//=============================================================================
int8  tick = 0;                // [DIDÁTICO] Valor de recarga do TMR0
int16 cont_int_timer0 = 0;     // [DIDÁTICO] Conta IRQs para formar 1 s
int16 timer_1s = 0;            // [DIDÁTICO] Constante = 1000
int8  semaforo_task = 0;       // [DIDÁTICO] Byte de semáforos (bit a bit)
int8  cont_task_1 = tempo_task_1;
int8  cont_task_2 = tempo_task_2;
int8  cont_task_3 = tempo_task_3;
int16 cont_task_4 = tempo_task_4;   // [DIDÁTICO] int16 porque 2000 > 255
int8  cont_task_5 = tempo_task_5;
int8  cont_task_6 = tempo_task_6;

//=============================================================================
// [DIDÁTICO] VARIÁVEIS DO PWM POR SOFTWARE (motor direito)
// Três variáveis trabalham juntas:
//   gradiente_pwm_d  → "ponteiro" que avança de 0 a freq_pwm_d
//   ciclo_ativo_pwm_d → largura do pulso (duty)
//   freq_pwm_d        → período total (50 ms)
// Comparação: gradiente <= duty → liga; senão → desliga.
//=============================================================================
int8 gradiente_pwm_d = ini_gradiente;
int8 ciclo_ativo_pwm_d = media_pt_d;
int8 freq_pwm_d = ini_freq_pwm;

//=============================================================================
// [DIDÁTICO] VARIÁVEIS DO PWM POR SOFTWARE (motor esquerdo)
//=============================================================================
int8 gradiente_pwm_e = ini_gradiente;
int8 ciclo_ativo_pwm_e = media_pt_e;
int8 freq_pwm_e = ini_freq_pwm;

//=============================================================================
// [DIDÁTICO] CONTROLE DA PÁ (por tempo, não por posição)
// Não há sensor de fim de curso: o sistema simplesmente aciona o motor
// por um tempo fixo (3,5 s) e assume que a pá chegou ao destino.
// [EXERCÍCIO] O que aconteceria se a pá travasse? Como detectar?
//=============================================================================
int8 tempo_motor_pa = tempo_pa;

//=============================================================================
// [DIDÁTICO] FLAGS DO SISTEMA — byte usado como "vetor de bits"
// Economia extrema: cada bit é uma flag. Em 68 bytes de RAM, isso importa.
//=============================================================================
int8 sistema = 0;

//=============================================================================
// [DIDÁTICO] CONTROLE DE DIREÇÃO (máquina de estados 1..4)
//=============================================================================
int8 chave_de_direcao = 0;

//=============================================================================
// [DIDÁTICO] RELÓGIO DE TEMPO REAL (RTC por software)
// Cascata: segundo → minuto → hora → dia → ano.
// Tudo dentro da ISR, sem hardware dedicado.
//=============================================================================
int8  segundo = 0;
int8  minuto  = 0;
int8  hora    = 0;
int16 dia     = 0;
int16 ano     = 0;

//=============================================================================
// [DIDÁTICO] BITS DE SEMÁFORO
// 0 = tarefa liberada | 1 = tarefa bloqueada
// Permite exclusão mútua simples sem mutex pesado.
// [CUIDADO] Note que sema_task6 aponta para o bit 6 (não 5!).
//           Provavelmente um bug sutil do autor original.
//=============================================================================
#bit sema_task1 = semaforo_task.0
#bit sema_task2 = semaforo_task.1
#bit sema_task3 = semaforo_task.2
#bit sema_task4 = semaforo_task.3
#bit sema_task5 = semaforo_task.4
#bit sema_task6 = semaforo_task.6

//=============================================================================
// [DIDÁTICO] BITS DE FLAGS DO SISTEMA
//=============================================================================
#bit flag_direcao_d = sistema.0   // 1=frente, 0=trás
#bit flag_direcao_e = sistema.1   // 1=frente, 0=trás
#bit posicao_pa     = sistema.2   // 1=abaixo, 0=levantada

//=============================================================================
// [DIDÁTICO] ISR DO TIMER0 — o "coração" determinístico do sistema
// Executa a cada 1 ms. TUDO que depende de tempo é derivado daqui.
// [POR QUÊ] Manter a ISR curta é essencial: se ela demorar > 1 ms,
//           o sistema perde o determinismo.
// [EXERCÍCIO] Meça o tempo de execução da ISR com osciloscópio em RA0.
//=============================================================================
#int_timer0
void timer0_isr(void)
{
   ra0 = !ra0;                    // [DIDÁTICO] "Sistema vivo" (debug)

   ++gradiente_pwm_d;             // [DIDÁTICO] Base do PWM por software
   ++gradiente_pwm_e;

   // [DIDÁTICO] Contadores regressivos das tarefas
   if (cont_task_1 > 0){--cont_task_1;}
   if (cont_task_2 > 0){--cont_task_2;}
   if (cont_task_3 > 0){--cont_task_3;}
   if (cont_task_4 > 0){--cont_task_4;}
   if (cont_task_5 > 0){--cont_task_5;}
   if (cont_task_6 > 0){--cont_task_6;}

   // -------- RTC: 1000 IRQs = 1 segundo --------
   ++cont_int_timer0;
   if (cont_int_timer0 == timer_1s) {
      cont_int_timer0 = 0;
      ++segundo;

      if (segundo == 60){ segundo=0; ++minuto; }
      if (minuto  == 60){ minuto=0; segundo=0; ++hora; }
      if (hora    == 24){ hora=0; minuto=0; segundo=0; ++dia; }
      if (dia     == 365){ dia=0; hora=0; minuto=0; segundo=0; ++ano; }
      if (ano     == 2000){ ano=0; dia=0; hora=0; minuto=0; segundo=0; }

      tmr0 = tick;   // [DIDÁTICO] Recarrega TMR0 para manter 1 ms exato
   }
}

//=============================================================================
// [DIDÁTICO] CONFIGURAÇÃO DOS I/Os
// [CUIDADO] Sempre zere os registradores de saída ANTES de configurar TRIS.
//           Isso evita acionamento indevido de motores no boot.
//=============================================================================
void config_ios(void)
{
   porta = 0b00000000;
   portb = 0b00000000;
   trisa = 0b11111110;   // RA0 = saída (led), demais = entradas
   trisb = 0b00000001;   // RB0 = entrada (chave), demais = saídas
}

//=============================================================================
// [DIDÁTICO] CONFIGURAÇÃO DO TIMER0
// Prescaler 1:4 + clock 4 MHz → contagem a cada 1 µs.
// Recarga = 6 → interrupção a cada ~1 ms (ver cálculo no datasheet).
//=============================================================================
void config_timer0(void)
{
   tick = 6;
   timer_1s = 1000;
   psa=0; ps0=1; ps1=0; ps2=0;   // [DIDÁTICO] Prescaler 1:4 no Timer0
   t0cs=0;                        // [DIDÁTICO] Clock interno Fosc/4
   tmr0=tick;
   t0ie=1;                        // [DIDÁTICO] Habilita IRQ do Timer0
   gie=1;                         // [DIDÁTICO] Habilita IRQs globais
}

//=============================================================================
// [DIDÁTICO] PRESET INICIAL — estado seguro conhecido
// Lê a chave liga/desliga e bloqueia tarefas conforme o estado.
//=============================================================================
void config_preset(void)
{
   flag_direcao_d = 1;   // Motores para frente
   flag_direcao_e = 1;
   posicao_pa = 1;       // Pá começa embaixo
   da0 = 1;
   if (!ra0) {           // [DIDÁTICO] Chave desligada → bloqueia tarefas
      sema_task3 = 1;
      sema_task4 = 1;
      sema_task6 = 1;
   } else {              // [DIDÁTICO] Chave ligada → bloqueia bloqueio_motor
      sema_task5 = 1;
   }
   da0 = 0;
}

//=============================================================================
// [DIDÁTICO] TAREFA 1 — PWM POR SOFTWARE (motor direito)
// Compara gradiente com duty para gerar o pulso.
// [CUIDADO] O delay_us(10) é DEAD-TIME: evita curto-circuito na ponte H.
//=============================================================================
void controle_motor_direita(void)
{
   if (gradiente_pwm_d <= ciclo_ativo_pwm_d) {   // Fase ATIVA
      if (flag_direcao_d == 1) {
         traz_d = 0;
         delay_us(10);                            // DEAD-TIME
         frente_d = 1;
      }
      if (flag_direcao_d == 0) {
         frente_d = 0;
         delay_us(10);                            // DEAD-TIME
         traz_d = 1;
      }
   }
   if (gradiente_pwm_d >= ciclo_ativo_pwm_d) {   // Fase INATIVA
      frente_d = 0;
      traz_d = 0;
   }
   if (gradiente_pwm_d >= freq_pwm_d) {          // Fim do período
      gradiente_pwm_d = 0;
   }
}

//=============================================================================
// [DIDÁTICO] TAREFA 2 — PWM POR SOFTWARE (motor esquerdo)
// Idêntica à tarefa 1, espelhada.
//=============================================================================
void controle_motor_esquerda(void)
{
   if (gradiente_pwm_e <= ciclo_ativo_pwm_e) {
      if (flag_direcao_e == 1) {
         traz_e = 0;
         delay_us(10);
         frente_e = 1;
      }
      if (flag_direcao_e == 0) {
         frente_e = 0;
         delay_us(10);
         traz_e = 1;
      }
   }
   if (gradiente_pwm_e >= ciclo_ativo_pwm_e) {
      frente_e = 0;
      traz_e = 0;
   }
   if (gradiente_pwm_e >= freq_pwm_e) {
      gradiente_pwm_e = 0;
   }
}

//=============================================================================
// [DIDÁTICO] TAREFA 3 — LEITURA DE SENSORES E REAÇÃO
// Comportamento reativo: cada sensor ajusta direção/potência.
// [POR QUÊ] Roda a cada 1 ms → resposta rápida a obstáculos.
//=============================================================================
void entrada_sensores(void)
{
   // ---- Sensor frontal ----
   if (!sensor_frente) {
      ciclo_ativo_pwm_d = max_pt_d;
      ciclo_ativo_pwm_e = max_pt_e;
      flag_direcao_d = 1;
      flag_direcao_e = 1;

      // Reação da pá: alterna entre subir e descer
      if (posicao_pa == 0) {          // Estava levantada → abaixa
         liga_serra = 1;
         abaixa = 1;
         sobe = 0;
         if (tempo_motor_pa == 0) {
            tempo_motor_pa = tempo_pa;
            sema_task6 = 0;
            sema_task4 = 1;
         }
      }
      if (posicao_pa == 1) {          // Estava abaixada → levanta
         liga_serra = 1;
         abaixa = 0;
         sobe = 1;
         if (tempo_motor_pa == 0) {
            tempo_motor_pa = tempo_pa;
            sema_task6 = 0;
            sema_task4 = 1;
         }
      }
   }

   // ---- Sensor traseiro ----
   if (!sensor_traz) {
      ciclo_ativo_pwm_d = max_pt_d;
      ciclo_ativo_pwm_e = max_pt_e;
      flag_direcao_d = 0;
      flag_direcao_e = 0;
   }

   // ---- Sensor lateral ----
   if (!sensor_lateral) {
      flag_direcao_d = 1;
      flag_direcao_e = 1;
      ciclo_ativo_pwm_d = max_pt_d;
      ciclo_ativo_pwm_e = max_pt_e;
   }

   // ---- Limite frontal (borda da arena) ----
   if (!sensor_limite_frente) {
      flag_direcao_d = 0;
      flag_direcao_e = 0;
      ciclo_ativo_pwm_d = max_pt_d;
      ciclo_ativo_pwm_e = max_pt_e;
   }

   // ---- Limite traseiro ----
   if (!sensor_limite_traz) {
      flag_direcao_d = 1;
      flag_direcao_e = 1;
      ciclo_ativo_pwm_d = media_alta_pt_d;   // Reduz potência
      ciclo_ativo_pwm_e = media_alta_pt_e;
   }
}

//=============================================================================
// [DIDÁTICO] TAREFA 4 — CONTROLE DE DIREÇÃO (máquina de estados)
// A cada 2 s, alterna: esquerda → frente → direita → frente → ...
// [EXERCÍCIO] Desenhe o diagrama de estados dessa máquina.
//=============================================================================
void controle_direcao(void)
{
   ++chave_de_direcao;

   if (chave_de_direcao == 1) {           // Giro à esquerda
      ciclo_ativo_pwm_e = minima_pt_e;
      ciclo_ativo_pwm_d = max_pt_d;
      flag_direcao_d = 1;
      flag_direcao_e = 0;
   }
   if (chave_de_direcao == 2) {           // Frente
      ciclo_ativo_pwm_e = media_alta_pt_e;
      ciclo_ativo_pwm_d = media_alta_pt_d;
      flag_direcao_d = 1;
      flag_direcao_e = 1;
   }
   if (chave_de_direcao == 3) {           // Giro à direita
      ciclo_ativo_pwm_e = max_pt_e;
      ciclo_ativo_pwm_d = minima_pt_d;
      flag_direcao_d = 0;
      flag_direcao_e = 1;
   }
   if (chave_de_direcao == 4) {           // Frente e reinicia
      ciclo_ativo_pwm_e = media_alta_pt_e;
      ciclo_ativo_pwm_d = media_alta_pt_d;
      flag_direcao_d = 1;
      flag_direcao_e = 1;
      chave_de_direcao = 0;
   }
}

//=============================================================================
// [DIDÁTICO] TAREFA 5 — BLOQUEIO APÓS 20 SEGUNDOS
// Regra da competição: robô deve parar sozinho após 20 s.
// [CUIDADO] O while(1) é um "travamento controlado": não retorna nunca.
//           Em sistema embarcado, isso é aceitável APÓS cumprir a regra.
//=============================================================================
void bloqueia_motor(void)
{
   if (segundo == 20) {
      while (true) {
         frente_d = 0;
         frente_e = 0;
      }
   }
}

//=============================================================================
// [DIDÁTICO] TAREFA 6 — TEMPO DE ACIONAMENTO DA PÁ
// Decrementa a cada 100 ms. Ao chegar a zero: desliga atuadores,
// alterna posição lógica e libera/bloqueia tarefas.
//=============================================================================
void tempo_de_pa(void)
{
   if (tempo_motor_pa > 0) {
      --tempo_motor_pa;
   }
   if (tempo_motor_pa == 0) {
      abaixa = 0;
      sobe = 0;
      liga_serra = 0;
      posicao_pa = !posicao_pa;
      sema_task6 = 1;   // Bloqueia esta tarefa
      sema_task4 = 0;   // Libera controle de direção
   }
}

//=============================================================================
// [DIDÁTICO] MAIN — DESPACHADOR CÍCLICO
// Padrão "cyclic executive": cada tarefa tem período fixo e é executada
// quando seu contador chega a zero. Nada de RTOS, nada de preempção.
// [POR QUÊ] Determinismo total: sabe-se exatamente quando cada tarefa roda.
//
// Autor: Marcos Roberto Braga (2012)
//=============================================================================
void main(void)
{
   config_ios();
   config_preset();
   config_timer0();

   while (true) {
      // [DIDÁTICO] Cada bloco segue o mesmo padrão:
      //   1. Verifica se o contador chegou a zero
      //   2. Rearma o contador (mantém o período)
      //   3. Se o semáforo permitir, executa a tarefa

      if (cont_task_1 == 0) {
         cont_task_1 = tempo_task_1;
         if (!sema_task1) { controle_motor_direita(); }
      }
      if (cont_task_2 == 0) {
         cont_task_2 = tempo_task_2;
         if (!sema_task2) { controle_motor_esquerda(); }
      }
      if (cont_task_3 == 0) {
         cont_task_3 = tempo_task_3;
         if (!sema_task3) { entrada_sensores(); }
      }
      if (cont_task_4 == 0) {
         cont_task_4 = tempo_task_4;
         if (!sema_task4) { controle_direcao(); }
      }
      if (cont_task_5 == 0) {
         cont_task_5 = tempo_task_5;
         if (!sema_task5) { bloqueia_motor(); }
      }
      if (cont_task_6 == 0) {
         cont_task_6 = tempo_task_6;
         if (!sema_task6) { tempo_de_pa(); }
      }
   }
}
//=============================================================================
