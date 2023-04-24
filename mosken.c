#include "mosken.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void page_init(Page page, Size size) {
  PgHeader p = (PgHeader)page;
  assert(size == BLKSZ);

  memset(p, 0, size);

  p->pgh_lower = size_of_page_header;
  p->pgh_upper = size;
}

Item page_item(Page page, PgItemId pgi_id) {
  return (Item)(((char *)page) + pgi_id->pgi_offset);
}

Payment payment(Page page, PgItemId pgi_id) {
  return (Payment)page_item(page, pgi_id);
}

PgItemId page_get_item_id(Page page, OffsetNum offset_number) {
  return &((PgHeader)page)->pgh_im[offset_number - 1];
}

void add_page_item(Page page, Item item, Size size, OffsetNum offset_number) {
  PgHeader p = (PgHeader)page;
  PgItemId item_id = page_get_item_id(page, offset_number);
  int upper, lower;

  upper = (int)p->pgh_upper - (int)size;
  lower = p->pgh_lower + sizeof(PageItemMeta);

  item_id_set(item_id, upper, size);

  memcpy((char *)page + upper, item, size);

  p->pgh_upper = (unsigned short)upper;
  p->pgh_lower = (unsigned short)lower;
}

void print_payment(Payment p) {
  printf("Payment { payment_id [%d] payment_name [%s] payment_time [%zu] "
         "total_cents [%zu] }\n",
         p->payment_id, p->payment_name, p->payment_time, p->total_cents);
}

int main(void) {
  PaymentData payment_obj = {.payment_id = 1,
                             .total_cents = 2000,
                             .payment_name = "Expensive Tuna",
                             .payment_time = 1682331745};
  Page page = malloc(BLKSZ);
  page_init(page, BLKSZ);
  OffsetNum offset_number = 1;
  add_page_item(page, (Item)&payment_obj, sizeof(PaymentData), offset_number);
  print_payment(payment(page, page_get_item_id(page, 1)));
  return EXIT_SUCCESS;
}
