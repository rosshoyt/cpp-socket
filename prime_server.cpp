/**
 * Ross Hoyt
 * CPSC 5042-01, Spring 2019
 * Homework - 4 EXTRA CREDIT
 *
 * To compile on CS1, use:
 * g++ prime_server.cpp -o <name> -pthread -std=c++11
 * To run:
 * ./<name>
 * To run and show primes found in the console:
 * ./<name> show
 */

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <pthread.h>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#define PORT 6543
#define FILE_NAME "P.txt"
#define MAX_VALUE UINT32_MAX


/**
 * Locking queue class, used by method 'getNextUnseenNumber()'
 * Tracks numbers needing to be tested, but which got dropped by a client
 */
class ClientValueHolder
{
private:
    std::vector<uint32_t> numbersToBeTested; // tracks all numbers which were
    pthread_mutex_t client_val_lock;
public:
    ClientValueHolder() : numbersToBeTested()
    {
        pthread_mutex_init(&client_val_lock, NULL);
    }
    bool isEmpty()
    {
        return numbersToBeTested.empty();
    }
    void addNumber(uint32_t n)
    {
        pthread_mutex_lock(&client_val_lock);
        numbersToBeTested.push_back(n);
        pthread_mutex_unlock(&client_val_lock);
    }
    uint32_t getNumber()
    {
        uint32_t n;
        if(!isEmpty())
        {
            pthread_mutex_lock(&client_val_lock);
            n = *numbersToBeTested.begin(); // copy
            numbersToBeTested.erase(numbersToBeTested.begin()); //erase from list
            pthread_mutex_unlock(&client_val_lock);
        } else n = 0; // When no numbers to be tested, should not occur
        return n;
    }
};

// Method which suppresses error SIGPIPE
void sigpipe_handler(int signal)
{
    // Do nothing
}

// Commandline input vars
const char *SHOW_SEARCH_ARGUMENT = "show";
bool showSearch = false; // default to not show prime search results on console

// Shared vars
uint32_t nextUnseenNumber = 2; // start search from first prime number (2)
pthread_mutex_t numberLock, fileLock;
std::ofstream file;
static ClientValueHolder clientValueHolder; // helper which tracks values that were dropped

// Functions
bool isPrime(uint32_t n);
void writeToFile(std::string str);
uint32_t getNextUnseenNumber();
void *findPrimes_Server(void *param);
void *manageClientPrimeSearch(void *param);

/**
 * Server main entry point which accepts command line arguments
 * @param argv if "show" passed in as arg, program displays prime search on console
 */
