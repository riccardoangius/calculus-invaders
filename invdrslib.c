/*
	invdrslib.c
	Calculus Invaders, esercizio 2
	Scritto da Riccardo Angius
	Ultima modifica: 14 gennaio 2012
*/
#include "invdrslib.h"

// Aggiunge un carattere alla porzione di schermo riservata agli oggetti volanti
void board_mvaddch(int y, int x, char c) {	
	mvaddch(y + GAME_STATUS_ROWS, x, c);
}

// Inizializza un oggetto con i valori forniti
void obj_init(object * item, object_class class, object_id id, int start_y, int start_x, int default_height, int default_width, int default_health, char * default_pic, object_status status) {
	item->id = id;
	item->class = class;
	item->tid = pthread_self();
	item->prev_y = NOWHERE;
	item->prev_x = NOWHERE;
	item->y = start_y;
	item->x = start_x;
	item->height = default_height;
	item->width = default_width;
	item->hp = default_health;
	sem_init (&item->obj_mutex, 0, 0);

	item->status = status;
	item->revision = 0;
	item->pic = default_pic;
}

// Aggiorna le coordinate di un oggetto
void obj_new_coords(object * item, int y, int x) {
	item->prev_y = item->y;
	item->prev_x = item->x;
	item->y = y;
	item->x = x;
}

// Verifica se una posizione sul tabellone è nulla
_Bool obj_is_null(object a) {	
	return (a.class == null_c);
}

// Pulisce una posizione sul tabellone
void obj_reset(object * info) {
	info->id = NULL_ID;
	info->class = null_c;
	info->tid = 0;
	info->prev_y = NOWHERE;
	info->prev_x = NOWHERE;
	info->y = NOWHERE;
	info->x = NOWHERE;
	info->height = 0;
	info->width = 0;
	info->hp = 0;
	info->status = dead;
	info->revision = -1;
	info->pic = NULL;
}

// Stampa un file
void print_file(char * filename) {
	
	FILE * handle = fopen(filename, "r");
	
	if (!handle) {
		printf("Could not open file for reading.\nMake sure \"%s\" is in the cwd.", filename);
		exit(1);
	}
	int i;
	char buf[MAXX];
	
	// Legge e stampa a video ogni riga del file
	for (i = 0; !feof(handle); i++) {
		fgets(buf, MAXX, handle);
		mvprintw(i, 0, buf);
	}
	
	fclose(handle);
	refresh();
	
}

// Schermata di avvio
void welcome_screen() {
	
	print_file(WELCOME_FILE);
	
	getch();
	
	clear();
	
}

// Schermata di game_over
void game_over_screen(int game_points, _Bool win) {
	clear();
	
	print_file(win ? WINNER_FILE : LOSER_FILE);

	printw("%d points.", game_points);
	refresh();
	
	// Attesa per evitare uscita dovuta a tasti premuti accidentalmente
	sleep(2);
	getch();
	return;	
	
}

// Aggiorna il punteggio e lo stampa a video
void update_points(int * game_points, int offset) {
	
	beep(); // Suono per segnalare la modifica al punteggio
	
	*game_points += offset; 

	// Converte il punteggio in stringa
	char buf[11];
	sprintf(buf,"%d",*game_points);
	
	// Scrive nella prima riga, nell'angolo destro
	mvprintw(0, MAXX-1-strlen(buf), buf);
	
}

// Copia un oggetto (utile per riempire una posizione sul tabellone)
void obj_copy(object * dest, object source) {
	*dest = source;
}

// Controlla se due strutture sono allo stesso oggetto
_Bool obj_equal(object a, object b) {
	return ((a.class == b.class) && (a.id == b.id));
}

// Routine per i thread produttori affinchè accodino il puntatore
// alla propria struttura e attendano che siano elaborate dal consumatore
void queue_obj(object * item, std_args * shared) {
	
	int * in = shared->in;
	
	// Attesa al semaforo "disponibili" con controllo periodico di eventuale game over
	while (sem_trywait(shared->available) == -1) {
		if (*shared->game_over)
			return;
		usleep(500000);
	}
	
	// Attesa al mutex del buffer
	sem_wait(shared->mutex);
	
		shared->buffer[*in] = item;		
		*in = (*in +1)%(shared->buffer_size);

	// Rilascio mutex del buffer
	sem_post(shared->mutex);
	
	// Segnala nuovo elemento presente
	sem_post(shared->present);

	// Attesa al proprio semaforo con controllo periodico di eventuale game over
	while (sem_trywait(&item->obj_mutex) == -1) {
		if (*shared->game_over)
			return;
		usleep(500);
	}
	
}

