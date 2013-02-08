/*
	main.c
	Calculus Invaders, esercizio 2
	Scritto da Riccardo Angius
	Ultima modifica: 14 gennaio 2012
*/
#include "invdrslib.h"

int main(int argc, char * argv[]) {

	// Controllo sul passaggio effettivo del numero di navicelle richieste
	if (argc < 2) {
		printf("Fornire come primo parametro il numero M di navicelle nemiche.\n");
		exit(1);
	}
	
	std_args shared;	// Informazioni convidise dai thread
	
	shared.fiends_no = atoi(argv[1]);	
	
	// Controlla se il parametro fornito non è maggiore del numero di navicelle massimo per una visualizzazione ottimale
	if (shared.fiends_no > MAX_FIENDS) 
		shared.fiends_no = MAX_FIENDS;
	
	// Riempimento dell'array statico contenente i disegni degli oggetti 	
	strcpy(pics[player_p], PLAYER_PIC);
	strcpy(pics[rocket_r_p], ROCKET_RIGHT_PIC);
	strcpy(pics[rocket_l_p], ROCKET_LEFT_PIC);
	strcpy(pics[rocket_s_p], ROCKET_SIDE_PIC);
	
	strcpy(pics[bomb_p], BOMB_PIC);
	strcpy(pics[fiend1_p], FIEND1_PIC);
	strcpy(pics[fiend2_p], FIEND2_PIC);
	strcpy(pics[fiend3_p], FIEND3_PIC);	
		
	initscr();		// Inizializza schermo

	noecho();		// Previene la stampa dei caratteri premuti
	curs_set(0);	// Nasconde cursore
	welcome_screen(); // Schermata di avvio
	srand(time(NULL));

	// Nella schermata di gioco, stampa in alto a sinistra il nome del gioco
	mvprintw(0, 0, "CALCULUS INVADERS");
	
	int i, j, y;

	shared.buffer_size = BUFFER_SIZE;
	
	// Si usa di un buffer di puntatori ad oggetti
	object * buffer[shared.buffer_size];
	
	shared.buffer = buffer;

	// Semafori e loro allocazione
	sem_t available, present, mutex; 

	sem_init (&present, 0, 0);
	
	sem_init (&available, 0, shared.buffer_size);
	
	sem_init (&mutex, 0, 1);
	
	shared.available = &available;
	shared.present = &present;
	shared.mutex = &mutex;
	
	// Flag per game over
	sig_atomic_t game_over = false;
	shared.game_over = &game_over;
	
	// Flag per attesa thread in esecuzione a fine gioco
	sig_atomic_t running_threads = 0;
	shared.running_threads = &running_threads;
	
	int in = 0;
	shared.in = &in;

	pthread_t tmp;
	 
	int game_points = 0;	// Punti
	_Bool win;
	
	// Creazione thread giocatore
	pthread_create(&tmp, NULL, &player_task, &shared);
				
	// Genera fiends_no thread per ogni navicella nemica iniziale
	for (i = 0; i < shared.fiends_no; i++) {
		
		fiend_args * orders = malloc(sizeof(fiend_args));
		
		if (!orders) { endwin(); perror("Couldn't malloc"); exit(1); }
		
		orders->shared = &shared;
		orders->fiend_id = i;
		orders->y = ((i / MAX_FIENDS_PER_ROW) * (TALLEST + VSPACE_BETWEEN_FIENDS));
		orders->level = lv1;
		
		pthread_create(&tmp, NULL, &fiend_task, orders);

	}

	// Lancia il gestore di disegno e di collisioni
	win = manager(&shared, MAXY - GAME_STATUS_ROWS, MAXX, &game_points);
	
		
	game_over_screen(game_points, win); // Schermata fine gioco
	
	// Attesa (senza spinlock) della terminazione di tutti gli altri thread
	while (running_threads > 0)
		usleep(50000);
		
	// Deallocazione semafori
	sem_destroy(&present);
	sem_destroy(&available);
	sem_destroy(&mutex);
	
	endwin();		// Ripristina schermo
	
	return 0;
}
