#include "main.h"

MPI_Datatype MPI_PAKIET_T;
pthread_t threadCom, threadM;

pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t modify_permits = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t modify_exited_from_pyrkon = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wait_for_agreement_to_enter = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wait_for_new_pyrkon = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t on_lecture_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t on_pyrkon_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gtfo_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t i_can_allow_pyrkon_entering_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t i_can_allow_lecture_entering_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile int state = BEFORE_PYRKON;
volatile int lamport_clock;
volatile int last_message_clock = 0;
volatile int pyrkon_number = 0;
volatile int exited_from_pyrkon = 0;
volatile int* permits;
volatile int* desired_lectures;

/* end == TRUE oznacza wyjście z main_loop */

void mainLoop(void);

/* Deklaracje zapowiadające handlery. */
void want_enter_handler(packet_t *message);
void alright_enter_handler(packet_t *message);
void exit_handler(packet_t *message);

/* typ wskaźnik na funkcję zwracającej void i z argumentem packet_t* */
typedef void (*f_w)(packet_t *);

/* Lista handlerów dla otrzymanych pakietów
   Nowe typy wiadomości dodaj w main.h, a potem tutaj dodaj wskaźnik do handlera.
   Funkcje handlerowe są na końcu pliku.
   Nie zapomnij dodać deklaracji zapowiadającej funkcji!
*/
f_w handlers[MAX_HANDLERS] = {
        [WANT_TO_ENTER] = want_enter_handler,
        [ALRIGHT_TO_ENTER] = alright_enter_handler,
        [EXIT] = exit_handler
};

extern void inicjuj(int *argc, char ***argv);
extern void finalizuj(void);

/**
 * Function increments Lamport clock based on information from input.
 * @param to_increase integer has value either 0 or 1.
 * @return value of Lamport clock either incremented or not.
 */
int get_clock( int to_increase ) {

    pthread_mutex_lock( &clock_mutex );

    if( to_increase ) lamport_clock++;
    last_message_clock = lamport_clock; // We are remembering the value of Lamport clock.

    pthread_mutex_unlock( &clock_mutex );
    return last_message_clock;
}

int get_state(){
    pthread_mutex_lock( &state_mutex );
    int a = state;
    pthread_mutex_unlock( &state_mutex );
    return a;
}   

void change_state(int new_state){
    pthread_mutex_lock( &state_mutex );
    state = new_state;
    pthread_mutex_unlock( &state_mutex );
}

/**
 * Function broadcasts packets to every process.
 * @param type Describes an activity that a process wants to do.
 * @param data Further information regarding
 * @param additional_data Even further information regarding action.
 */
void pyrkon_broadcast( int type, int data ) {

    packet_t newMessage;
    newMessage.ts = get_clock( TRUE );
    newMessage.data = data;
    newMessage.pyrkon_number = pyrkon_number;

    for (int i = 0; i < size; i++){
        if (rank != i) {
            println("Sending %d %d do %d\n", type, newMessage.data, i);
            sendPacket( &newMessage, i, type );
            println("Sending %d %d do %d\n", type, newMessage.data, i);
        }
    }
}

/**
 * Function draws int from interval; both inclusive.
 * @param min left number in an interval.
 * @param max right number in an interval.
 * @return drawn int
 */
int my_random_int( int min, int max ) {
    float tmp = (float) ( (float) rand() / (float) RAND_MAX );
    return (int) (tmp * ( max - min + 1 ) + min);
}

int main ( int argc, char **argv ) {

    /* Tworzenie wątków, inicjalizacja itp */
    inicjuj( &argc, &argv);
    pthread_mutex_lock( &i_can_allow_lecture_entering_mutex );
    mainLoop();
    finalizuj();
    return 0;
}

/**
 * Main thread - process is participating in Pyrkon
 */
