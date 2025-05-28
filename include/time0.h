#ifndef TIME0_H
#define TIME0_H

#include <time.h>
#include <pthread.h>

// Enum per la politica di scheduling del task
typedef enum { OTHER, FIFO, RR, DEADLINE } schedulazione;

// Struttura che contiene tutti i parametri necessari per la gestione di un task periodico
typedef struct 
{
    int id;                    // Identificativo univoco del task
    struct timespec at, dl;    // at: activation time (prossima attivazione), dl: deadline assoluta
    int periodo;               // Periodo del task in millisecondi
    int deadline;              // Deadline relativa in millisecondi
    int priorita;              // Priorità del task 
    schedulazione sched;       // Tipo di scheduling (OTHER, FIFO, RR)
    int deadperse;             // Numero di deadline perse
    int wcet;                  // Worst Case Execution Time (stima, opzionale)
} parametri;

// Funzioni per la gestione del tempo

// Confronta due istanti temporali (struct timespec)
// Ritorna 1 se t1 > t2, -1 se t1 < t2, 0 se uguali
int confronta_istanti(struct timespec t1, struct timespec t2);

// Copia un istante temporale (struct timespec)
void copia_istante(struct timespec *td, struct timespec ts);

// Aggiunge un certo numero di millisecondi a un istante temporale
void aggiunge_millisecondi(struct timespec *t, int ms);

// Funzioni per la gestione dei task

// Imposta il periodo iniziale e la deadline assoluta di un task
void set_period(parametri *tp);

// Attende fino al prossimo periodo del task (sleep assoluto)
void attende_periodo(parametri *tp);

// Verifica se la deadline è stata mancata e notifica la parte grafica
int deadline_miss(parametri *tp);

// Crea un nuovo thread per il task, impostando la politica di scheduling e la priorità
void crea_task(void *(*miotask)(void *), parametri *par);

#endif // TIME0_H