// Segnala il semaforo di un oggetto affinchè il produttore proprietario
// possa riprendere l'esecuzione
void release_obj(object * item) {	
	sem_post(&item->obj_mutex);
}

// Toglie un punto salute ad una navicella nemica
void fiend_damage(object * fiend) {
	fiend->hp--;
}

// Controlla lo stato di salute della navicella, eventualmente decretandone
// la morte e generando un segnale per navicella di livello superiore
void fiend_health_check(object * fiend) {

	// Controlla stato salute
	if (fiend->hp <= 0) 		
		// Decreto di morte
		fiend->status = dead;
	
}

// Data l'altezza h dell'oggetto, la sua larghezza l e una riga y
// nel tabellone di gioco, trova una x tale che il rettangolo ABCD sia completamente libero,
// con A = (x,y), B = (x + l, y), C = (x + l, y + h), D = (x, y + h)
int free_spot_search(int m, int n, object board[m][n], int height, int width, int * y) {

	int i = 0, j = 0;
		
	int x = 0;
	
	for (i = 0; i < height; i++)
		for (j = 0; j < width; j++)
			if (!obj_is_null(board[*y + i][x + j])) {
				i = 0;
				j = 0;
				x = x + j + 1;
				// Se raggiunta fine riga, rinizia dalla prossima e modifica y
				if (x >= MAXX-1) {
						x = 0;
						(*y)++;
					}
			}

	return x;
}

// Verifica se è avvenuta una collisione tra l'oggetto corrente
// e una posizione che vuole occupare
_Bool collision(object * item, object position) {
	
	return 
		(!obj_is_null(position)) &&
		(!obj_equal(*item, position)) &&
	
	(
		// Collisione tra giocatore e bomba
	 	(item->class == player_c && position.class == bomb_c) ||
		(item->class == bomb_c && position.class == player_c) ||
		// Collisione tra giocatore e navicella nemica
	 	(item->class == player_c && position.class == fiend_c) ||
		(item->class == fiend_c && position.class == player_c) ||
		// Collisione tra navicella nemica e missile
	 	(item->class == fiend_c && position.class == rocket_c) ||
		(item->class == rocket_c && position.class == fiend_c) ||
		// Collisione tra navicella nemica e bomba
	 	(item->class == fiend_c && position.class == bomb_c) ||
		(item->class == bomb_c && position.class == fiend_c) ||
		// Collisione tra due navicelle nemiche
	 	(item->class == fiend_c && position.class == fiend_c) ||
		// Collisione tra bomba e missile
		(item->class == bomb_c && position.class == rocket_c) ||
		(item->class == rocket_c && position.class == bomb_c)
	);
}

// Rimuove dal tabellone un disegno e le informazioni sull'oggetto corrispondente
void rub_out(int y, int x, int height, int width, int m, int n, object board[m][n]) {
	
	int i, j;
	
	int sup_y = y + height -1; 	// y del carattere dell'oggetto più in basso
	int sup_x = x + width -1; 	// x del carattere dell'oggetto più a destra

	for (i = y; i <= sup_y; i++) 
		for (j = x; j <= sup_x; j++) {
			obj_reset(&board[i][j]);
			board_mvaddch(i, j, ' '); 
		}
	curs_set(0);
	refresh(); 
	
}

