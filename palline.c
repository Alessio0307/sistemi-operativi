#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <stdlib.h>
#include <time.h>

#define BALL_RADIUS 20  // Raggio di base delle palline
#define MAX_BALLS 100

int screen_w = 800;
int screen_h = 600;
int ball_radius = BALL_RADIUS;  // Raggio delle palline

typedef struct {
    float x, y;
    float dx, dy;
    ALLEGRO_COLOR color;
} Ball;

Ball balls[MAX_BALLS];
int ball_count = 0;

void add_ball() {
    if (ball_count >= MAX_BALLS) return;

    Ball b;
    b.x = rand() % (screen_w - 2 * ball_radius) + ball_radius;
    b.y = rand() % (screen_h - 2 * ball_radius) + ball_radius;
    b.dx = (rand() % 5 + 2) * (rand() % 2 == 0 ? 1 : -1);
    b.dy = (rand() % 5 + 2) * (rand() % 2 == 0 ? 1 : -1);
    b.color = al_map_rgb(rand() % 256, rand() % 256, rand() % 256);

    balls[ball_count++] = b;
}

void update_ball(Ball* b) {
    b->x += b->dx;
    b->y += b->dy;

    if (b->x <= ball_radius || b->x >= screen_w - ball_radius) b->dx = -b->dx;
    if (b->y <= ball_radius || b->y >= screen_h - ball_radius) b->dy = -b->dy;
}

void draw_ball(Ball* b) {
    al_draw_filled_circle(b->x, b->y, ball_radius, b->color);
}

// Funzione per riposizionare le palline all'interno della finestra
void adjust_balls_to_screen() {
    for (int i = 0; i < ball_count; i++) {
        // Controlla e corregge la posizione x e la direzione
        if (balls[i].x < ball_radius) {
            balls[i].x = ball_radius;
            // Se la pallina si muove verso sinistra, inverti la direzione
            if (balls[i].dx < 0) {
                balls[i].dx = -balls[i].dx;
            }
        } else if (balls[i].x > screen_w - ball_radius) {
            balls[i].x = screen_w - ball_radius;
            // Se la pallina si muove verso destra, inverti la direzione
            if (balls[i].dx > 0) {
                balls[i].dx = -balls[i].dx;
            }
        }
        
        // Controlla e corregge la posizione y e la direzione
        if (balls[i].y < ball_radius) {
            balls[i].y = ball_radius;
            // Se la pallina si muove verso l'alto, inverti la direzione
            if (balls[i].dy < 0) {
                balls[i].dy = -balls[i].dy;
            }
        } else if (balls[i].y > screen_h - ball_radius) {
            balls[i].y = screen_h - ball_radius;
            // Se la pallina si muove verso il basso, inverti la direzione
            if (balls[i].dy > 0) {
                balls[i].dy = -balls[i].dy;
            }
        }
    }
}

