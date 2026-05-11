#ifndef MOSKEN_TEST_H
#define MOSKEN_TEST_H

#define TEST(name)                                                            \
    void test_##name()                                                        \
    {                                                                         \
        printf("TEST: %s... ", #name);
#define ASSERT(cond)                                                          \
    do                                                                        \
    {                                                                         \
        if(!(cond))                                                           \
        {                                                                     \
            printf("FAIL\n");                                                 \
            exit(1);                                                          \
        }                                                                     \
    }                                                                         \
    while(0)

#define END_TEST()                                                            \
    printf("PASS\n");                                                         \
    }

#define RUN_TEST(name) test_##name()

#endif // !MOSKEN_TEST_H
