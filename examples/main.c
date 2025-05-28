#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <math.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>
long int syscall(long int number, ...);

// *** USA LA LIBRERIA CON I NUOVI NOMI ***
#include "bouncing_balls.h"
#include "time0.h"

#define MAX 100

ALLEGRO_MUTEX *task_mutex = NULL; // Mutex globale per sincronizzare accesso ai dati dei task
parametri par[MAX];               // Array dei parametri dei task

// Definizione struct sched_attr
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

static int sched_setattr(pid_t pid, struct sched_attr *attr, unsigned int flags) {
    return syscall(__NR_sched_setattr, pid, attr, flags);
}

// Conta quanti task sono attivi
int task_count()
{
    int count = 0;
    for (int i = 1; i < MAX; i++)
    {
        if (par[i].id > 0)
            count++;
    }
    return count;
}

// Crea un nuovo task periodico e lo aggiunge alla visualizzazione
void crea_periodico(void *(*miotask)(void *), schedulazione cl_sched, int indice, int per, int dedrel, int prio)
{
    al_lock_mutex(task_mutex); // Protegge l'accesso ai parametri del task
    par[indice].id = indice;
    par[indice].periodo = per;
    par[indice].deadline = dedrel;
    par[indice].priorita = prio;
    par[indice].sched = cl_sched;
    par[indice].deadperse = 0;
    par[indice].wcet = per / 3;
    al_unlock_mutex(task_mutex);

    crea_task(miotask, &par[indice]);                // Crea il thread del task
    bouncing_balls_add_task(&par[indice]);           // Aggiunge il task alla visualizzazione

    printf("Task %d creato - P:%d ms, D:%d ms, Prio:%d\n",
           indice, per, dedrel, prio);
}

// Funzione eseguita da ogni task periodico
void *periodico(void *arg)
{
    parametri *argp = (parametri *)arg;
    int i = argp->id;

    // Imposta SCHED_DEADLINE se richiesto
    if (argp->sched == DEADLINE) {
        struct sched_attr attr = {0};
        attr.size = sizeof(attr);
        attr.sched_policy = SCHED_DEADLINE;
        // Conversione ms -> ns
        attr.sched_runtime  = 10*1000*1000;//(argp->periodo / 3) * 1000 * 1000;   // esempio: 1/3 del periodo
        attr.sched_deadline = 30*1000*1000;//argp->deadline * 1000 * 1000;
        attr.sched_period   = 30*1000*1000;//argp->periodo * 1000 * 1000;
        if (sched_setattr(0, &attr, 0) < 0) {
            perror("sched_setattr (DEADLINE)");
        }
    }

    set_period(argp); // Imposta il periodo iniziale

    printf("Task %d avviato con scheduler %d, priorità %d\n",
           i, argp->sched, argp->priorita);

    while (1)
    {
        bouncing_balls_notify_execution_start(i); // Notifica inizio esecuzione (colore pallina)

        // --- Simulazione carico di lavoro (commentato) ---
        // int lavoro = argp->periodo / 3;
        // volatile double result = 0.0;
        // for (int j = 0; j < lavoro * 10000; j++) {
        //     result += sin(j) * cos(j);
        // }

        bouncing_balls_notify_execution_end(i); // Notifica fine esecuzione

        deadline_miss(argp);    // Verifica se la deadline è stata mancata
        attende_periodo(argp);  // Attende il prossimo periodo
    }
}

// Permette all'utente di scegliere la politica di scheduling
schedulazione scegli_sched()
{
    int scelta;
    printf("Scegli scheduling per tutti i task:\n");
    printf("1 - FIFO (richiede root)\n");
    printf("2 - RR (richiede root)\n");
    printf("3 - OTHER (consigliato per test)\n");
    printf("4 - DEADLINE\n");
    printf("Scelta [4]: ");
    scanf("%d", &scelta);
    switch (scelta)
    {
    case 1:
        return FIFO;
    case 2:
        return RR;
    case 3:
        return OTHER;
    case 4:
        return DEADLINE;    
    default:
        return OTHER;
    }
}

