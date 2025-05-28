#define _POSIX_C_SOURCE 199309L

// Include delle librerie Allegro e standard
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include "bouncing_balls.h"
#include "time0.h"

// *** DICHIARAZIONI FORWARD ***
// Funzioni di utilità dichiarate in anticipo
void draw_priority_groups(ALLEGRO_FONT *font, int window_w);
void bouncing_balls_draw_wrapped_text(ALLEGRO_FONT *font, ALLEGRO_COLOR color, int x, int y, int max_width, const char *text);
long bouncing_balls_diff_timespec_ms(struct timespec *a, struct timespec *b);

// *** VARIABILI PRIVATE DELLA LIBRERIA ***
extern ALLEGRO_MUTEX *task_mutex; // Mutex globale per sincronizzare accesso ai dati

// Costanti fisiche per il movimento delle palline
#define GRAVITY 0.3f           // Gravità applicata alle palline
#define BOUNCE_ELASTICITY 0.9f // Elasticità del rimbalzo

// Struttura che rappresenta una pallina/task
typedef struct {
    float x, y;                // Posizione
    float vx, vy;              // Velocità
    float radius;              // Raggio
    float mass;                // Massa (non usata direttamente)
    bool active;               // Pallina attiva
    ALLEGRO_COLOR color;       // Colore della pallina
    parametri *task_params;    // Puntatore ai parametri del task associato
    int flash_counter;         // Contatore per lampeggi (deadline miss)
    int dead_flashes;          // Numero di lampeggi da fare per deadline miss
    float flash_intensity;     // Intensità del lampeggio
    bool ready;                // Indica se il task è pronto (a terra)
    bool executing;            // Indica se il task è in esecuzione
    int execution_count;       // Numero di esecuzioni completate
    float periodo_progress;    // Progresso nel periodo attuale (0-1)
} Ball;

// Variabili globali per la gestione delle palline e della finestra
#define MAX_BALLS 100
#define BALL_RADIUS 20

static Ball balls[MAX_BALLS];              // Array delle palline
static int num_balls = 0;                  // Numero di palline attive
static int screen_w = 800, screen_h = 600; // Dimensioni finestra
static ALLEGRO_FONT* font = NULL;          // Font per il testo
static ALLEGRO_DISPLAY *display = NULL;    // Display Allegro
static ALLEGRO_EVENT_QUEUE *event_queue = NULL; // Coda eventi
static ALLEGRO_TIMER *timer = NULL;        // Timer principale
static bool initialized = false;           // Flag di inizializzazione
static int total_deadline_misses = 0;      // Conteggio globale deadline miss

// Variabili per tracciare l'esecuzione dei task
static int currently_executing_task = -1;  // ID del task in esecuzione (-1 = nessuno)
static int total_executions = 0;           // Numero totale di esecuzioni
static int executions_per_task[MAX_BALLS] = {0}; // Esecuzioni per ogni task

// Variabili per overlay dei task eseguiti di recente
#define MAX_EXECUTION_HISTORY 10
static int recent_executions[MAX_EXECUTION_HISTORY]; // ID dei task eseguiti di recente
static int recent_execution_count = 0;               // Quanti task nella storia

// Ottimizzazione aggiornamenti
#define UPDATE_FREQUENCY_DIVIDER 3

