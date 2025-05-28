#ifndef BOUNCING_BALLS_H
#define BOUNCING_BALLS_H

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <stdbool.h>
#include "time0.h"

// *** LIBRERIA BOUNCING BALLS - API PUBBLICA ***

// Versione della libreria (major.minor.patch)
#define BOUNCING_BALLS_VERSION_MAJOR 1
#define BOUNCING_BALLS_VERSION_MINOR 0
#define BOUNCING_BALLS_VERSION_PATCH 0

// *** INIZIALIZZAZIONE E CLEANUP ***
// Inizializza la libreria grafica e le strutture dati interne
// screen_w, screen_h: dimensioni iniziali della finestra
bool bouncing_balls_init(int screen_w, int screen_h);

// Libera tutte le risorse allocate dalla libreria
void bouncing_balls_shutdown(void);

// *** GESTIONE TASK/PALLINE ***
// Aggiunge un nuovo task/pallina alla simulazione
void bouncing_balls_add_task(parametri *params);

// Aggiorna la simulazione (posizione palline, stato, ecc.)
void bouncing_balls_update(void);

// Ridisegna la scena grafica (tutte le palline e overlay)
void bouncing_balls_draw(void);

// *** GESTIONE EVENTI REAL-TIME ***
// Notifica una deadline mancata per il task indicato (effetto visivo)
void bouncing_balls_notify_deadline_miss(int task_id);

// Notifica l'inizio dell'esecuzione di un task (cambia stato/colore)
void bouncing_balls_notify_execution_start(int task_id);

// Notifica la fine dell'esecuzione di un task
void bouncing_balls_notify_execution_end(int task_id);

// *** CONFIGURAZIONE SCHEDULER ***
// Imposta la politica di scheduling visualizzata (solo per overlay)
void bouncing_balls_set_scheduler(schedulazione sched);

// *** GESTIONE FINESTRA ***
// Gestisce il ridimensionamento della finestra grafica
void bouncing_balls_resize(int new_w, int new_h);

// Imposta il titolo della finestra
void bouncing_balls_set_title(const char *title);

// *** ACCESSO RISORSE ALLEGRO ***
// Restituisce il puntatore al display Allegro
ALLEGRO_DISPLAY* bouncing_balls_get_display(void);

// Restituisce la coda eventi Allegro
ALLEGRO_EVENT_QUEUE* bouncing_balls_get_event_queue(void);

// Restituisce il timer principale Allegro
ALLEGRO_TIMER* bouncing_balls_get_timer(void);

// *** UTILITÀ ***
// Restituisce un colore unico per ogni task (in base all'id)
ALLEGRO_COLOR bouncing_balls_get_task_color(int task_id);

// Restituisce la versione della libreria come stringa
const char* bouncing_balls_get_version(void);

// *** COMPATIBILITÀ (deprecate - usa le nuove API) ***
// Macro per mantenere compatibilità con vecchi nomi delle funzioni
#ifndef BOUNCING_BALLS_NO_LEGACY
#define add_ball bouncing_balls_add_task
#define update_balls bouncing_balls_update
#define draw_balls bouncing_balls_draw
#define notify_deadline_miss bouncing_balls_notify_deadline_miss
#define notify_task_execution_start bouncing_balls_notify_execution_start
#define notify_task_execution_end bouncing_balls_notify_execution_end
#define set_current_scheduler bouncing_balls_set_scheduler
#define resize_balls_screen bouncing_balls_resize
#define get_display bouncing_balls_get_display
#define get_event_queue bouncing_balls_get_event_queue
#define get_timer bouncing_balls_get_timer
#define get_id_color bouncing_balls_get_task_color
#endif

#endif // BOUNCING_BALLS_H