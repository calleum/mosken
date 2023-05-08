#include <stdio.h>

enum
{
    DEFAULT = 01,
    SPECIAL_1 = 02,
    SPECIAL_2 = 04,
    SPECIAL_3 = 010,
};

/**
 * Integer to Boolean String Represenation
 *
 */
char *itobs(int s) {
    return s == 1 ? "True" : "False";
}

/**
 * @brief 
 * Check that the bits desired by the mask are set.
 *
 * @param f the target int to check the mask against.
 * @param m the bitmask to check.
 * @return 1 if the mask is set, 0 otherwise.
 */
int chkbit(int f, int m) {
    return (f & m) == m;
}

int
main(void)
{
    int flags = 0;

    /* Set the DEFAULT and SPECIAL_1 flags ON */
    flags |= (DEFAULT | SPECIAL_1);

    printf("flags = DEFAULT?[%s], SPECIAL_1?[%s], DEF_SPECIAL_1?[%s], SPECIAL_3?[%s]\n",
           itobs(chkbit(flags, DEFAULT)), itobs(chkbit(flags, SPECIAL_1)), itobs(chkbit(flags, (DEFAULT | SPECIAL_1))), itobs(chkbit(flags, SPECIAL_3)));

    return 0;
}
