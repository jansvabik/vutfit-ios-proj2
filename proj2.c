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

#define REMOVE_SEMAPHORES_COMMAND "rm -f /dev/shm/sem.xsvabi00.ios.proj2.* /dev/shm/xsvabi00.ios.proj2.*"
#define OUTPUT_FILE "proj2.out"

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
    int peopleInGroup;
    int maxHackersDelay;
    int maxSurfersDelay;
    int maxCruiseTime;
    int maxBackToPier;
    int maxPierCapacity;
} data_t;

// shared variables to map
pier_t* pier = NULL;
int* actionCounter = NULL;
data_t* pdata = NULL;
FILE* file = NULL;

// semaphore storing
sem_list_t* sems = NULL;

/**
 * @brief Starting init
 * Inits the pier and the semaphores. Sets the starting values.
 * @param sems ukazatel na strukturu semaforu
 * @return true kdyz nenastane zadna chyba
 * @return false pokud se nepodari otevrit/vytvorit semafor
 */
bool init() {
    // create the action counter
    MMAP(actionCounter);
    *actionCounter = 0;

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
    UNMAP(pdata);
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
        // there was a matching group, start the journey
        sem_wait(sems->journey);
        sem_wait(sems->printing); // to the captain boarding

        sem_post(sems->hackerQueue);
        sem_post(sems->hackerQueue);
        sem_post(sems->hackerQueue);
        sem_post(sems->hackerQueue);
        pier->hackers -= 4;
    }
    else if (pier->surfers >= 4) {
        // there was a matching group, start the journey
        sem_wait(sems->journey);
        sem_wait(sems->printing); // to the captain boarding

        sem_post(sems->surferQueue);
        sem_post(sems->surferQueue);
        sem_post(sems->surferQueue);
        sem_post(sems->surferQueue);
        pier->surfers -= 4;
    }
    else if (pier->surfers >= 2 && pier->hackers >= 2) {
        // there was a matching group, start the journey
        sem_wait(sems->journey);
        sem_wait(sems->printing); // to the captain boarding

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

    return true;
}

/**
 * @brief One person (process) life
 * It starts, goes to the pier, boards the boat and dies...
 * @param type type of the person (HACKER/SURFER)
 * @param id ID of the person
 * @param pdata program variables (e.g. for timing data)
 */
