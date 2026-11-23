/*
    Test utility for Basin language test suite
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int total_tests = 0;
int passed_tests = 0;

void test_begin() {
    printf("TEST BEGIN\n");
}

void test_end() {

    if (total_tests == passed_tests) {
        printf("\033[32mTESTS PASSED\033[0m (%d/%d)\n", passed_tests, total_tests);
    } else {
        printf("\033[31mTESTS FAILED\033[0m (%d/%d)\n", passed_tests, total_tests);
    }

}

void test_int(int a, int b) {
    total_tests++;
    if (a == b) {
        passed_tests++;
        printf("\033[32mpassed\033[0m %d == %d\n", a, b);
    } else {
        printf("\033[31mFAILED\033[0m %d == %d\n", a, b);
    }
}


// void main() {
//     test_begin();
//     test_int(1, 1);
//     test_int(2, 2);
//     // test_int(2, 3);
//     test_end();
// }
