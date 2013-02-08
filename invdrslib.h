/*
	invdrslib.h
	Calculus Invaders, esercizio 2
	Scritto da Riccardo Angius
	Ultima modifica: 14 gennaio 2012
*/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/wait.h> 
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <limits.h>
#include <ncurses.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>

// Inizio parametri di gioco personalizzabili

#define POINTS_HIT 50
#define POINTS_FRAG 200

#define SLEEP_INTERVAL (70000)	// microsecondi tra ogni generazione di coordinate

// Giocatore
#define PLAYER_PIC 	"  _ |  | _| "
#define PLAYER_WIDTH 3
#define PLAYER_HEIGHT 4

// Missile
#define ROCKET_WIDTH 1
#define ROCKET_HEIGHT 1
#define ROCKET_RIGHT_PIC "/"
#define ROCKET_LEFT_PIC "\\"
#define ROCKET_SIDE_PIC "O"

// Bomba
#define BOMB_WIDTH 1
#define BOMB_HEIGHT 1
#define BOMB_PIC "8"

// Navicella nemica nella prima fase di vita
#define FIEND1_PIC "dxdy"	
#define FIEND1_HEIGHT 2	// max 3 secondo specifica
#define FIEND1_WIDTH 2	// max 3 secondo specifica
#define FIEND1_MAX_HP 2

// Navicella nemica nella seconda fase di vita
#define FIEND2_PIC "d2xdy2"	
#define FIEND2_HEIGHT 2	// max 4 secondo specifica
#define FIEND2_WIDTH 3	// max 4 secondo specifica
#define FIEND2_MAX_HP 3
 
// Navicella nemica nella seconda fase di vita
#define FIEND3_PIC "d3xdy3"	
#define FIEND3_HEIGHT 2	// max 5 secondo specifica
#define FIEND3_WIDTH 3	// max 5 secondo specifica
#define FIEND3_MAX_HP 4

// Modificare questi parametri in relazione ai precedenti
#define WIDEST PLAYER_WIDTH
#define TALLEST PLAYER_HEIGHT
#define TALLEST_FIEND FIEND3_HEIGHT

#define BOMB_MULTIPLIER	20	// Divisore della frequenza di lancio bombe (per valori minori bombe più frequenti) (Ottimale = 30)
#define VMOVE_INTERVAL	80 // Numero di spostamenti orizzontali della navicella tra ogni spostamento orizzontale (Minimo = WIDEST, Ottimale = 80)


// FINE Inizio parametri di gioco personalizzabili

#define BUFFER_SIZE 100

#define NO_FEEDBACK (-1)

#define NULL_ID (-1)

#define GAME_STATUS_ROWS 1

#define MAXY 24
#define MAXX 80

#define BOARD_HEIGHT (MAXY - GAME_STATUS_ROWS)
#define BOARD_WIDTH MAXX

#define UP 65
#define DOWN 66
#define LEFT 68
#define RIGHT 67
#define SPACEBAR 32
#define QUIT_KEY 'q'

#define NOWHERE (-1)

#define PLAYER_START_X (MAXX/2-PLAYER_WIDTH/2)
#define PLAYER_START_Y (MAXY-GAME_STATUS_ROWS-PLAYER_HEIGHT)
#define PLAYER_ID 0

#define MAX_PIC_SIZE ((TALLEST*WIDEST)+1)

#define VSPACE_BETWEEN_FIENDS 2
#define HSPACE_BETWEEN_FIENDS 1

#define MAX_FIENDS_PER_ROW (MAXX/(WIDEST+HSPACE_BETWEEN_FIENDS))
#define MAX_FIENDS_PER_COL ((MAXY-GAME_STATUS_ROWS-PLAYER_HEIGHT)/(TALLEST_FIEND+VSPACE_BETWEEN_FIENDS))
#define MAX_FIENDS ((MAX_FIENDS_PER_ROW)*(MAX_FIENDS_PER_COL))

