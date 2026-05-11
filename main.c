#include "mosken.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

Payment
payment(Page page, PgItemId pgi_id)
{
    return (Payment)page_item(page, pgi_id);
}

void
print_payment(Payment p)
{
    printf("Payment { payment_id [%d] payment_name [%s] payment_time [%u] "
           "total_cents [%u] }\n",
           p->payment_id, p->payment_name, p->payment_time, p->total_cents);
}

int
main(void)
{
    PaymentData payment_obj = { .payment_id = 1,
                                .total_cents = 2000,
                                .payment_name = "Expensive Tuna",
                                .payment_time = 1682331745 };
    Page page = malloc(BLKSZ);
    page_init(page, BLKSZ);
    OffsetNum offset_number = 1;
    add_page_item(page, (Item)&payment_obj, sizeof(PaymentData),
                  offset_number);

    char *filename = "mosken.db";

    struct stat buffer;
    if(stat(filename, &buffer) != 0)
    {
        FILE *fp = fopen(filename, "w");
        fclose(fp);
    }

    FILE *fp = fopen(filename, "r+");
    if(fp == NULL)
    {
        fprintf(stderr, "Error opening file\n");
        exit(1);
    }

    write_page(fp, 0L, (char *)page);

    Page page_2 = malloc(BLKSZ);
    read_page(fp, 0L, page_2);

    print_payment(payment(page, page_get_item_id(page, 1)));

    print_payment(payment(page_2, page_get_item_id(page_2, 1)));
    free(page);
    free(page_2);
    return EXIT_SUCCESS;
}