void mainLoop ( void ) {

     int prob_of_sending = PROB_OF_SENDING;
     int dst;
     packet_t pakiet;

    /* mały sleep, by procesy nie zaczynały w dokładnie tym samym czasie */
    struct timespec t = { 0, rank*50000 };
    struct timespec rem = { 1, 0 };
    nanosleep( &t, &rem);

    permits = malloc( ( LECTURE_COUNT + 1 ) * sizeof( int ) );
    desired_lectures = malloc( ( LECTURE_COUNT + 1 ) * sizeof( int ) );

    while( !end ) {
	    int percent = rand()%2 + 1;
        struct timespec t2 = { percent, 0 };
        struct timespec rem2 = { 1, 0 };
        nanosleep( &t2, &rem2 );

        int current_state = get_state();
        switch( current_state ){

            case BEFORE_PYRKON: {
                /* Process is waiting in line to enter Pyrkon. It broadcasts question and waits for agreement. */
                println( "Is waiting in line to enter Pyrkon\n" )

                pyrkon_broadcast(WANT_TO_ENTER, 0);

                pthread_mutex_lock( &wait_for_agreement_to_enter );
                pthread_mutex_lock( &wait_for_agreement_to_enter );
                pthread_mutex_unlock( &wait_for_agreement_to_enter );
                change_state(ON_PYRKON);
                pthread_mutex_lock( &on_pyrkon_mutex );
                pthread_mutex_lock( &i_can_allow_pyrkon_entering_mutex );
                pthread_mutex_unlock( &i_can_allow_lecture_entering_mutex );

                break;
            }

            case ON_PYRKON: {
                /* Process entered Pyrkon and chooses lectures. */
                println( "Is choosing it's lectures.\n" )

                for(int i = 1; i <= LECTURE_COUNT; i++) {
                    desired_lectures[i]=my_random_int(0,1);
                }

                /* When it's chosen its desired lectures it broadcasts that information to others. */
                for(int i = 1; i <= LECTURE_COUNT; i++) {
                    if(desired_lectures[i]) pyrkon_broadcast(WANT_TO_ENTER, i);
                }

                println( "Is waiting for lectures.\n" )
                pthread_mutex_lock( &gtfo_mutex ); //blokada
                pthread_mutex_lock( &gtfo_mutex ); //czekanko
                pthread_mutex_unlock( &gtfo_mutex ); //odblokowanie, żeby w następnym obiegu dało się odblokować
                pthread_mutex_lock( &i_can_allow_lecture_entering_mutex );
                break;
            }
            case AFTER_PYRKON: {
                /* Process broadcasts information that it's left Pyrkon */
                println( "PROCESS [%d] is AFTER_PYRKON.\n", rank )

                pyrkon_broadcast(EXIT, 0);

                pthread_mutex_lock( &wait_for_new_pyrkon ); // Process waits for everyone
                change_state(BEFORE_PYRKON); // and when everybody's ready new Pyrkon starts.
                pthread_mutex_unlock( &i_can_allow_pyrkon_entering_mutex );

                pyrkon_number ++;
                break;
            }
            default:
                break;
        }
    }
}

/**
 * Communication thread - for every received message a handler is called.
 * @param ptr
 * @return 0
 */
void *comFunc ( void *ptr ) {
    MPI_Status status;
    packet_t *pakiet;

    /* odbieranie wiadomości */
    while ( !end ) {

        pakiet = (packet_t *)malloc(sizeof(packet_t));
	    println("Waiting for messages.\n")
        MPI_Recv( pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status );
        pakiet->src = status.MPI_SOURCE;
        println( "Received a message from [%d] \n", pakiet->src );

        pthread_mutex_lock( &clock_mutex );
        if( lamport_clock < pakiet->ts ) {
            lamport_clock = pakiet->ts + 1;
        } else {
            lamport_clock ++;
        }
        pthread_mutex_unlock( &clock_mutex );

        if ( pyrkon_number == pakiet -> pyrkon_number ){
            println( "Pyrkon number [%d] is valid\n", pyrkon_number )
            pthread_t new_thread;
            pthread_create(&new_thread, NULL, (void *)handlers[(int)status.MPI_TAG], pakiet);
        }
    }
    return 0;
}

void allow_pyrkon ( packet_t *message ) {
    println( "Received info that [%d] wants to enter Pyrkon.\n", message->src )

    while( TRUE ) {

        pthread_mutex_lock( &i_can_allow_pyrkon_entering_mutex );

        int clock_allows = ( message->ts < last_message_clock ||
                ( message->ts == last_message_clock && rank > message->src ) );

        if( clock_allows ) {
            println( "Sends agreement to enter Pyrkon to [%d].\n", message->src )
            message->ts = get_clock( TRUE );
            sendPacket( message, message->src, ALRIGHT_TO_ENTER );
            break;
        }
        pthread_mutex_unlock( &i_can_allow_pyrkon_entering_mutex );
    }
    pthread_mutex_unlock( &i_can_allow_pyrkon_entering_mutex );
}

