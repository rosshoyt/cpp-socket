/**
 * Ross Hoyt
 * CPSC 5042-01, Spring 2019
 * Homework - 4
 *
 * To compile on CS1, use:
 * g++ prime_server.cpp -o <name> -pthread -std=c++11
 * 
 * Pledge:
 * I have not received unauthorized aid on this assignment.
 * I understand the answers that I have submitted. The answers submitted have not
 * been directly copied from another source, but instead
 * are written in my own words.
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fstream>
#include <string>
#define PORT 6543
#define FILE_NAME "P.txt"
#define MAX_VALUE UINT32_MAX

// Shared variables
uint32_t nextUnseenNumber = 2; // start search from first prime number, 2
pthread_mutex_t numberLock, fileLock;
std::ofstream file;

// Functions
/**
 * Locks then writes to the shared .txt file.
 * @param str - the string to be written to the file
 */
void writeToFile(std::string str);
/**
 * Function which checks if a number is prime.
 * @param n - uint32_t whose primeness will be tested
 */
bool isPrime(uint32_t n);
/**
 * Function which copies the nextUnseenNumber, increments it in a Critical section,
 * @return the copy of nextUnseenNumber, or returns 0 once the maximum value has
 * been tested (indicating program completion)
 */
uint32_t getNextUnseenNumber();
/**
 * Multithread function used by a thread on server to find primes. Works alone or with
 * client threads to find prime numbers smaller than UINT32_MAX
 * @param param NULL
 */
void *findPrimes_Server(void *param);
/**
 * Multithread function which manages a client's search for primes. This passes
 * nextUnseenNumber to the client and, if prime, writes it to the shared file.
 * @param param NULL
 */
void *manageClientPrimeSearch(void * param);

/**
 * Server main entry point
 */
int main(int argc, char const *argv[])
{
    // Declare Threads & initialize mutex locks
    pthread_t tid_client, tid_search;
    pthread_mutex_init(&numberLock, NULL);
    pthread_mutex_init(&fileLock, NULL);
    
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
    
    // Create local search pthread
    pthread_create(&tid_search, NULL, findPrimes_Server, NULL);
    
    // Run server
    while(1 > 0)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address,
                                 (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        printf("New client connected...\n");
        pthread_create(&tid_client, NULL, manageClientPrimeSearch, (void *)new_socket);
        
        
    }
    file.close();
    return 0;
}

// Function definitions
bool isPrime(uint32_t n)
{
    if(n < 2) return false;
    uint32_t divisor = 1; // (value is incremented at beginning of 'do' loop,
                          // so first tested divisor is 2)
    // Set ceiling for largest value the prime needs to be divided by
    uint32_t maxTestVal = (n&1) ? n / 2 + 1: n / 2;
    // Value which indicates non-primeness when equal to 0
    uint32_t remainder;
    do
    {
        ++divisor;
        remainder = n % divisor;
    } while(divisor < maxTestVal && remainder != 0);
    // Number is prime if remainder is still != 0, or edge case of n == 2 is true.
    if(remainder != 0 || n == 2) return true;
    return false;
}
void writeToFile(std::string str)
{
    pthread_mutex_lock(&fileLock);
    file << str << "\n";
    pthread_mutex_unlock(&fileLock);
}
uint32_t getNextUnseenNumber()
{
    // First create copy of the number
    uint32_t n = nextUnseenNumber;
    // Increase nextUnseenNumber to next odd number
    if(nextUnseenNumber + 2 <= MAX_VALUE)
    {
        pthread_mutex_lock(&numberLock);
        if(nextUnseenNumber & 1) nextUnseenNumber += 2; // Tests for odd number
        else ++nextUnseenNumber; // Occurs when nextUnseenNumber is 2
        pthread_mutex_unlock(&numberLock);
    }
    else n = 0; // Sets the return flag of 0, which shows the program has
                // finished testing all numbers under MAX_VALUE
    return n;
}
void *findPrimes_Server(void *param)
{
    printf("Server starting search for prime numbers...\n");
    uint32_t testNumber = getNextUnseenNumber(); //setting to dummy value
    while(testNumber != 0)
    {
        // If number was prime, write to file
        if(isPrime(testNumber))
        {
            
            writeToFile(std::to_string(testNumber));
            printf("Server found prime %d\n",testNumber);
        }
        testNumber = getNextUnseenNumber();
    }
    return 0;
}
void *manageClientPrimeSearch(void *param)
{
    printf("Sending client integers to analyze\n");
    uint64_t new_socket;
    int valread;
    new_socket = (long)param;
    bool result;
    uint32_t testNumber = getNextUnseenNumber();
   
    // Loop which continuously sends clients number to test for primeness
    while(testNumber != 0)
    {
        uint32_t convertedNumber = htonl(testNumber);
        // Send client the number to test
        write(new_socket, &convertedNumber, sizeof(convertedNumber));
        // Read client's response
        valread = read(new_socket, &result, sizeof(result));
        // If tested number was prime, write to file
        if(result)
        {
            writeToFile(std::to_string(testNumber));
            printf("Client found prime %d\n",testNumber);
        }
        // Get next number for client to test
        testNumber = getNextUnseenNumber();
    }
    close(new_socket);
    pthread_exit(0);
}
