#ifndef COMMON_H
#define COMMON_H

#include <allegro5/allegro.h>  // Usa i mutex di Allegro per la sincronizzazione
#include "time0.h"

// Struttura per mappare task e parametri
typedef struct {
    parametri pinfo;    // Parametri del task (definiti in time0.h)
    void* thread_id;    // Identificatore generico del thread associato al task
                        // (può essere un pthread_t o altro, a seconda della piattaforma)
} task_bundle_t;

// Mutex globale per sincronizzare l'accesso ai dati condivisi tra i task
extern ALLEGRO_MUTEX *task_mutex;

#endif // COMMON_H