void allow_lecture ( packet_t *message ) {

    println( "Received info that [%d] wants to enter lecture [%d].\n", message->src, message->data )

    int lecture = message->data;
    while( TRUE ){

        pthread_mutex_lock( &i_can_allow_lecture_entering_mutex );
        int clock_allows;
        if ( desired_lectures[lecture] ){
             clock_allows = ( message->ts < last_message_clock ||
                    ( message->ts == last_message_clock && rank > message->src ) );
        } else {
            clock_allows = TRUE;
        }
        if( clock_allows ){

            pthread_mutex_lock( &on_lecture_mutex );
            pthread_mutex_unlock( &on_lecture_mutex );

            println( "Sends agreement to enter lecture [%d] to [%d].\n", message->data, message->src )
            message->ts = get_clock( TRUE );
            sendPacket( message, message->src, ALRIGHT_TO_ENTER );
            break;
        }
        pthread_mutex_unlock( &i_can_allow_lecture_entering_mutex );    
    }
    pthread_mutex_unlock( &i_can_allow_lecture_entering_mutex );
}

/**
 * Function received WANT_TO_ENTER message from other process and deals with it's content.
 * @param message packet_t received from other process.
 */
void want_enter_handler ( packet_t *message ) {
    if( message->data == ENTER_PYRKON ) {
        allow_pyrkon(message);
    } else {
        allow_lecture(message);
    }
    free(message);
}

void alright_enter_pyrkon_extension (packet_t *message ) {
    if( get_state() == BEFORE_PYRKON ){
        /* Process is waiting to enter Pyrkon and it's received message that allows it to go in. */
        println( "Received agreement to enter Pyrkon from [%d].\n ", message->src)

        pthread_mutex_lock( &modify_permits ); // To access permits we are locking modify_permits.
        int number_of_permits = ++ permits[ message->data ]; // Process increases number of received permits.
        pthread_mutex_unlock( &modify_permits ); // Permits are no longer needed -> unlocking modify_permits.

        if( number_of_permits >= size - MAX_PEOPLE_ON_PYRKON ) {
            pthread_mutex_unlock( &wait_for_agreement_to_enter );
        }
    }
}

void alright_enter_lecture_extension(packet_t *message ) {
    if ( get_state() == ON_PYRKON ) {
        /* Process is on Pyrkon and receives a message allowing it to participate in one lecture. */
        println( "Received agreement to enter lecture [%d] from [%d].\n", message->data, message->src )

        pthread_mutex_lock( &modify_permits ); // To access permits we are locking modify_permits.
        int number_of_permits = ++ permits[ message->data ]; // Process increases number of received permits.

        if( desired_lectures[message->data] && number_of_permits >= size - MAX_PEOPLE_ON_LECTURE ) {

            pthread_mutex_lock( &on_lecture_mutex ); // Process is on lecture -> locks on_lecture_mutex.
            pthread_mutex_unlock( &modify_permits ); // Permits are no longer needed -> unlocking modify_permits.

            desired_lectures[message->data] = 0;
            sleep(5000);
            pthread_mutex_unlock( &on_lecture_mutex );

            int lectures_left = 0; // Process will be counting how many lecture it has to visit.
            for ( int i = 1 ; i <= LECTURE_COUNT ; i++ ) {
                lectures_left += desired_lectures[i];
            }
            if ( !lectures_left ) { // If there are no lectures left
                pthread_mutex_unlock( &gtfo_mutex ); // something TODO idk wtf it is
            }
        } else {
            pthread_mutex_unlock( &modify_permits ); // Permits are no longer needed -> unlocking modify_permits.
        }
    }
}

/**
 * Function receives ALRIGHT_TO_ENTER message from other process and deals with it's content.
 * @param message packet_t received from other process.
 */
void alright_enter_handler ( packet_t * message ) {
    if( message->data == ENTER_PYRKON ){
        alright_enter_pyrkon_extension(message);
    } else {
        alright_enter_lecture_extension(message);
    } 
    free(message);
}

/**
 * Function receives EXIT message from other process and deals with it's content.
 * @param message - packet_t received from other process.
 */
void exit_handler ( packet_t * message ) {

    println( "Received info that [%d] has left Pyrkon.\n", message->src )

    pthread_mutex_lock( &modify_exited_from_pyrkon );
    exited_from_pyrkon++;
    pthread_mutex_unlock( &modify_exited_from_pyrkon );

    /* If everyone exited Pyrkon process can start another one. */
    if( exited_from_pyrkon == MAX_PEOPLE_ON_PYRKON ) {
        change_state(BEFORE_PYRKON);
        pthread_mutex_unlock( &wait_for_new_pyrkon );
    }
    free(message);
}