int main() {
    int i = 1; // Indice per i nuovi task

    printf("Bouncing Balls Library v%s\n", bouncing_balls_get_version());
    
    if (!al_init()) { // Inizializza Allegro
        fprintf(stderr, "Errore inizializzazione Allegro\n");
        return 1;
    }
    
    task_mutex = al_create_mutex(); // Crea mutex globale
    if (!task_mutex) {
        fprintf(stderr, "Errore creazione mutex\n");
        return 1;
    }

    schedulazione sched = scegli_sched(); // Scegli scheduling
    
    if (!bouncing_balls_init(800, 600)) { // Inizializza la libreria grafica
        fprintf(stderr, "Errore inizializzazione libreria\n");
        al_destroy_mutex(task_mutex);
        return 1;
    }

    // Imposta titolo della finestra in base allo scheduling scelto
    const char *title = sched == FIFO ? "Pallina - FIFO" : 
                       sched == RR ? "Pallina - RR" : 
                       sched == DEADLINE ? "Pallina - DL" : "Pallina - OTHER";

    bouncing_balls_set_title(title);
    bouncing_balls_set_scheduler(sched);

    // Ottiene riferimenti alle risorse Allegro
    ALLEGRO_EVENT_QUEUE *queue = bouncing_balls_get_event_queue();
    ALLEGRO_TIMER *timer = bouncing_balls_get_timer();
    ALLEGRO_DISPLAY *display = bouncing_balls_get_display();

    al_start_timer(timer); // Avvia il timer principale

    printf("==== Task Periodici con Visualizzazione ====\n");
    printf("SPAZIO = aggiungi task (max %d)\n", MAX - 1);
    printf("D = aggiungi task con deadline ridotta (per forzare miss)\n");
    printf("ESC = uscita\n");

    bool running = true;
    bool redraw = true;
    while (running)
    {
        ALLEGRO_EVENT ev;
        al_wait_for_event(queue, &ev); // Attende un evento

        if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
        {
            running = false; // Chiudi finestra
        }
        else if (ev.type == ALLEGRO_EVENT_KEY_DOWN)
        {
            if (ev.keyboard.keycode == ALLEGRO_KEY_SPACE && i < MAX)
            {
                // Aggiungi un nuovo task con periodo e deadline uguali
                printf("Creo task %d con P=D=%dms...\n", i, 100 * i);
                crea_periodico(periodico, sched, i, 100 * i, 100 * i, 30 - i);
                i++;
                redraw = true;
            }
            else if (ev.keyboard.keycode == ALLEGRO_KEY_D && i < MAX)
            {
                // Aggiungi un task con deadline più stretta (più facile mancare la deadline)
                int periodo = 100 * i;
                int deadline = periodo / 4; // Solo 25% del periodo
                printf("Task %d con deadline stretta: P=%d, D=%d\n", i, periodo, deadline);
                crea_periodico(periodico, sched, i, periodo, deadline, 30 - i);
                i++;
                redraw = true;
            }
            if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE)
            {
                running = false; // Esci dal programma
            }
        }
        else if (ev.type == ALLEGRO_EVENT_DISPLAY_RESIZE) {
            // Gestisce il ridimensionamento della finestra
            int new_w = ev.display.width;
            int new_h = ev.display.height;
            al_acknowledge_resize(display);
            bouncing_balls_resize(new_w, new_h);
            redraw = true;
        }
        else if (ev.type == ALLEGRO_EVENT_TIMER) {
            // Aggiorna la simulazione ad ogni tick del timer
            bouncing_balls_update();
            redraw = true;
        }

        // Ridisegna la scena solo se necessario e la coda eventi è vuota
        if (redraw && al_is_event_queue_empty(queue)) {
            bouncing_balls_draw();
            redraw = false;
        }
    }

    bouncing_balls_shutdown(); // Libera risorse della libreria
    al_destroy_mutex(task_mutex); // Libera il mutex
    return 0;
}