int main() {
    srand(time(NULL));

    al_init();
    al_install_keyboard();
    al_init_primitives_addon();
    al_init_font_addon();
    al_init_ttf_addon();

    // Configurazione specifica per migliorare la stabilità su Mac
    al_set_new_display_option(ALLEGRO_SAMPLE_BUFFERS, 1, ALLEGRO_SUGGEST);
    al_set_new_display_option(ALLEGRO_SAMPLES, 4, ALLEGRO_SUGGEST);
    
    // Disabilita vsync per ridurre i problemi durante il ridimensionamento
    al_set_new_display_option(ALLEGRO_VSYNC, 0, ALLEGRO_SUGGEST);
    
    // Riduci la profondità di colore per migliorare le prestazioni
    al_set_new_display_option(ALLEGRO_COLOR_SIZE, 24, ALLEGRO_SUGGEST);
    
    // Manteniamo la finestra ridimensionabile ma con maggiori precauzioni
    al_set_new_display_flags(ALLEGRO_WINDOWED | ALLEGRO_RESIZABLE);
    ALLEGRO_DISPLAY* display = al_create_display(screen_w, screen_h);

    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
    ALLEGRO_TIMER* timer = al_create_timer(1.0 / 60);

    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_display_event_source(display));
    al_register_event_source(queue, al_get_timer_event_source(timer));

    bool running = true;
    bool redraw = true;
    bool is_resizing = false;
    double resize_cooldown = 0;
    int resize_buffer_w = screen_w;
    int resize_buffer_h = screen_h;
    
    add_ball();  // Prima pallina
    al_start_timer(timer);

    while (running) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(queue, &ev);

        if (ev.type == ALLEGRO_EVENT_TIMER) {
            // Aggiornamento delle palline solo se non stiamo ridimensionando
            if (!is_resizing) {
                for (int i = 0; i < ball_count; i++)
                    update_ball(&balls[i]);
            }
            
            // Gestione del periodo di cooldown dopo il ridimensionamento
            if (resize_cooldown > 0) {
                resize_cooldown -= 1.0 / 60.0;
                if (resize_cooldown <= 0) {
                    // Applicazione effettiva dei cambiamenti dopo il cooldown
                    screen_w = resize_buffer_w;
                    screen_h = resize_buffer_h;
                    ball_radius = BALL_RADIUS * (screen_w / 800.0);
                    adjust_balls_to_screen();
                    is_resizing = false;
                }
            }
            
            redraw = true;
        }
        else if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
            if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE)
                running = false;
            else if (ev.keyboard.keycode == ALLEGRO_KEY_SPACE)
                add_ball();
            else if (ev.keyboard.keycode == ALLEGRO_KEY_ENTER) {
                // Cambia tra finestra e schermo intero
                int flags = al_get_display_flags(display);
                
                // Blocchiamo temporaneamente l'aggiornamento delle palline durante la transizione
                is_resizing = true;
                
                if (flags & ALLEGRO_FULLSCREEN_WINDOW) {
                    // Torna alla finestra normale
                    al_set_display_flag(display, ALLEGRO_FULLSCREEN_WINDOW, false);
                    
                    // Applica un breve ritardo prima del ridimensionamento
                    al_rest(0.1);
                    
                    // Ripristina le dimensioni originali
                    al_resize_display(display, 800, 600);
                    resize_buffer_w = 800;
                    resize_buffer_h = 600;
                    resize_cooldown = 0.3; // Aspetta 0.3 secondi prima di applicare i cambiamenti
                } else {
                    // Passa a schermo intero
                    al_set_display_flag(display, ALLEGRO_FULLSCREEN_WINDOW, true);
                    
                    // Applica un breve ritardo prima di ottenere le dimensioni
                    al_rest(0.1);
                    
                    // Ottieni le nuove dimensioni dello schermo
                    resize_buffer_w = al_get_display_width(display);
                    resize_buffer_h = al_get_display_height(display);
                    resize_cooldown = 0.3; // Aspetta 0.3 secondi prima di applicare i cambiamenti
                }
            }
        }
        else if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            running = false;
        }
        else if (ev.type == ALLEGRO_EVENT_DISPLAY_RESIZE) {
            // Ridimensionamento in corso, blocca temporaneamente l'aggiornamento delle palline
            is_resizing = true;
            
            // Acknowledgiamo il ridimensionamento per evitare che la finestra si blocchi
            al_acknowledge_resize(display);
            
            // Non aggiorniamo subito screen_w e screen_h ma usiamo un buffer temporaneo
            resize_buffer_w = al_get_display_width(display);
            resize_buffer_h = al_get_display_height(display);
            
            // Impostiamo un periodo di cooldown dopo il quale applicare i cambiamenti
            resize_cooldown = 0.2; // 200ms di attesa
        }
        else if (ev.type == ALLEGRO_EVENT_DISPLAY_HALT_DRAWING) {
            // Gestiamo questo evento per migliorare la stabilità durante il ridimensionamento
            is_resizing = true;
            al_acknowledge_drawing_halt(display);
        }
        else if (ev.type == ALLEGRO_EVENT_DISPLAY_RESUME_DRAWING) {
            // Riprendiamo il disegno dopo l'interruzione
            al_acknowledge_drawing_resume(display);
        }

        if (redraw && al_is_event_queue_empty(queue)) {
            // Disegniamo solo se non stiamo attivamente ridimensionando
            if (!is_resizing || resize_cooldown <= 0) {
                al_clear_to_color(al_map_rgb(0, 0, 0));
                for (int i = 0; i < ball_count; i++)
                    draw_ball(&balls[i]);
                al_flip_display();
            }
            redraw = false;
        }
    }

    al_destroy_timer(timer);
    al_destroy_event_queue(queue);
    al_destroy_display(display);
    return 0;
}