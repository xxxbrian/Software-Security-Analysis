#include <stdio.h>
#include <stdlib.h> // For rand() and srand()
#include <time.h>   // For time()

// Helper function to print int values
void printIntLine(int value) {
    printf("%d\n", value);
}

void printLine(const char *message) {
    printf("%s\n", message);
}

void CWE121_Stack_Based_Buffer_Overflow__CWE129_rand_01_bad() {
    int data;
    /* Initialize data */
    data = -1;
    /* POTENTIAL FLAW: Set data to a random value */
    srand((unsigned)time(NULL)); // Seed the random number generator
    data = rand() % 100; // Simulating RAND32() to generate a value between 0 and 99

    int i;
    int buffer[10] = { 0 };
    /* POTENTIAL FLAW: Attempt to write to an index of the array that is above the upper bound
    * This code does check to see if the array index is negative */
    if (data >= 0 && data < 10) { // Adding condition to prevent out-of-bounds write
        buffer[data] = 1;
        // Simulating buffer access function by directly accessing the buffer
        printIntLine(buffer[data]);
    } else if (data >= 10) {
        printLine("ERROR: Array index is out of bounds.");
    } else {
        printLine("ERROR: Array index is negative.");
    }

    /* Print the array values */
    for(i = 0; i < 10; i++) {
        printIntLine(buffer[i]);
    }
}

int main() {
    CWE121_Stack_Based_Buffer_Overflow__CWE129_rand_01_bad();
    return 0;
}