#include "test.h"
#include "mosken.h"
#include <stdint.h>
#include <stdlib.h>

TEST(page_init_correct_values)
{
    Page page = malloc(BLKSZ);
    page_init(page);

    PgHeader p = (PgHeader)page;

    ASSERT(p->pgh_upper == BLKSZ);

    free(page);
}
END_TEST()

TEST(page_init_zeroed)
{
    Page page = malloc(BLKSZ);
    page_init(page);

    PgHeader p = (PgHeader)page;

    for(uint32_t i = p->pgh_lower; i < p->pgh_upper; i++)
    {
        ASSERT(page[i] == 0);
    }

    free(page);
}
END_TEST()

int
main(void)
{
    RUN_TEST(page_init_correct_values);
    RUN_TEST(page_init_zeroed);
    return EXIT_SUCCESS;
}