// Gestione collisioni
void launch_collision(object * item, object * collided_body, int m, int n, object board[m][n], std_args * shared, int * game_points) {
	
	// Di seguito, per entità attiva (item) si intende l'oggetto il cui movimento ha causato la collisione
	// Per entità passiva (collided_body) si intende l'oggetto già presente in una posizione che l'entità attiva voleva occupare 
	
		if (item->class == fiend_c && collided_body->class == fiend_c) {
	
		object * fiend_a = item;
		object * fiend_b = collided_body;		
		
		// Rimbalzo solo per collisioni orizzontali
		if (fiend_a->prev_y + fiend_a->height >= fiend_b->y) {
			fiend_a->hdirection *= -1; // Rimbalzo		
		
			// Rimbalzo anche dell'entità passiva se non c'è rischio di conflitti con rimbalzo a lato schermo
			if (fiend_a->prev_x > 0 && fiend_a->prev_x + fiend_a->width < MAXX && fiend_b->x > 0 && fiend_b->x + fiend_b->width < MAXX) {
				fiend_b->hdirection = fiend_a->hdirection * -1;
				fiend_b->revision++;
			}
		
		}

		// Ripristina posizione originale
		fiend_a->x = fiend_a->prev_x;
		fiend_a->y = fiend_a->prev_y;
	}  
	
	// Collisione tra navicella e missile 
	else if ((item->class == fiend_c && collided_body->class == rocket_c) || (item->class == rocket_c && collided_body->class == fiend_c)) {
	
		object * fiend_a;
 
		object * weapon_a;
 
		if (item->class == fiend_c) {
 
			// L'entità attiva è la navicella
			fiend_a = item;
			weapon_a = collided_body;
 
			// Ripristina posizione originale
			fiend_a->x = fiend_a->prev_x;
			fiend_a->y = fiend_a->prev_y;
 
			// Elimina il missile
			rub_out(weapon_a->y, weapon_a->x, weapon_a->height, weapon_a->width, m, n, board);
 
		}
		else {
			// L'entità attiva è l'arma
			fiend_a = collided_body;
			weapon_a = item;

			// Poichè la navicella è l'entità passiva, occorre segnalare che saranno
			// eseguite delle modifiche, in modo da restituire queste e di non elaborare quelle ricevute
			// alla prossima sincronizzazione dell'oggetto
			fiend_a->revision++;	

			// Elimina il missile
			rub_out(weapon_a->prev_y, weapon_a->prev_x, weapon_a->height, weapon_a->width, m, n, board);

			weapon_a->status = dead;	
		}

		update_points(game_points, POINTS_HIT);

		// Toglie punti salute alla navicella
		fiend_damage(fiend_a);
			
		// Se la navicella è morta, bonus punti
		if (fiend_a->hp == 0)
			update_points(game_points, POINTS_FRAG);

	}
	
	// Collisione tra navicella e bomba 
	else if ((item->class == fiend_c && collided_body->class == bomb_c) || (item->class == bomb_c && collided_body->class == fiend_c)) {

		object * fiend_a;		
		object * weapon_a;

		// L'entità attiva è la navicella
		if (item->class == fiend_c) {
			fiend_a = item;

			// Ripristino posizione precedente
			fiend_a->x = fiend_a->prev_x;
			fiend_a->y = fiend_a->prev_y;

		}
		// L'entità attiva è l'arma
		else {
			fiend_a = collided_body;
			weapon_a = item;
	
			// Ripristino posizione
			weapon_a->y = weapon_a->prev_y;
			weapon_a->x = weapon_a->prev_x;
			
			// Salto in avanti (weapon_task() provvederà a ripristinare il normale passo dopo il salto)
			weapon_a->vdirection = fiend_a->height + 1;	
		}

	}

	// Collisione tra bomba e missile 
	else if ((item->class == bomb_c && collided_body->class == rocket_c) || (item->class == rocket_c && collided_body->class == bomb_c)) {

			// Elimina entità attiva
			rub_out(item->prev_y, item->prev_x, item->height, item->width, m, n, board);
			item->status = dead;
			
			// Elimina entità passiva
			rub_out(collided_body->y, collided_body->x, collided_body->height, collided_body->width, m, n, board);

	}
	
	// Collisione tra giocatore e bomba o navicella
	else if (
	(item->class == player_c && (collided_body->class == bomb_c || collided_body->class == fiend_c)) ||
	((item->class == bomb_c || item->class == fiend_c) && collided_body->class == player_c)
	) 
		*shared->game_over = true; // Game over

	
} 

