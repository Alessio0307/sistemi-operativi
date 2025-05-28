#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <allegro5/allegro.h>
#include "time0.h"
#include "bouncing_balls.h"
#include <unistd.h>         
#include <stdint.h>
#include <sys/syscall.h>    
long int syscall(long int number, ...);

// --- AGGIUNGI QUI la struct e la funzione, SOLO UNA VOLTA ---
struct sched_attr {
    uint32_t size;
    uint32_t sched_policy;
    uint64_t sched_flags;
    int32_t  sched_nice;
    uint32_t sched_priority;
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;
};

#ifndef __NR_sched_setattr
#define __NR_sched_setattr 314
#endif

#ifndef SCHED_DEADLINE
#define SCHED_DEADLINE 6
#endif

// --- FINE AGGIUNTA ---

#define handle_error_en(en, msg) \
    do                           \
    {                            \
        errno = en;              \
        perror(msg);             \
        exit(EXIT_FAILURE);      \
    } while (0)

// Mutex globale dichiarato esternamente, usato per sincronizzare l'accesso ai dati dei task
extern ALLEGRO_MUTEX *task_mutex;

// Funzione di utilità per ottenere il tempo corrente in secondi (solo per debug/log)
static double get_time_seconds()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1.0e9;
}

// Confronta due istanti temporali (struct timespec)
// Ritorna 1 se t1 > t2, -1 se t1 < t2, 0 se uguali
int confronta_istanti(struct timespec t1, struct timespec t2)
{
    if (t1.tv_sec > t2.tv_sec)
        return 1;
    if (t1.tv_sec < t2.tv_sec)
        return -1;
    if (t1.tv_nsec > t2.tv_nsec)
        return 1;
    if (t1.tv_nsec < t2.tv_nsec)
        return -1;
    return 0;
}

// Copia un istante temporale (struct timespec)
void copia_istante(struct timespec *td, struct timespec ts)
{
    td->tv_sec = ts.tv_sec;
    td->tv_nsec = ts.tv_nsec;
}

// Aggiunge un certo numero di millisecondi a un istante temporale
void aggiunge_millisecondi(struct timespec *t, int ms)
{
    t->tv_sec += ms / 1000;
    t->tv_nsec += (ms % 1000) * 1000000;
    if (t->tv_nsec >= 1000000000)
    {
        t->tv_nsec -= 1000000000;
        t->tv_sec += 1;
    }
}

// Imposta il periodo iniziale e la deadline assoluta di un task
void set_period(parametri *tp)
{
    al_lock_mutex(task_mutex);  // Protegge l'accesso ai dati del task
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    copia_istante(&(tp->at), t); // Prossima attivazione
    copia_istante(&(tp->dl), t); // Prossima deadline
    aggiunge_millisecondi(&(tp->at), tp->periodo);
    aggiunge_millisecondi(&(tp->dl), tp->deadline);
    al_unlock_mutex(task_mutex);
}

// Attende fino al prossimo periodo del task (sleep assoluto)
void attende_periodo(parametri *tp)
{
    // Lettura protetta della prossima attivazione
    al_lock_mutex(task_mutex);
    struct timespec at_copy = tp->at;
    al_unlock_mutex(task_mutex);

    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &at_copy, NULL);

    // Aggiorna at e dl per il prossimo ciclo (scrittura protetta)
    al_lock_mutex(task_mutex);
    aggiunge_millisecondi(&(tp->at), tp->periodo);
    aggiunge_millisecondi(&(tp->dl), tp->deadline);
    al_unlock_mutex(task_mutex);
}

// Verifica se la deadline è stata mancata e notifica la parte grafica
int deadline_miss(parametri *tp)
{
    struct timespec adesso;
    clock_gettime(CLOCK_MONOTONIC, &adesso);

    al_lock_mutex(task_mutex);
    int miss = confronta_istanti(adesso, tp->dl) > 0;
    if (miss)
    {
        tp->deadperse++; // Incrementa il contatore di deadline perse
        al_unlock_mutex(task_mutex);

        // Notifica la parte grafica della deadline persa
        bouncing_balls_notify_deadline_miss(tp->id);

        printf("Task %d: Deadline persa! (totale: %d) a %.3f sec\n",
               tp->id, tp->deadperse, get_time_seconds());
        return 1;
    }
    al_unlock_mutex(task_mutex);
    return 0;
}

// Crea un nuovo thread per il task, impostando la politica di scheduling e la priorità
void crea_task(void *(*miotask)(void *), parametri *par)
{
    pthread_t tid;
    pthread_attr_t attribute;
    struct sched_param param;
    int tret;

    pthread_attr_init(&attribute);
    pthread_attr_setinheritsched(&attribute, PTHREAD_EXPLICIT_SCHED);

    int policy;

    switch (par->sched)
    {
    case FIFO:
        policy = SCHED_FIFO;
        pthread_attr_setschedpolicy(&attribute, SCHED_FIFO);
        break;
    case RR:
        policy = SCHED_RR;
        pthread_attr_setschedpolicy(&attribute, SCHED_RR);
        break;
    case DEADLINE:
        // Per DEADLINE, lascia SCHED_OTHER e imposta DEADLINE nel thread
        policy = DEADLINE;
        break;
    default:
        policy = SCHED_OTHER;
        pthread_attr_setschedpolicy(&attribute, SCHED_OTHER);
    }

    int min_prio = sched_get_priority_min(policy);
    int max_prio = sched_get_priority_max(policy);

    if (policy == SCHED_OTHER)
        par->priorita = 0;
    else
        par->priorita = min_prio + rand() % (max_prio - min_prio + 1);

    param.sched_priority = par->priorita;
    pthread_attr_setschedparam(&attribute, &param);

    printf("Chiamo pthread_create per task id=%d (policy=%d, prio=%d)\n",
           par->id, par->sched, param.sched_priority);

    tret = pthread_create(&tid, &attribute, miotask, (void *)par);
    if (tret)
        handle_error_en(tret, "pthread_create");
}
