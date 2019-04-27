/**
 * @file proj2.c
 * @author Jan Svabik (xsvabi00@stud.fit.vutbr.cz)
 * @brief The second IOS project
 * @version 1.0
 * @date 2019-04-28
 * 
 * ! IF THE SEMAPHORE DELETION IS NEEDED !
 * SEMAPHORE NAMES: xsvabi00.ios.proj2.*.sem
 * IN THE /dev/shm DIRECTORY USUALLY: sem.xsvabi00.ios.proj2.*.sem
 * rm -f /dev/shm/sem.xsvabi00.ios.proj2.* /dev/shm/xsvabi00.ios.proj2.*
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>

// define the semaphore names
#define SEM_PIERFREECAPACITY "xsvabi00.ios.proj2.pierfreecapacity.sem"
#define SEM_PRINTING "xsvabi00.ios.proj2.printing.sem"
#define SEM_PIERTESTING "xsvabi00.ios.proj2.piertesting.sem"
#define SEM_HACKERQUEUE "xsvabi00.ios.proj2.hackerqueue.sem"
#define SEM_SURFERQUEUE "xsvabi00.ios.proj2.surferqueue.sem"
#define SEM_GROUPMANIPULATION "xsvabi00.ios.proj2.groupmanipulation.sem"
#define SEM_BOATISBACK "xsvabi00.ios.proj2.boatisback.sem"
#define SEM_JOURNEY "xsvabi00.ios.proj2.journey.sem"
#define SEM_CAPTAINWAITS "xsvabi00.ios.proj2.captainwaits.sem"

// simpler mapping
#define MMAP(pointer) {(pointer) = mmap(NULL, sizeof(*(pointer)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);}
#define UNMAP(pointer) {munmap((pointer), sizeof(*(pointer)));}

// simpler semaphore creating and destroying
#define OSEM(pointer, name, startValue) {(pointer) = sem_open((name), O_CREAT | O_EXCL, 0666, (startValue));}
#define CSEM(pointer, name) {sem_close((pointer)); sem_unlink((name));}

// person types
enum personType {HACKER, SURFER};
static char* ptn[2] = {"HACK", "SERF"};

/**
 * @brief Semaphore structure
 * Includes pointer to all semaphores used.
 */
typedef struct semafory {
    sem_t* pierFreeCapacity;
    sem_t* printing;
    sem_t* piertesting;
    sem_t* hackerQueue;
    sem_t* surferQueue;
    sem_t* groupManipulation;
    sem_t* boatIsBack;
    sem_t* journey;
    sem_t* captainWaits;
} sem_list_t;

/**
 * @brief The pier structure
 * Includes the capacity of the boat and the array of people on board.
 */
typedef struct pier {
    int hackers;
    int surfers;
} pier_t;

/**
 * @brief Program data
 * Structure with the program arguments for simpler access.
 */
typedef struct data {
    unsigned peopleInGroup;
    unsigned maxHackersDelay;
    unsigned maxSurfersDelay;
    unsigned maxCruiseTime;
    unsigned maxBackToPier;
    unsigned maxPierCapacity;
} data_t;

// shared variables to map
pier_t* pier = NULL;
int* actionCounter = NULL;

// semaphore storing
sem_list_t* sems = NULL;

/**
 * @brief Starting init
 * Inits the pier and the semaphores. Sets the starting values.
 * @param sems ukazatel na strukturu semaforu
 * @return true kdyz nenastane zadna chyba
 * @return false pokud se nepodari otevrit/vytvorit semafor
 */
bool init(data_t* pdata) {
    // pier init
    MMAP(pier);
    pier->hackers = pier->surfers = 0;

    // semaphores init
    MMAP(sems);
    OSEM(sems->pierFreeCapacity, SEM_PIERFREECAPACITY, pdata->maxPierCapacity);
    OSEM(sems->printing, SEM_PRINTING, 1);
    OSEM(sems->piertesting, SEM_PIERTESTING, 1);
    OSEM(sems->hackerQueue, SEM_HACKERQUEUE, 0);
    OSEM(sems->surferQueue, SEM_SURFERQUEUE, 0);
    OSEM(sems->groupManipulation, SEM_GROUPMANIPULATION, 1);
    OSEM(sems->boatIsBack, SEM_BOATISBACK, 0);
    OSEM(sems->journey, SEM_JOURNEY, 1);
    OSEM(sems->captainWaits, SEM_CAPTAINWAITS, 0);

    // semaphore error handling
    if (
        sems->pierFreeCapacity == SEM_FAILED ||
        sems->printing == SEM_FAILED ||
        sems->piertesting == SEM_FAILED ||
        sems->hackerQueue == SEM_FAILED ||
        sems->surferQueue == SEM_FAILED ||
        sems->groupManipulation == SEM_FAILED ||
        sems->boatIsBack == SEM_FAILED ||
        sems->journey == SEM_FAILED ||
        sems->captainWaits == SEM_FAILED
    ) return false;

    // no error there
    return true;
}