// Disegna un oggetto sul tabellone, avendo cura di cancellare
// il disegno precedente (se presente) e verificando eventuali collisioni
void draw(object * item,  int m, int n, object board[m][n], std_args * shared, int * game_points) {

	object * collided_body;
	int i, j;
	
	int k; // Puntatore al carattere in item->pic[]

	int sup_y = item->y + item->height -1; 	// y del carattere dell'oggetto più in basso
	int sup_x = item->x + item->width -1; 	// x del carattere dell'oggetto più a destra

	// Se l'oggetto è "morto", cancella semplicemente il disegno
	if (item->status == dead) 
		rub_out(item->y, item->x, item->height, item->width, m, n, board);
	else {
		
		// Controlla che le nuove posizioni da occupare non appartengano ad un altro oggetto
		for (i = item->y; i <= sup_y; i++) 
			for (j = item->x; j <= sup_x; j++)
				if (collision(item, board[i][j])) {	
					collided_body = &board[board[i][j].y][board[i][j].x];		
					launch_collision(item, collided_body, m, n, board, shared, game_points);
					return;		
				}
	
		// Se non è la prima volta che l'oggetto viene disegnato, cancella il disegno precedente
		if (item->prev_x != NOWHERE)
			rub_out(item->prev_y, item->prev_x, item->height, item->width, m, n, board);
		
		// Disegna l'oggetto con le nuove coordinate
		for (i = item->y; i <= sup_y; i++) 
			for (j = item->x; j <= sup_x; j++) {
				
				// Calcolo del puntatore per la stringa con il disegno, trattata come un array bidimensionale
				k = ((i - item->y) * item->width) + ((j - item->x) % item->width);	
				
				// Riempe la posizione con le informazioni sull'oggetto
				obj_copy(&board[i][j], *item);
				
				// Disegna 
				board_mvaddch(i, j, item->pic[k]);
			}
			
		curs_set(0);			
		refresh();	
	}
	
}

// Controlla che le coordinate di un oggetto siano all'interno dei limiti del tabellone
int within_bounds(object * item) {
	
	return 	(
			((item->y + item->height - 1) < BOARD_HEIGHT) && 
			(item->y >= 0) && 
			(item->x >= 0) && 
			((item->x + item->width - 1) < BOARD_WIDTH)
			);
}


// Routine per la generazione di coordinate di un'arma (bomba o missile)
void * weapon_task(void * args) {
	
	weapon_args * orders = (weapon_args *) args;
	
	std_args * shared = orders->shared;
	
	(*(shared->running_threads))++;
	
	object itself = orders->itself;

	free(args);
	
	// Aggiorna il pid (Al momento è quello del processo che ha generato il segnale per l'arma)
	itself.tid = pthread_self();

	switch (itself.class) {
		case bomb_c:
			itself.vdirection = +1;	// Verso il basso
			itself.hdirection = 0;
		break;
		case rocket_c:
			itself.vdirection = -1; // Verso l'alto
		break;
	}

	do {
		
		// Comunicazione unidirezionale (le collisioni comportano sempre la morte dell'oggetto, eventualmente comunicata con kill())
		queue_obj(&itself, shared);
		
		usleep(SLEEP_INTERVAL);

		// Aggiorna coordinate
		obj_new_coords(&itself, itself.y + itself.vdirection, itself.x + itself.hdirection);
		
		if (itself.vdirection > 1) itself.vdirection = 1;
		
	} while (itself.status > dead && within_bounds(&itself) && !(*(shared->game_over)));

	// Comunica la morte solo se l'uscita dal loop non è avvenuta per game over
	if (!(*(shared->game_over))) {
		
		// Comunica la morte solo se è avvenuta per uscita dai confini del tabellone
		if (!within_bounds(&itself)) {
			itself.y = itself.prev_y;
			itself.x = itself.prev_x;
			itself.status = dead;
			queue_obj(&itself, shared);
		}
	}
	
	// Deallocazione proprio semaforo
	sem_destroy(&itself.obj_mutex);
	
	(*(shared->running_threads))--;

}