#define SECS_BETWEEN_ROCKET_WALLS 10

#define PICS_NO 8

#define WELCOME_FILE "welcome_screen.txt"
#define LOSER_FILE "loser.txt"
#define WINNER_FILE "winner.txt"

// Variabile statica che conterrà i disegni dei vari tipi di oggetto
char pics[PICS_NO][MAX_PIC_SIZE];

// Indici dei disegni
typedef enum { player_p, rocket_r_p, rocket_l_p, rocket_s_p, bomb_p, fiend1_p, fiend2_p, fiend3_p } pic_id;

typedef short int object_id;
typedef enum { null_c, player_c, fiend_c, rocket_c, bomb_c, sig_quit, sig_pause, sig_bomb, sig_rocket, sig_fiend } object_class;
typedef enum { dead, alive, lv1, lv2, lv3 } object_status;

typedef struct object {
	object_id id;		// ID dell'oggetto
	object_class class;	// Classe dell'oggetto
	object_status status;	// Stato dell'oggetto (vivo o morto per oggetti semplici e giocatore, livello per navicelle nemiche)
	pthread_t tid;			// tID dell'oggetto
	int y;				// y del carattere (dell'oggetto) più in alto
	int prev_y;			// y precedente
	int height;			// altezza dell'oggetto
	int x;				// x del carattere (dell'oggetto) più a sinistra
	int prev_x;			// x precedente
	int width;			// larghezza dell'oggetto
	int hp;				// punti salute dell'oggetto
	sem_t obj_mutex;	// mutex per l'elaborazione dell'oggetto
	int revision;		// numero revisione della copia dell'oggetto
	int hdirection;		// passo orizzontale
	int vdirection;		// passo verticale
	char * pic;			// puntatore alla stringa contenente il disegno dell'oggetto
} object;

// Informazioni condividise dai thread
typedef struct {
	object  * * buffer;	// array buffer
	int buffer_size;		// dimensione del buffer
	int * in;				// indice per nuovo elemento
	sem_t * available;		// semaforo "disponibili"
	sem_t * present;		// semaforo "presenti"
	sem_t * mutex;			// puntatore al mutex del buffer
	sig_atomic_t * game_over;	// Variable atomica per segnalazione asincrona di game over
	int fiends_no;			// numero di navicelle
	sig_atomic_t * running_threads;	// Variable atomica per l'attesa all'uscita dei thread ancora in esecuzione
} std_args;

// Parametri per fiend_task()
typedef struct {
	std_args * shared;		// Puntatore ad informazioni condivise
	int y;
	int fiend_id;
	object_status level;
} fiend_args;

// Parametri per weapon_task()
typedef struct {
	std_args * shared;
	object itself;
} weapon_args;

void board_mvaddch(int y, int x, char c);
void obj_init(object * item, object_class class, object_id id, int start_y, int start_x, int default_height, int default_width, int default_health, char * default_pic, object_status status);
void obj_new_coords(object * item, int y, int x);
_Bool obj_is_null(object a);
void obj_reset(object * info);
void print_file(char * filename);
void welcome_screen();
void game_over_screen(int game_points, _Bool win);
void update_points(int * game_points, int offset);
void obj_copy(object * dest, object source);
_Bool obj_equal(object a, object b);
void queue_obj(object * item, std_args * shared);
void release_obj(object * item);
void fiend_damage(object * fiend);
void fiend_health_check(object * fiend);
int free_spot_search(int m, int n, object board[m][n], int height, int width, int * y);
_Bool collision(object * item, object position);
void launch_collision(object * item, object * collided_body, int m, int n, object board[m][n], std_args * shared, int * game_points);
void draw(object * item,  int m, int n, object board[m][n], std_args * shared, int * game_points);
void * weapon_task(void * args);
void * player_task(void * args);
void * fiend_task(void * args);
_Bool manager(std_args * shared, int m, int n, int * game_points);
