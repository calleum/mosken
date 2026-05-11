#include "test.h"
#include "mosken.h"
#include <stdlib.h>

TEST(page_init_correct_values)
{
    Page page = malloc(BLKSZ);
    page_init(page, BLKSZ);

    PgHeader p = (PgHeader)page;

    ASSERT(p->pgh_upper == BLKSZ);

    free(page);
}
END_TEST()

int
main(void)
{
    RUN_TEST(page_init_correct_values);
    return EXIT_SUCCESS;
}