// Routine per l'acquisizione di caratteri da tastiera e movimento del giocatore
void * player_task(void * args) {

	// Cast al tipo corretto dell'argomento
	std_args * shared = (std_args *) args;	
	
	// Segnala nuovo thread
	(*(shared->running_threads))++;

	object player;
	
	char c;
	int i;
	time_t rocket_wall_last_used;
	
	// Esponente per la direzione orizzontale per il prossimo missile
	int direction = 0;

	obj_init(&player, player_c, PLAYER_ID, PLAYER_START_Y, PLAYER_START_X, PLAYER_HEIGHT, PLAYER_WIDTH, 0, &pics[player_p][0], alive);
	queue_obj(&player, shared);
	
	rocket_wall_last_used = time(NULL) - SECS_BETWEEN_ROCKET_WALLS;
	
	while (!(*shared->game_over)) {

		c = getch();
		
			switch (c) {
				case QUIT_KEY:	// Uscita dal gioco (quit)
					*shared->game_over = true;
				break;
				case SPACEBAR: case UP: // Missile
				
					// Alterna direzione
					direction = (direction+1)%2;
				
					// Crea segnale per nuovo missile
					object new_rocket;
			
					obj_init(&new_rocket, sig_rocket, NULL_ID, player.y-1, player.x + (player.width/2) - direction, ROCKET_HEIGHT, ROCKET_WIDTH, 0,
					&pics[rocket_r_p + direction][0], alive);
			
					new_rocket.hdirection = pow(-1,direction);	// direzione alternata
							
					// Invia segnale per nuovo missile al manager, che si occuperà di generarare e lanciare il processo opportuno
					queue_obj(&new_rocket, shared);
				
				break;
			
				case DOWN: 	// Premendo "freccia giù" invece che la barra spaziatrice 
							// viene rilasciato un "muro" di missili
			
					// Controllo su ultimo uso muro di missili
					if (time(NULL) - rocket_wall_last_used >= SECS_BETWEEN_ROCKET_WALLS) {
						for (i = 0; i < MAXX; i++) {
							// Crea segnale per nuovo missile
							object new_rocket;
			
							obj_init(&new_rocket, sig_rocket, NULL_ID, player.y-1, i, ROCKET_HEIGHT, ROCKET_WIDTH, 0,
							&pics[rocket_s_p][0], alive);
							new_rocket.hdirection = 0;
							new_rocket.vdirection = -1;
				
							// Invia segnale per nuovo missile al manager, che si occuperà di generarare e lanciare il task opportuno					
							queue_obj(&new_rocket, shared);
						}
						// Aggiorna timestamp ultimo uso
						rocket_wall_last_used = time(NULL);
					}
				break;
			
				case LEFT:	// Spostamento a sinistra
					if (player.x > 0)
						obj_new_coords(&player, player.y, player.x-1);
						
					// Comunicazione con processo consumatore
					queue_obj(&player, shared);
				break;
				
				case RIGHT: // Spostamento a destra
					if (player.x + player.width -1 < MAXX - 1)
						obj_new_coords(&player, player.y, player.x+1);
					
					// Comunicazione con processo consumatore
					queue_obj(&player, shared);
				break;
			
			}

		

		}
		
	// Deallocazione proprio semaforo
	sem_destroy(&player.obj_mutex);
		
	// Segnala thread concluso
  	(*(shared->running_threads))--;

}