int main(int argc, char const *argv[])
{
    printf("\nWelcome to the Prime Server Program!\n");
    // Check command line args
    if(argc > 1)
    {
        if(std::strcmp(argv[1], SHOW_SEARCH_ARGUMENT) == 0)
        {
            showSearch = true;
            printf("'Show primes discovered in console' = ON\n");
        }
    }
    else printf("'Show primes discovered in console' = OFF\n"
                "To enable this setting, pass in command 'show' when running.\n"
                );
    // Declare threads and locks
    pthread_t tid_client, tid_search;
    pthread_mutex_init(&numberLock, NULL);
    pthread_mutex_init(&fileLock, NULL);
    // Set signal settings (NEW)
    signal(SIGPIPE, &sigpipe_handler);
    // Socket vars
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    // Set sockaddr_in fields
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
    // Forcefully attach socket to the port 8080
    if (::bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    // Open the file, or create if not exists
    file.open(FILE_NAME);
    // Start local prime search thread
    pthread_create(&tid_search, NULL, findPrimes_Server, NULL);
    // Listen for client connections
    while(1 > 0)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address,
                                 (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        printf("New client connected.\n");
        pthread_create(&tid_client, NULL, manageClientPrimeSearch, (void *)new_socket);
    }
    return 0;
}

// Function definitions
/**
 * Checks if a number is prime.
 * @param n - uint32_t whose primeness will be tested
 */
bool isPrime(uint32_t n)
{
    for(size_t i = 2; i * i <= n; ++i) if (n % i == 0) return false;
    return true;
}
/**
 * Locks then writes to the shared .txt file.
 * @param str - the string to be written to the file
 */
void writeToFile(std::string str)
{
    pthread_mutex_lock(&fileLock);
    file << str << "\n";
    pthread_mutex_unlock(&fileLock);
}
/**
 * (UPDATED) Checks if numbers were dropped by other client; if so, returns one of them.
 * Otherwise, copies the nextUnseenNumber, then locks and increments it.
 * @return Copy of nextUnseenNumber, or returns 0 once the maximum value has
 * been tested (indicating program completion)
 */
uint32_t getNextUnseenNumber()
{
    uint32_t n;
    // First make sure no numbers need to be tested from other broken connections
    if(clientValueHolder.isEmpty())
    {
        // First copy of number
        n = nextUnseenNumber;
        // Check if number is less than maximum test value before incrementing
        if(nextUnseenNumber + 2 <= MAX_VALUE)
        {
            // Critical Section
            pthread_mutex_lock(&numberLock);
            // Increase nextUnseenNumber to next odd number
            if(nextUnseenNumber & 1) nextUnseenNumber += 2; // Tests if odd number
            else ++nextUnseenNumber; // Occurs when nextUnseenNumber is 2, at start
            pthread_mutex_unlock(&numberLock);
        }
        else n = 0; // Sets the return flag of 0, which shows the program has
        // finished testing all numbers under MAX_VALUE
    }
    else
    {
        n = clientValueHolder.getNumber(); // There was a value dropped by other connection
        printf("Dropped Value %d being calculated by another thread\n", n);
    }
    return n;
}
/**
 * Multithread function used by a thread on server to find primes. Works alone,
 * or with client threads to find prime numbers smaller than UINT32_MAX
 * @param NULL
 */
void *findPrimes_Server(void *param)
{
    printf("Starting server's search for prime numbers...\n");
    uint32_t testNumber = getNextUnseenNumber(); //setting to dummy value
    while(testNumber != 0)
    {
        // If number was prime, write to file
        if(isPrime(testNumber))
        {
            writeToFile(std::to_string(testNumber));
            if(showSearch) printf("Server found prime %d\n",testNumber);
        }
        // Get next number for client to test
        testNumber = getNextUnseenNumber();
    }
    return 0;
}
/**
 * Multithread function which manages a client's search for primes. This passes
 * nextUnseenNumber to the client and, if prime, writes it to the shared file.
 * @param uint64_t client socket address
 */
void *manageClientPrimeSearch(void *param)
{
    printf("Sending client numbers to analyze...\n");
    
    uint32_t testNumber = getNextUnseenNumber();
    uint32_t convertedNumber = htonl(testNumber);
    uint64_t new_socket;
    //int valread;
    new_socket = (long)param;
    //bool result;
    
    
    /* Loop which continuously sends clients number to test for primality
     (Loop ends when getNextUnseenNumber() returns 0, its flag showing max val reached) */
    while(testNumber != 0)
    {
        int result = 0; //local variable which holds read and write results
        int count = 0;
        do
        {
            if(count != 0) // write error has occured
            {
                // add value for other cleints/server to test
                printf("Problem trying to contact client, sleeping 1 second then asking again\n");
                clientValueHolder.addNumber(testNumber); // add number to
                sleep(1);
            }
            // Get next number
            testNumber = getNextUnseenNumber();
            uint32_t convertedNumber = htonl(testNumber);
            // Send client the number to test
            result = write(new_socket, &convertedNumber, sizeof(convertedNumber));
            ++count;
        } while(result == -1 && count < 3);
        //printf("here\n");
        //Proceed normally if client was able to be contacted
        if(result != -1)
        {
            bool isPrime;
            // Read client's response
            result = read(new_socket, &isPrime, sizeof(result));
            // If tested number was prime, write to file
            if(isPrime)
            {
                writeToFile(std::to_string(testNumber));
                if(showSearch) printf("Client found prime %d\n",testNumber);
            }
            // Get next number for client to test
            testNumber = getNextUnseenNumber();
        }
        else // Client did not respond, delete thread
        {
            testNumber = 0; // Sets the flag to exit loop and close socket,thread
            printf("Client not responding, closing connection. Prime Search Continues...");
            close(new_socket);
            pthread_exit(0);
        }
    }
    printf("Closing connection... Prime search continues");
    close(new_socket);
    pthread_exit(0);
}