/**
 * @brief Unmap sh. vars and free mem.
 * Close and unlink semaphores, unmap all shared variables and free all dynamically allocated memory.
 */
void clear() {
    // semaphores
    CSEM(sems->pierFreeCapacity, SEM_PIERFREECAPACITY);
    CSEM(sems->printing, SEM_PRINTING);
    CSEM(sems->piertesting, SEM_PIERTESTING);
    CSEM(sems->hackerQueue, SEM_HACKERQUEUE);
    CSEM(sems->surferQueue, SEM_SURFERQUEUE);
    CSEM(sems->groupManipulation, SEM_GROUPMANIPULATION);
    CSEM(sems->boatIsBack, SEM_BOATISBACK);
    CSEM(sems->journey, SEM_JOURNEY);
    CSEM(sems->captainWaits, SEM_CAPTAINWAITS);

    // shared variables
    UNMAP(sems);
    UNMAP(pier);
    UNMAP(actionCounter);
}

/**
 * @brief Add type to pier
 * Adds person of specific type to the pier.
 * @param type 
 */
void addToPier(int type) {
    if (type == HACKER)
        ++pier->hackers;
    else
        ++pier->surfers;
}

/**
 * @brief Check for group
 * Checks for groups that might be created from the people on the pier. Starts the journey.
 * @return true if there is a matching group
 * @return false if there is not any matching group
 */
bool checkForGroup() {
    if (pier->hackers >= 4) {
        sem_post(sems->hackerQueue);
        sem_post(sems->hackerQueue);
        sem_post(sems->hackerQueue);
        sem_post(sems->hackerQueue);
        pier->hackers -= 4;
    }
    else if (pier->surfers >= 4) {
        sem_post(sems->surferQueue);
        sem_post(sems->surferQueue);
        sem_post(sems->surferQueue);
        sem_post(sems->surferQueue);
        pier->surfers -= 4;
    }
    else if (pier->surfers >= 2 && pier->hackers >= 2) {
        sem_post(sems->hackerQueue);
        sem_post(sems->hackerQueue);
        sem_post(sems->surferQueue);
        sem_post(sems->surferQueue);
        pier->hackers -= 2;
        pier->surfers -= 2;
    }
    else {
        // no matching group on the pier
        return false;
    }

    // there was a matching group, start the journey
    sem_wait(sems->journey);
    sem_wait(sems->printing); // to the captain boarding

    return true;
}

/**
 * @brief One person (process) life
 * It starts, goes to the molo, boards the boat and dies...
 * @param type type of the person (HACKER/SURFER)
 * @param id ID of the person
 * @param pdata program variables (e.g. for timing data)
 */
void zivot(int type, int id, data_t* pdata) {
    // ******** THE LIFE BEGINS ******** //

    sem_wait(sems->printing);
    printf("%d:\t%s %d\t\tstarts\n", ++*actionCounter, ptn[type], id);
    sem_post(sems->printing);

    // ******** GOING TO THE PIER ******** //

    int sem_test = -1;
    while (true) {
        // now will be checked that this process can go to the pier
        sem_wait(sems->piertesting);

        // is there a free capacity on the pier?
        sem_getvalue(sems->pierFreeCapacity, &sem_test);
        if (sem_test <= 0) {
            // no, try it again after some time
            printf("%d:\t%s %d\t\tleaves queue\t: %d\t: %d\n", ++*actionCounter, ptn[type], id, pier->hackers, pier->surfers);
        }
        else {
            // yes, decrement the capacity and go to pier
            sem_wait(sems->pierFreeCapacity);
            sem_wait(sems->groupManipulation);
            addToPier(type);
            sem_post(sems->groupManipulation);
            printf("%d:\t%s %d\t\twaits\t\t: %d\t: %d\n", ++*actionCounter, ptn[type], id, pier->hackers, pier->surfers);
            
            // pierchecking ends
            sem_post(sems->piertesting);
            break;
        }

        // another process can now test pier capacity
        sem_post(sems->piertesting);

        // random waiting time to check the pier capacity again
        usleep(rand() % (pdata->maxBackToPier >= 20 ? pdata->maxBackToPier : 20) * 1000);
        printf("%d:\t%s %d\t\tis back\n", ++*actionCounter, ptn[type], id);
    }

    // ******** MOLO WAS REACHED SUCCESSFULLY ******** //

    sem_wait(sems->groupManipulation);
    bool iAmCaptain = checkForGroup();
    sem_post(sems->groupManipulation);

    // wait for its own semaphore
    if (type == HACKER)
        sem_wait(sems->hackerQueue);
    else
        sem_wait(sems->surferQueue);

    // ******** THE JOURNEY ******** //

    // if not captain, wait until boat will be back
    if (!iAmCaptain) {
        sem_wait(sems->boatIsBack);
    }
    // if captain, do the pier changes and the journey
    else {
        // free the pier capacity
        sem_post(sems->pierFreeCapacity);
        sem_post(sems->pierFreeCapacity);
        sem_post(sems->pierFreeCapacity);
        sem_post(sems->pierFreeCapacity);
        
        // print the captain's boarding
        printf("%d:\t%s %d\t\tboards\t\t: %d\t: %d\n", ++*actionCounter, ptn[type], id, pier->hackers, pier->surfers);
        sem_post(sems->printing);

        // do the journey
        if (pdata->maxCruiseTime > 0)
            usleep(rand() % pdata->maxCruiseTime * 1000);

        // unlock the other three waiting processes
        sem_post(sems->boatIsBack);
        sem_post(sems->boatIsBack);
        sem_post(sems->boatIsBack);

        // wait until the other processes won't be out of the boat
        sem_wait(sems->captainWaits);
        sem_wait(sems->captainWaits);
        sem_wait(sems->captainWaits);
    }

    // ******** GET OUT FROM THE BOAT! ******** //

    if (!iAmCaptain) {
        // exit the boat
        sem_wait(sems->printing);
        printf("%d:\t%s %d\t\tmember exits\t: %d\t: %d\n", ++*actionCounter, ptn[type], id, pier->hackers, pier->surfers);
        sem_post(sems->printing);

        // I am out! The captain could get out if I am the last!
        sem_post(sems->captainWaits);
    }
    else {
        // captain exits the boat
        sem_wait(sems->printing);
        printf("%d:\t%s %d\t\tcaptain exits\t: %d\t: %d\n", ++*actionCounter, ptn[type], id, pier->hackers, pier->surfers);
        sem_post(sems->printing);

        // the journey ends...
        sem_post(sems->journey);
    }

    // ... and the life too.
    exit(0);
}