// Navicella nemica
void * fiend_task(void * args) {
	
	// Cast al tipo corretto dell'argomento e assegnamento a variabile locale
	fiend_args orders = * ((fiend_args *) args);
	
	// Deallocazione argomenti allocati ad-hoc per questo thread 
	free(args);

	// Segnala nuovo thread attivo
	(*(orders.shared->running_threads))++;

	int y = orders.y;
	object_status level = orders.level;
	int fiends_no = orders.shared->fiends_no;
	int fiend_id = orders.fiend_id; 
	
	object fiend;
	
	int i = 0, j = 1;
	int x = (fiend_id%MAX_FIENDS_PER_ROW) * (WIDEST + HSPACE_BETWEEN_FIENDS);
	
	// Differenti inizializzazioni a seconda del livello-status della navicella
	switch (level) {
		case lv1:	
		obj_init(&fiend, fiend_c, fiend_id, y, x, FIEND1_HEIGHT, FIEND1_WIDTH, FIEND1_MAX_HP, &pics[fiend1_p][0], level);	
		break;
		
		case lv2:
		obj_init(&fiend, fiend_c, fiend_id, y, NOWHERE, FIEND2_HEIGHT, FIEND2_WIDTH, FIEND2_MAX_HP, &pics[fiend2_p][0], level);
		break;
	
		case lv3:
		obj_init(&fiend, fiend_c, fiend_id, y, NOWHERE, FIEND3_HEIGHT, FIEND3_WIDTH, FIEND3_MAX_HP, &pics[fiend3_p][0], level);	
				
		break;
	}
		
	fiend.prev_y = fiend.y;
	
	fiend.hdirection = 1;	// Verso destra
	fiend.vdirection = 1;	// Verso il basso
	
	// Invia posizione iniziale
	queue_obj(&fiend, orders.shared);	
	
	do {
		
		// Se il precedente tentativo di discesa verso il basso 
		// non è avvenuto a causa di una collisione, riprova tra WIDEST spostamenti orizzontali
		if (j == 0 && fiend.y == fiend.prev_y)
				j = VMOVE_INTERVAL-WIDEST;
			
		fiend.prev_y = fiend.y;		
		fiend.prev_x = fiend.x;
		
		// Contatore modulo VMOVE_INTERVAL per la discesa verso il basso
		j++; j %= VMOVE_INTERVAL;
				
		if (j == 0) { 
			fiend.y += fiend.vdirection;	// Discesa verso il basso
			if (!within_bounds(&fiend)) { // Raggiunto il bordo inferiore, stop alle discese e ripristino y precedente
				fiend.y -= fiend.vdirection;
				fiend.vdirection = 0;
			}
		}
			
		// Rimbalzo ai lati dello schermo
		if (fiend.x + fiend.hdirection < 0 || (fiend.x + fiend.width - 1) + fiend.hdirection > MAXX -1 )
			fiend.hdirection *= -1;
			
		fiend.x += fiend.hdirection;
		
		// Comunicazione bidirezionale
		queue_obj(&fiend, orders.shared);

		// Controllo sullo stato della navicella, eventuale decreto di morte e generazione segnale navicella livello superiore
		fiend_health_check(&fiend);

		// Operazioni saltate se la navicella muore
		if (fiend.status > dead) {
			
			// Contatore modulo (fiends_no*BOMB_MULTIPLIER) per il lancio di bombe
			i++;
			i %= MAX_FIENDS_PER_ROW*BOMB_MULTIPLIER;
			
			if (i == (fiend_id % MAX_FIENDS_PER_ROW)*BOMB_MULTIPLIER) {
				
				// Generazione segnale nuova bomba
				object new_bomb;
				obj_init(&new_bomb, sig_bomb, NULL_ID, fiend.y+2, fiend.x, BOMB_HEIGHT, BOMB_WIDTH, 0, &pics[bomb_p][0], alive);
		
				// Invia il segnale solo se effettivamente la bomba sarà all'interno del tabellone
				// come invece non accadrebbe con navicella vicine al lato inferiore del tabellone
				if (within_bounds(&new_bomb))
					queue_obj(&new_bomb, orders.shared);
				else
					sem_destroy(&new_bomb.obj_mutex);

			}
			usleep(SLEEP_INTERVAL);
		}
		
		
	}
	while (fiend.status > dead && !(*(orders.shared->game_over)));
	
	// Operazioni non eseguite in caso di game over
	if (!(*(orders.shared->game_over))) {
	
	// Comunicazione dopo la morte (essenzialmente solo per cancellare il disegno)
	queue_obj(&fiend, orders.shared);
	
		// Generazione navicella livello superiore
		if (level < lv3) {
			pthread_t tmp;
			fiend_args * new_fiend_orders = malloc(sizeof(fiend_args));
			
			if (!new_fiend_orders) { endwin(); perror("Couldn't malloc."); exit(1); }
			
			new_fiend_orders->shared = orders.shared;
			new_fiend_orders->fiend_id = orders.fiend_id;
			new_fiend_orders->y = fiend.y;
			new_fiend_orders->level = level + 1;
			
			pthread_create(&tmp, NULL, &fiend_task, new_fiend_orders);
		
		}
	}
	// Deallocazione proprio semaforo
	sem_destroy(&fiend.obj_mutex);
	
	// Segnala thread concluso
	(*(orders.shared->running_threads))--;

}

