# Bouncing Balls Real-Time Task Visualization Library

Una libreria C per la visualizzazione di task real-time attraverso palline che rimbalzano.

## Compilazione

```bash
make                    # Compila libreria ed esempio
make test              # Esegue l'esempio
make install           # Installa la libreria nel sistema
make clean             # Pulisce i file di build
```

## Utilizzo

```c
#include "bouncing_balls.h"

// Inizializza
bouncing_balls_init(800, 600);
bouncing_balls_set_scheduler(FIFO);

// Loop principale
while (running) {
    bouncing_balls_update();
    bouncing_balls_draw();
}

// Cleanup
bouncing_balls_shutdown();
```

## API

- `bouncing_balls_init()` - Inizializza la libreria
- `bouncing_balls_add_task()` - Aggiunge un task
- `bouncing_balls_notify_deadline_miss()` - Notifica deadline miss
- `bouncing_balls_update()` - Aggiorna lo stato
- `bouncing_balls_draw()` - Disegna la scena

## Dipendenze

- Allegro 5
- pthread
- libm