// Funzione per generare un colore unico per ogni task
ALLEGRO_COLOR bouncing_balls_get_task_color(int id) {
    float hue = (id * 67) % 360;
    float sat = 0.7f + (id % 3) * 0.1f;
    float val = 0.8f + (id % 2) * 0.2f;
    float c = val * sat;
    float x = c * (1 - fabsf(fmodf(hue / 60.0f, 2) - 1));
    float m = val - c;
    float r, g, b;
    if (hue < 60) { r = c; g = x; b = 0; }
    else if (hue < 120) { r = x; g = c; b = 0; }
    else if (hue < 180) { r = 0; g = c; b = x; }
    else if (hue < 240) { r = 0; g = x; b = c; }
    else if (hue < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    return al_map_rgb((r+m)*255, (g+m)*255, (b+m)*255);
}

// Notifica una deadline miss per un task (fa lampeggiare la pallina)
void bouncing_balls_notify_deadline_miss(int task_id) {
    al_lock_mutex(task_mutex);
    total_deadline_misses++;
    for (int i = 0; i < num_balls; i++) {
        if (balls[i].active && balls[i].task_params && balls[i].task_params->id == task_id) {
            balls[i].dead_flashes = 4; // 4 lampeggi
            balls[i].flash_counter = 0;
            break;
        }
    }
    al_unlock_mutex(task_mutex);
}

// Notifica l'inizio dell'esecuzione di un task (cambia stato e overlay)
void bouncing_balls_notify_execution_start(int task_id) {
    al_lock_mutex(task_mutex);
    currently_executing_task = task_id;
    total_executions++;
    // Aggiorna la lista dei task eseguiti di recente (overlay)
    for (int i = 0; i < recent_execution_count; i++) {
        if (recent_executions[i] == task_id) {
            for (int j = i; j < recent_execution_count - 1; j++)
                recent_executions[j] = recent_executions[j + 1];
            recent_execution_count--;
            break;
        }
    }
    if (recent_execution_count == MAX_EXECUTION_HISTORY) {
        for (int i = MAX_EXECUTION_HISTORY - 1; i > 0; i--)
            recent_executions[i] = recent_executions[i - 1];
    } else {
        for (int i = recent_execution_count; i > 0; i--)
            recent_executions[i] = recent_executions[i - 1];
        recent_execution_count++;
    }
    recent_executions[0] = task_id;
    // Aggiorna stato della pallina
    for (int i = 0; i < num_balls; i++) {
        if (balls[i].active && balls[i].task_params && balls[i].task_params->id == task_id) {
            balls[i].executing = true;
            balls[i].execution_count++;
            executions_per_task[i]++;
            break;
        }
    }
    al_unlock_mutex(task_mutex);
}

// Notifica la fine dell'esecuzione di un task
void bouncing_balls_notify_execution_end(int task_id) {
    al_lock_mutex(task_mutex);
    if (currently_executing_task == task_id)
        currently_executing_task = -1;
    for (int i = 0; i < num_balls; i++) {
        if (balls[i].active && balls[i].task_params && balls[i].task_params->id == task_id) {
            balls[i].executing = false;
            break;
        }
    }
    al_unlock_mutex(task_mutex);
}

// Inizializza la libreria grafica e le risorse Allegro
bool bouncing_balls_init(int w, int h) {
    if (initialized) return true;
    if (!al_init()) return false;
    if (!al_init_primitives_addon()) return false;
    if (!al_install_keyboard()) return false;
    if (!al_init_font_addon()) return false;
    if (!al_init_ttf_addon()) return false;
    al_set_new_display_flags(ALLEGRO_RESIZABLE);
    screen_w = w;
    screen_h = h;
    display = al_create_display(w, h);
    if (!display) return false;
    event_queue = al_create_event_queue();
    if (!event_queue) { al_destroy_display(display); return false; }
    timer = al_create_timer(1.0 / 60.0); // 60 FPS
    if (!timer) { al_destroy_event_queue(event_queue); al_destroy_display(display); return false; }
    al_register_event_source(event_queue, al_get_keyboard_event_source());
    al_register_event_source(event_queue, al_get_timer_event_source(timer));
    al_register_event_source(event_queue, al_get_display_event_source(display));
    font = al_create_builtin_font();
    initialized = true;
    srand(time(NULL));
    return true;
}

// Libera tutte le risorse allocate dalla libreria
void bouncing_balls_shutdown(void) {
    if (font) al_destroy_font(font);
    if (timer) al_destroy_timer(timer);
    if (event_queue) al_destroy_event_queue(event_queue);
    if (display) al_destroy_display(display);
    al_shutdown_font_addon();
    al_shutdown_ttf_addon();
    al_shutdown_primitives_addon();
    initialized = false;
}

// Funzioni di accesso alle risorse Allegro
ALLEGRO_DISPLAY* bouncing_balls_get_display(void) { return display; }
ALLEGRO_EVENT_QUEUE* bouncing_balls_get_event_queue(void) { return event_queue; }
ALLEGRO_TIMER* bouncing_balls_get_timer(void) { return timer; }

// Aggiunge una nuova pallina/task alla simulazione
void bouncing_balls_add_task(parametri *params) {
    if (num_balls >= MAX_BALLS || !params) return;
    al_lock_mutex(task_mutex);
    Ball* b = &balls[num_balls];
    memset(b, 0, sizeof(Ball));
    b->radius = BALL_RADIUS;
    b->mass = 1.0f;
    b->active = true;
    b->color = bouncing_balls_get_task_color(params->id);
    b->task_params = params;
    float ground_level = screen_h * 0.9f;
    float ground_position = ground_level - BALL_RADIUS;
    b->x = b->radius + (rand() % (int)(screen_w - 2 * b->radius));
    b->y = ground_position;
    b->vx = 1.0f;
    float periodo_factor = fminf(params->periodo / 1000.0f, 1.0f);
    float bounce_height = 20.0f + 40.0f * periodo_factor;
    b->vy = -sqrtf(2.0f * GRAVITY * bounce_height);
    b->dead_flashes = 0;
    b->executing = false;
    b->execution_count = 0;
    b->ready = false;
    b->periodo_progress = 0.0f;
    num_balls++;
    al_unlock_mutex(task_mutex);
}

// Aggiorna la simulazione delle palline (movimento, rimbalzi, stato)
void bouncing_balls_update(void) {
    al_lock_mutex(task_mutex);
    float ground_level = screen_h * 0.9f;
    float ground_position = ground_level - BALL_RADIUS;
    float ceiling_position = BALL_RADIUS + 20;
    float available_height = ground_position - ceiling_position;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    for (int i = 0; i < num_balls; i++) {
        Ball* b = &balls[i];
        if (!b->active || !b->task_params) continue;
        b->x += b->vx;
        if (b->x - b->radius < 0) {
            b->x = b->radius;
            b->vx = 1.0f;
        } else if (b->x + b->radius > screen_w) {
            b->x = screen_w - b->radius;
            b->vx = -1.0f;
        }
        if (b->task_params->periodo > 0) {
            long ms_elapsed = bouncing_balls_diff_timespec_ms(&now, &b->task_params->at) % b->task_params->periodo;
            b->periodo_progress = (float)ms_elapsed / b->task_params->periodo;
        }
        // Movimento verticale: diverso se il task è in esecuzione o in attesa
        if (b->executing) {
            b->vy += GRAVITY;
            b->y += b->vy;
            if (b->y >= ground_position) {
                b->y = ground_position;
                float periodo_factor = fminf(b->task_params->periodo / 500.0f, 1.0f);
                float bounce_height = available_height * (0.4f + 0.4f * periodo_factor);
                b->vy = -sqrtf(2.0f * GRAVITY * bounce_height);
                b->ready = true;
            } else {
                b->ready = false;
            }
        } else {
            b->vy += GRAVITY;
            b->y += b->vy;
            if (b->y >= ground_position) {
                b->y = ground_position;
                float max_height_factor = b->task_params->periodo / 1500.0f;
                max_height_factor = fminf(max_height_factor, 0.9f);
                max_height_factor = fmaxf(max_height_factor, 0.3f);
                float bounce_height = available_height * max_height_factor;
                b->vy = -sqrtf(2.0f * GRAVITY * bounce_height);
                b->ready = true;
            } else {
                b->ready = false;
            }
        }
    }
    al_unlock_mutex(task_mutex);
}

// Disegna tutte le palline e le informazioni a schermo
void bouncing_balls_draw(void) {
    al_clear_to_color(al_map_rgb(16, 16, 32));
    al_lock_mutex(task_mutex);
    // Pannello informativo in alto
    char info[200];
    if (currently_executing_task >= 0) {
        snprintf(info, sizeof(info), "TASK ATTIVI: %d | DEADLINE PERSE: %d | IN ESECUZIONE: TASK %d | ESECUZIONI TOT: %d", 
                num_balls, total_deadline_misses, currently_executing_task, total_executions);
    } else {
        snprintf(info, sizeof(info), "TASK ATTIVI: %d | DEADLINE PERSE: %d | IN ESECUZIONE: nessuno | ESECUZIONI TOT: %d", 
                num_balls, total_deadline_misses, total_executions);
    }
    al_draw_text(font, al_map_rgb(255, 255, 100), 10, 10, 0, info);
    draw_priority_groups(font, screen_w); // Mostra gruppi di priorità se necessario
    float ground_level = screen_h * 0.9f;
    al_draw_line(0, ground_level, screen_w, ground_level, al_map_rgb(80, 80, 120), 2.0f);
    int flash_state = (al_get_timer_count(timer) / 30) % 2;
    int draw_order[MAX_BALLS];
    int draw_order_count = 0;
    // Prima disegna i task non recenti, poi quelli recenti (overlay)
    for (int i = 0; i < num_balls; i++) {
        Ball* b = &balls[i];
        if (!b->active || !b->task_params) continue;
        bool in_recent_list = false;
        for (int j = 0; j < recent_execution_count; j++) {
            if (recent_executions[j] == b->task_params->id) {
                in_recent_list = true;
                break;
            }
        }
        if (!in_recent_list) draw_order[draw_order_count++] = i;
    }
    for (int j = recent_execution_count - 1; j >= 0; j--) {
        int task_id = recent_executions[j];
        for (int i = 0; i < num_balls; i++) {
            if (balls[i].active && balls[i].task_params && balls[i].task_params->id == task_id) {
                draw_order[draw_order_count++] = i;
                break;
            }
        }
    }
    // Disegna le palline nell'ordine determinato
    for (int idx = 0; idx < draw_order_count; idx++) {
        int i = draw_order[idx];
        Ball* b = &balls[i];
        int overlay_level = -1;
        for (int j = 0; j < recent_execution_count; j++) {
            if (recent_executions[j] == b->task_params->id) {
                overlay_level = j;
                break;
            }
        }
        float scale_factor = 1.0f;
        float brightness_factor = 1.0f;
        if (overlay_level >= 0) {
            scale_factor = 1.3f - (overlay_level * 0.05f);
            scale_factor = fmaxf(scale_factor, 1.0f);
            brightness_factor = 1.3f - (overlay_level * 0.05f);
            brightness_factor = fmaxf(brightness_factor, 1.0f);
        }
        if (b->executing) scale_factor *= 1.1f;
        ALLEGRO_COLOR ball_color = b->color;
        if (brightness_factor > 1.0f) {
            ball_color.r = fminf(1.0f, ball_color.r * brightness_factor);
            ball_color.g = fminf(1.0f, ball_color.g * brightness_factor);
            ball_color.b = fminf(1.0f, ball_color.b * brightness_factor);
        }
        float radius = b->radius * scale_factor;
        al_draw_filled_circle(b->x, b->y, radius, ball_color);
        // Bordo rosso lampeggiante per deadline miss
        if (b->dead_flashes > 0) {
            if (flash_state == 0)
                al_draw_circle(b->x, b->y, radius, al_map_rgb(255, 0, 0), 3.0f);
            else
                al_draw_circle(b->x, b->y, radius, al_map_rgb(200, 200, 200), 1.0f);
            if ((al_get_timer_count(timer) % 30) == 0)
                b->dead_flashes--;
        } else {
            float border_width = overlay_level == 0 ? 2.0f : 1.0f;
            al_draw_circle(b->x, b->y, radius, al_map_rgb(200, 200, 200), border_width);
        }
        // Disegna l'ID del task sulla pallina
        char id_str[16];
        snprintf(id_str, sizeof(id_str), "%d", b->task_params->id);
        al_draw_text(font, al_map_rgb(0, 0, 0), b->x, b->y - 5, ALLEGRO_ALIGN_CENTRE, id_str);
    }
    al_unlock_mutex(task_mutex);
    al_flip_display();
}

// Gestisce il ridimensionamento della finestra e delle palline
void bouncing_balls_resize(int new_w, int new_h) {
    al_lock_mutex(task_mutex);
    float scale_x = (float)new_w / screen_w;
    float scale_y = (float)new_h / screen_h;
    for (int i = 0; i < num_balls; i++) {
        balls[i].x *= scale_x;
        balls[i].y *= scale_y;
        balls[i].x = fminf(fmaxf(balls[i].x, balls[i].radius), new_w - balls[i].radius);
        balls[i].y = fminf(fmaxf(balls[i].y, balls[i].radius), new_h - balls[i].radius);
    }
    screen_w = new_w;
    screen_h = new_h;
    al_unlock_mutex(task_mutex);
}

// Calcola la differenza in millisecondi tra due struct timespec
long bouncing_balls_diff_timespec_ms(struct timespec *a, struct timespec *b) {
    return (a->tv_sec - b->tv_sec) * 1000 + (a->tv_nsec - b->tv_nsec) / 1000000;
}

// Variabile per la politica di scheduling corrente
static schedulazione current_scheduler = OTHER;

// Disegna i gruppi di task con la stessa priorità (solo se non SCHED_OTHER e DEADLINE)
void draw_priority_groups(ALLEGRO_FONT *font, int window_w) {
    if (current_scheduler == OTHER || current_scheduler == DEADLINE) return;
    char buf[1024] = "";
    bool found = false;
    bool processed[MAX_BALLS] = {false};
    for (int i = 0; i < num_balls; i++) {
        if (!balls[i].active || !balls[i].task_params || processed[i]) continue;
        int group_size = 1;
        int group_priority = balls[i].task_params->priorita;
        char group[256] = "";
        snprintf(group, sizeof(group), "P:%d [%d", group_priority, balls[i].task_params->id);
        processed[i] = true;
        for (int j = i + 1; j < num_balls; j++) {
            if (balls[j].active && balls[j].task_params && !processed[j] && balls[j].task_params->priorita == group_priority) {
                char tmp[16];
                snprintf(tmp, sizeof(tmp), ",%d", balls[j].task_params->id);
                strncat(group, tmp, sizeof(group) - strlen(group) - 1);
                processed[j] = true;
                group_size++;
            }
        }
        if (group_size > 1) {
            strncat(group, "] ", sizeof(group) - strlen(group) - 1);
            strncat(buf, group, sizeof(buf) - strlen(buf) - 1);
            found = true;
        }
    }
    if (found) {
        int line_height = al_get_font_line_height(font);
        int start_y = 10 + line_height + 10;
        int effective_width = al_get_display_width(al_get_current_display());
        int max_text_width = effective_width - 40;
        bouncing_balls_draw_wrapped_text(font, al_map_rgb(255, 255, 0), 10, start_y, max_text_width, buf);
    }
}

// Funzione per disegnare testo con wrapping automatico
void bouncing_balls_draw_wrapped_text(ALLEGRO_FONT *font, ALLEGRO_COLOR color, int x, int y, int max_width, const char *text) {
    if (!font || !text) return;
    int line_height = al_get_font_line_height(font);
    int current_y = y;
    int current_x = x;
    int line_spacing = line_height + 5;
    char line_buffer[512] = "";
    int line_len = 0;
    char text_copy[1024];
    strncpy(text_copy, text, sizeof(text_copy) - 1);
    text_copy[sizeof(text_copy) - 1] = '\0';
    char *saveptr;
    char *word = strtok_r(text_copy, " ", &saveptr);
    while (word != NULL) {
        char test_line[600];
        if (line_len == 0) {
            snprintf(test_line, sizeof(test_line), "%s", word);
        } else {
            int needed_len = strlen(line_buffer) + strlen(word) + 2;
            if (needed_len < (int)sizeof(test_line)) {
                snprintf(test_line, sizeof(test_line), "%s %s", line_buffer, word);
            } else {
                al_draw_text(font, color, current_x, current_y, 0, line_buffer);
                current_y += line_spacing;
                snprintf(test_line, sizeof(test_line), "%s", word);
                strcpy(line_buffer, "");
                line_len = 0;
            }
        }
        int text_width = al_get_text_width(font, test_line);
        if (text_width > max_width && line_len > 0) {
            al_draw_text(font, color, current_x, current_y, 0, line_buffer);
            current_y += line_spacing;
            int display_height = al_get_display_height(al_get_current_display());
            if (current_y + line_height > display_height - 50) break;
            strncpy(line_buffer, word, sizeof(line_buffer) - 1);
            line_buffer[sizeof(line_buffer) - 1] = '\0';
            line_len = strlen(line_buffer);
        } else {
            strncpy(line_buffer, test_line, sizeof(line_buffer) - 1);
            line_buffer[sizeof(line_buffer) - 1] = '\0';
            line_len = strlen(line_buffer);
        }
        word = strtok_r(NULL, " ", &saveptr);
    }
    if (line_len > 0) {
        int display_height = al_get_display_height(al_get_current_display());
        if (current_y + line_height <= display_height - 50) {
            al_draw_text(font, color, current_x, current_y, 0, line_buffer);
        }
    }
}

// Imposta il titolo della finestra
void bouncing_balls_set_title(const char *title) {
    ALLEGRO_DISPLAY *display = bouncing_balls_get_display();
    if (display && title) {
        al_set_window_title(display, title);
    }
}

// Restituisce la versione della libreria
const char* bouncing_balls_get_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d", 
             BOUNCING_BALLS_VERSION_MAJOR,
             BOUNCING_BALLS_VERSION_MINOR, 
             BOUNCING_BALLS_VERSION_PATCH);
    return version;
}

// Imposta la politica di scheduling corrente (per la visualizzazione)
void bouncing_balls_set_scheduler(schedulazione sched) {
    current_scheduler = sched;
}