// Crea un "tabellone" di posizioni di dimensione m*n
// e gestisce le interazioni tra gli oggetti sul tabellone, disegnandoli e verificando collisioni
_Bool manager(std_args * shared, int m, int n, int * game_points) {	
	
	int i, j;
	update_points(game_points, 0);
	int fiends_no = shared->fiends_no;
	_Bool win = false;
	
	object board[m][n];	// Tabellone
	
	object * value;		// Holder temporaneo dell'oggetto corrente
	int out = 0;
	int rocket_counter = 0, bomb_counter = 0;
	int fiends_still_alive = fiends_no;
	
	weapon_args * weapon_orders;	// Puntatore per memoria dinamica
	
	object * copy_on_board = NULL;	// Puntatore alla copia sul tabellone dell'oggetto corrente
	
	pthread_t tmp;

	// Inizializza tabellone
	for (i = 0; i < m; i++)
		for (j = 0; j < n; j++)
			obj_reset(&board[i][j]);
	
	
	do {
		
		// Attesa al semaforo "presenti"
		while (sem_trywait(shared->present) == -1) {
			if (*shared->game_over)
				return win;
			usleep(500);
		}
				
		// Attesa al mutex del buffer
		sem_wait(shared->mutex);
		
			// Prelievo
			value = shared->buffer[out];
		
		// Rilascio mutex del buffer
		sem_post(shared->mutex);
		
		// Segnala semaforo "disponibili"
		sem_post(shared->available);
		
		// Aggiorna indice elemento da prelevare
		out = (out + 1)%(shared->buffer_size);
		
		copy_on_board = NULL;
		
		switch(value->class) {

				case player_c: case rocket_c: case bomb_c: case fiend_c:
					if (value->status > dead) {
						
						// Ricerca di copia pre-esistente sul tabellone
						if (value->prev_x != NOWHERE && obj_equal(*value, board[value->prev_y][value->prev_x]))
							copy_on_board = &board[value->prev_y][value->prev_x];
						else {
							
							// Assegnazione di x in base alla disponibilità del tabellone, per navicelle di livello superiore
							if (value->x == NOWHERE)
								value->x = free_spot_search(m, n, board, value->height, value->width, &value->y);
						}
					}
					else {	// Oggetto morto
					
						if (obj_equal(*value, board[value->y][value->x])) {
							copy_on_board = &board[value->y][value->x];
								
							if (copy_on_board->status == lv3) {
								fiends_still_alive--;	// Ai fini del game over è necessario computare solo il numero di navicelle di terzo livello uccise
								
								// Verifica ed eventuale decreto di game over
								if (fiends_still_alive == 0) 
									win = *shared->game_over = true;
							}
						}
				
					}

					// Disegna solo oggetti mai apparsi sul tabellone, oggetti morti od oggetti per cui non esista una versione più recente (dovuta a collisioni)
					if (value->prev_x == NOWHERE || (copy_on_board && (copy_on_board->revision <= value->revision || value->status == dead))) {
						draw(value, m, n, board, shared, game_points);
						copy_on_board = value;
						
						// Raggiungimento da parte di un nemico dell'ultima linea del tabellone
						if (copy_on_board->class == fiend_c && copy_on_board->y + copy_on_board->height == m) *shared->game_over = true;
					}
					else if (copy_on_board)
						*value = *copy_on_board;				

					// Rilascia l'oggetto elaborato
					release_obj(value);
				break;

				case sig_bomb: case sig_rocket: // Segnale nuovo arma sul tabllone
				
				weapon_orders = malloc(sizeof(weapon_args));
				
				if (!weapon_orders) { endwin(); perror("Couldn't malloc."); exit(1); }
				
				// Copia negli argomenti per weapon_task() dell'oggetto
				weapon_orders->itself = *value;
				
				// Rilascia l'oggetto elaborato
				release_obj(value);
				
				weapon_orders->shared = shared;
				
				// Trasformazione del segnale in oggetto
				switch (value->class) {
						case sig_bomb: weapon_orders->itself.id = bomb_counter++; weapon_orders->itself.class = bomb_c; break;
						case sig_rocket: weapon_orders->itself.id = rocket_counter++; weapon_orders->itself.class = rocket_c; break;				
				}				
				
				// Creazione del thread
				pthread_create(&tmp, NULL, &weapon_task, weapon_orders);
				
				break;
				
			}
	}
	while (!(*shared->game_over));
	
	return win;
	
}
