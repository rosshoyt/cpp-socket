/**
 * Ross Hoyt
 * CPSC 5042-01, Spring 2019
 * Homework - 4
 * To compile on CS1, use:
 * g++ prime_client.cpp -o <name> -pthread -std=c++11
 *
 * I have not received unauthorized aid on this assignment.
 * I understand the answers that I have submitted. The answers submitted have not
 * been directly copied from another source, but instead
 * are written in my own words.
 */

#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#define PORT 6543

/**
 * Function which checks if a number is prime.
 * @param n - uint32_t whose primeness will be tested
 */
bool isPrime(uint32_t n);

/**
 * Client main entry point
 */
int main(int argc, char const *argv[])
{
    // Socket vars
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }

    uint64_t return_status;
    while(1 > 0)
    {
        uint32_t testNumber;
        if (recv(sock, (void *)&testNumber, sizeof(uint32_t), 0) < (int)sizeof(uint32_t))
        {		
            printf("Problem receiving number from server, exiting\n");
            return -1;
        }
        testNumber = ntohl(testNumber);
        bool result = isPrime(testNumber);
        
        
        const char* pBytesOfResult = (const char*)&result;

        int lengthOfBytes = sizeof(result);

        return_status = write(sock, pBytesOfResult, lengthOfBytes);
    }

    send(sock, "", 0, 0);
    close(sock);
    return 0;
}

// Function definition
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
    if(remainder != 0 || n == 2)
    {
        printf("Prime found %d\n", n);
        return true;
    }
    return false;
}