/**
 * @brief Generate people
 * Generate the specified amount of people of a specific type (HACKER/SURFER)
 * @param type the type of the generated people (HACKER/SURFER)
 * @param pdata Program data (arguments) (especially for timing data)
 */
void generateProcess(int type, data_t* pdata) {
    for (unsigned i = 0; i < pdata->peopleInGroup; i++) {
        pid_t pid = fork();
        if (pid == 0)
            zivot(type, i+1, pdata);
        else if (pid < 0)
            fprintf(stderr, "Chyba pri forkovani\n");

        // wait until new process should be generated
        if (type == HACKER && pdata->maxHackersDelay > 0)
            usleep(rand() % pdata->maxHackersDelay * 1000);
        else if (pdata->maxSurfersDelay > 0)
            usleep(rand() % pdata->maxSurfersDelay * 1000);
    }

    // wait for all the processes generated
    for (unsigned i = 0; i < pdata->peopleInGroup*2; i++)
        wait(NULL);
    
    // end this process
    exit(0);
}

// just the program entry point...
int main(int argc, char* argv[]) {
    // initialize the random number generator
    srand(time(NULL));

    if (argc != 7) {
        printf("Wrong number of arguments!?\n");
        return EXIT_FAILURE;
    }
    
    // load the data from the program arguments
    data_t* pdata = malloc(sizeof(data_t));
    pdata->peopleInGroup = strtol(argv[1], NULL, 10);
    pdata->maxHackersDelay = strtol(argv[2], NULL, 10);
    pdata->maxSurfersDelay = strtol(argv[3], NULL, 10);
    pdata->maxCruiseTime = strtol(argv[4], NULL, 10);
    pdata->maxBackToPier = strtol(argv[5], NULL, 10);
    pdata->maxPierCapacity = strtol(argv[6], NULL, 10);

    // create the action counter
    MMAP(actionCounter);
    *actionCounter = 0;

    // create the semaphores and set them up to the first values
    if (!init(pdata)) {
        fprintf(stderr, "Nepovedlo se!\n");
        return EXIT_FAILURE;
    }

    // generate the surfers
    pid_t pidSurfers = fork();
    if (pidSurfers == 0) {
        generateProcess(SURFER, pdata);
    }
    else if (pidSurfers < 0) {
        fprintf(stderr, "Chyba pri forkovani.\n");
        return EXIT_FAILURE;
    }

    // generate the hackers
    pid_t pidHackers = fork();
    if (pidHackers == 0) {
        generateProcess(HACKER, pdata);
    }
    else if (pidHackers < 0) {
        fprintf(stderr, "Chyba pri forkovani hackeru\n");
        return EXIT_FAILURE;
    }

    // wait for the generators
    wait(NULL); // surfers/hackers
    wait(NULL); // the second group

    // deallocate the memory and close and remove the semaphores
    free(pdata);
    clear();

    return EXIT_SUCCESS;
}