void personLife(int type, int id, FILE* file) {
    // ******** THE LIFE BEGINS ******** //

    sem_wait(sems->printing);
    fprintf(file, "%d\t\t: %s %d\t\tstarts\n", ++*actionCounter, ptn[type], id);
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
            sem_wait(sems->printing);
            fprintf(file, "%d\t\t: %s %d\t\tleaves queue\t: %d\t\t: %d\n", ++*actionCounter, ptn[type], id, pier->hackers, pier->surfers);
            sem_post(sems->printing);
        }
        else {
            // yes, decrement the capacity and go to pier
            sem_wait(sems->pierFreeCapacity);

            sem_wait(sems->groupManipulation);
            addToPier(type);
            sem_post(sems->groupManipulation);

            sem_wait(sems->printing);
            fprintf(file, "%d\t\t: %s %d\t\twaits\t\t\t: %d\t\t: %d\n", ++*actionCounter, ptn[type], id, pier->hackers, pier->surfers);
            sem_post(sems->printing);
            
            // pierchecking ends
            sem_post(sems->piertesting);
            break;
        }

        // another process can now test pier capacity
        sem_post(sems->piertesting);

        // random waiting time to check the pier capacity again
        usleep(rand() % (pdata->maxBackToPier >= 20 ? pdata->maxBackToPier : 20) * 1000);

        sem_wait(sems->printing);
        fprintf(file, "%d\t\t: %s %d\t\tis back\n", ++*actionCounter, ptn[type], id);
        sem_post(sems->printing);
    }

    // ******** PIER WAS REACHED SUCCESSFULLY ******** //

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
        fprintf(file, "%d\t\t: %s %d\t\tboards\t\t\t: %d\t\t: %d\n", ++*actionCounter, ptn[type], id, pier->hackers, pier->surfers);
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
        fprintf(file, "%d\t\t: %s %d\t\tmember exits\t: %d\t\t: %d\n", ++*actionCounter, ptn[type], id, pier->hackers, pier->surfers);
        sem_post(sems->printing);

        // I am out! The captain could get out if I am the last!
        sem_post(sems->captainWaits);
    }
    else {
        // captain exits the boat
        sem_wait(sems->printing);
        fprintf(file, "%d\t\t: %s %d\t\tcaptain exits\t: %d\t\t: %d\n", ++*actionCounter, ptn[type], id, pier->hackers, pier->surfers);
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
void generateProcess(int type, FILE* file) {
    for (int i = 0; i < pdata->peopleInGroup; i++) {
        pid_t pid = fork();
        if (pid == 0)
            personLife(type, i+1, file);
        else if (pid < 0) {
            fprintf(stderr, "Error when trying to fork the generating process.\n");
            exit(EXIT_FAILURE);
        }

        // wait until new process should be generated
        if (type == HACKER && pdata->maxHackersDelay > 0)
            usleep(rand() % pdata->maxHackersDelay * 1000);
        else if (pdata->maxSurfersDelay > 0)
            usleep(rand() % pdata->maxSurfersDelay * 1000);
    }

    // wait for all the processes generated
    for (int i = 0; i < pdata->peopleInGroup*2; i++)
        wait(NULL);
    
    // end this process
    exit(0);
}

/**
 * @brief Check and extract the arguments
 * Check the format of program arguments given and extract them if there is no problem.
 * @param argc The number of program arguments
 * @param argv The array of program arguments
 * @param pdata Structure where to store the program arguments
 * @return true if there is no problem with the arguments
 * @return false if there is a problem with the arguments
 */
bool checkArguments(int argc, char* argv[]) {
    // problem counter
    int problems = 0;

    // check the number of arguments
    if (argc != 7)
        fprintf(stderr, "Problem %d: Wrong number of arguments. You should give me exactly 6 arguments.\n", ++problems);

    // * EXTRACT THE VALUE FROM THE PROGRAM ARGUMENTS AND CHECK THEM
    char* pattern;

    // people in group
    pdata->peopleInGroup = strtol(argv[1], &pattern, 10);
    if (strlen(pattern) > 0 || pdata->peopleInGroup < 2 || pdata->peopleInGroup % 2 != 0)
        fprintf(stderr, "Problem %d: The first argument (people in group) has to be number greater or equal to 2 and peopleInGroup %% 2 has to be 0.\n", ++problems);

    // max hackers delay
    pdata->maxHackersDelay = strtol(argv[2], &pattern, 10);
    if (strlen(pattern) > 0 || pdata->maxHackersDelay < 0 || pdata->maxHackersDelay > 2000)
        fprintf(stderr, "Problem %d: The second argument (max hackers delay) has to be number greater or equal to 0 and lower or equal to 2000.\n", ++problems);

    // max surfers delay
    pdata->maxSurfersDelay = strtol(argv[3], &pattern, 10);
    if (strlen(pattern) > 0 || pdata->maxSurfersDelay < 0 || pdata->maxSurfersDelay > 2000)
        fprintf(stderr, "Problem %d: The third argument (max surfers delay) has to be number greater or equal to 0 and lower or equal to 2000.\n", ++problems);

    // max surfers delay
    pdata->maxCruiseTime = strtol(argv[4], &pattern, 10);
    if (strlen(pattern) > 0 || pdata->maxCruiseTime < 0 || pdata->maxCruiseTime > 2000)
        fprintf(stderr, "Problem %d: The fourth argument (max cruise time) has to be number greater or equal to 0 and lower or equal to 2000.\n", ++problems);

    // max pier waiting checking time
    pdata->maxBackToPier = strtol(argv[5], &pattern, 10);
    if (strlen(pattern) > 0 || pdata->maxBackToPier < 20 || pdata->maxBackToPier > 2000)
        fprintf(stderr, "Problem %d: The fifth argument (max pier waiting checking time) has to be number greater or equal to 20 and lower or equal to 2000.\n", ++problems);

    // max pier waiting checking time
    pdata->maxPierCapacity = strtol(argv[6], &pattern, 10);
    if (strlen(pattern) > 0 || pdata->maxPierCapacity < 5)
        fprintf(stderr, "Problem %d: The sixth argument (max pier capacity) has to be number greater or equal to 5.\n", ++problems);

    if (problems == 0)
        return true;
    else
        return false;
}

/**
 * @brief Deinitialization
 * Deallocates the memory allocated, closes and unlinks the semaphores and closes the output file.
 */
void deinit() {
    clear();
    fclose(file);
}

// just the program entry point...
int main(int argc, char* argv[]) {
    // do the deinitialization when program ends unexpectedly
    signal(SIGTERM, deinit);
    signal(SIGINT, deinit);

    // extract and check the arguments
    MMAP(pdata);
    if (!checkArguments(argc, argv))
        return EXIT_FAILURE;

    // initialize the random number generator
    srand(time(NULL));

    // prepare the file
    file = fopen(OUTPUT_FILE, "w+");
    if (file == NULL) {
        fprintf(stderr, "Cannot open the output file.\n");
        return EXIT_FAILURE;
    }

    // remove file buffer to ensure that only one process really write data into the file
    setbuf(file, NULL);
    // setvbuf(file, NULL, _IONBF, 0);

    // create the semaphores and set them up to the first values
    if (!init()) {
        fprintf(stderr, "Cannot initialize semaphores and map the shared variables. Try to remove the semaphores by using the '%s' command\n", REMOVE_SEMAPHORES_COMMAND);
        return EXIT_FAILURE;
    }

    // generate the surfers
    pid_t pidSurfers = fork();
    if (pidSurfers == 0) {
        generateProcess(SURFER, file);
    }
    else if (pidSurfers < 0) {
        fprintf(stderr, "There was a problem when trying to fork the main process.\n");
        return EXIT_FAILURE;
    }

    // generate the hackers
    pid_t pidHackers = fork();
    if (pidHackers == 0) {
        generateProcess(HACKER, file);
    }
    else if (pidHackers < 0) {
        fprintf(stderr, "There was a problem when trying to fork the main process\n");
        return EXIT_FAILURE;
    }

    // wait for the generators
    wait(NULL); // surfers/hackers
    wait(NULL); // the second group

    // deallocate the memory and close and remove the semaphores
    deinit();

    return EXIT_SUCCESS;
}
