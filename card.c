/*****************************************************************************
 * Copyright (C) 2010 Tadeus Prastowo (eus@member.fsf.org)                   *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation, either version 3 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include "card.h"

/** A card having a particular suit and rank. */
struct card_impl
{
  uint8_t card; /**<
		 * The first 3 bits are for suit while the last 5 bits are for
		 * rank.
		 */
/* The value of the following defines will never change. */
#define INVALID_CARD_BITS (0U)

#define SUIT_BITS (0x7U << 5)
#define SPADE_BITS (1U << 5)
#define HEART_BITS (2U << 5)
#define DIAMOND_BITS (3U << 5)
#define CLUB_BITS (4U << 5)

#define RANK_BITS (0x1FU)
#define ACE_BITS (1U)
#define R2_BITS (2U)
#define R3_BITS (3U)
#define R4_BITS (4U)
#define R5_BITS (5U)
#define R6_BITS (6U)
#define R7_BITS (7U)
#define R8_BITS (8U)
#define R9_BITS (9U)
#define R10_BITS (10U)
#define J_BITS (11U)
#define Q_BITS (12U)
#define K_BITS (13U)
};

struct card_collection;

/** A collection of cards. */
struct card_collection
{
  struct card_collection *prev; /**< The previous card in the collection. */
  struct card_collection *next; /**< The next card in the collection. */
  const card *c; /**< A card in a collection. */
};

/**
 * Inserts a card having the specified suit and rank into a collection.
 *
 * @param [in] head a pointer to the first element of the collection (this
 *                  can be NULL if the collection is still empty).
 * @param [in] c the card to be inserted into the collection.
 * @param [in] sorter a callback function that determines the placement of the
 *                    card in the collection.
 *
 * @return 0 if the addition is successful or non-zero if a new element cannot
 *         be created.
 */
static int
insert_into_collection (struct card_collection **head, const card *c,
			card_sorter sorter)
{
  struct card_collection *col;
  struct card_collection *itr;

  col = malloc (sizeof (*col));
  if (col == NULL)
    {
      destroy_card (&c);
      return 1;
    }

  col->c = c;

  if (*head == NULL)
    {
      col->prev = col;
      col->next = col;
      *head = col;

      return 0;
    }

  itr = *head;
  if (sorter (NULL, c, itr->c))
    {
      col->prev = itr->prev;
      col->next = itr;

      itr->prev = col;
      col->prev->next = col;

      *head = col;

      return 0;
    }
  while (itr->next != *head)
    {
      if (sorter (itr->c, c, itr->next->c))
	{
	  col->prev = itr;
	  col->next = itr->next;

	  itr->next = col;
	  col->next->prev = col;

	  return 0;
	}
      itr = itr->next;
    }
  if (sorter (itr->c, c, NULL))
    {
      col->prev = itr;
      col->next = *head;

      itr->next = col;
      col->next->prev = col;

      return 0;
    }

  return 0;
}

/**
 * Iterates a collection through the next pointer.
 *
 * @param [in] head the pointer to the first element in the collection. NULL
 *                  means an empty collection.
 * @param [in,out] itr the pointer by which the caller can operate on the
 *                     element under the iterator. The iteration state is
 *                     saved in this pointer. The pointer should first point to
 *                     NULL. You can point itr to the address of an element in
 *                     the collection to start the
 *                     iteration with the element <strong>after</strong> the
 *                     pointed element onward.
 *
 * @return zero if all elements has been iterated or non-zero if there are still
 *         some elements to be iterated.
 */
static int
iterate_collection (struct card_collection *head, struct card_collection **itr)
{
  if (head == NULL)
    {
      return 0;
    }

  if (*itr == NULL)
    {
      *itr = head;
    }
  else
    {
      *itr = (*itr)->next;
      if (*itr == head)
	{
	  *itr = NULL;
	}
    }

  return *itr != NULL;
}

/**
 * Appends a new entry into the end of the collection.
 *
 * @param [in] head a pointer to the head of the collection. It can point to
 *                  NULL if the collection is empty.
 * @param [in] new the new entry to be appended.
 */
static void
append_into_collection (struct card_collection **head, struct card_collection *new)
{
  if (*head == NULL)
    {
      new->prev = new;
      new->next = new;      
      *head = new;

      return;
    }

  new->prev = (*head)->prev;
  new->next = *head;
  new->prev->next = new;
  new->next->prev = new;
}

/**
 * Detach an entry from the collection.
 * This function is safe to be used during iteration because the detached entry
 * is not freed and its previous and next pointers are still valid although the
 * entry is no longer recognized as a part of the collection (i.e., subsequent
 * iteration on the collection will not iterate the detached entry).
 * It is your responsibility to free the detached entry as well as its payload.
 *
 * @param [in] entry the entry to be detached.
 */
static void
detach_from_collection (const struct card_collection *entry)
{
  entry->next->prev = entry->prev;
  entry->prev->next = entry->next;
}

/**
 * The callback function used to free the payload of an entry in the collection.
 *
 * @param [in] payload the pointer to the payload to be freed.
 */
typedef void (*payload_freer) (void **payload);

static void
destroy_card_payload (void **payload)
{
  destroy_card ((const card **) payload);
}

/**
 * Reclaims the memory space that was allocated for the entry as well as
 * setting the pointer to NULL as a safe guard. Passing a pointer to NULL is,
 * safe but not a NULL pointer. A function callback to free the payload should
 * be provided unless the payload should not be freed.
 *
 * @param [in] entry the pointer pointing to the entry to be freed.
 * @param [in] freer the callback function used to free the payload of an entry
 *                   or NULL if the payload should not be freed.
 */
static void
destroy_collection_entry (struct card_collection **entry, payload_freer freer)
{
  if (*entry == NULL)
    {
      return;
    }

  if (freer != NULL)
    {
      freer ((void *) &(*entry)->c);
    }
  free ((void *) *entry);

  *entry = NULL;
}

/**
 * Reclaims the memory space that was allocated for the entry as well as
 * setting the pointer to NULL as a safe guard. Passing a pointer to NULL is,
 * safe but not a NULL pointer. A function callback to free the payload should
 * be provided unless the payload should not be freed.
 *
 * @param [in] entry the pointer pointing to the entry to be freed.
 * @param [in] freer the callback function used to free the payload of the entry
 *                   or NULL if the payload should not be freed.
 */
static void
remove_from_collection (struct card_collection **entry, payload_freer freer)
{
  if (*entry == NULL)
    {
      return;
    }

  (*entry)->next->prev = (*entry)->prev;
  (*entry)->prev->next = (*entry)->next;

  destroy_collection_entry (entry, freer);
}

/**
 * Reclaims the memory space that was allocated for the collection as well as
 * setting the pointer to NULL as a safe guard. Passing a pointer to NULL is,
 * safe but not a NULL pointer. A function callback to free the payload should
 * be provided unless the payload should not be freed.
 *
 * @param [in] col_ptr the pointer pointing to the collection to be freed.
 * @param [in] freer the callback function used to free the payload of an entry
 *                   or NULL if the payload should not be freed.
 */
static void
destroy_collection (struct card_collection **col_ptr, payload_freer freer)
{
  struct card_collection *head = *col_ptr;
  struct card_collection *head_next;

  if (head == NULL)
    {
      return;
    }

  head->prev->next = NULL; /* break the ring */

  do
    {
      head_next = head->next;

      if (freer != NULL)
	{
	  freer ((void *) &head->c);
	}
      free (head);

      head = head_next;
    }
  while (head != NULL);

  *col_ptr = NULL;
}

/** A hand of cards. */
struct card_hand_impl
{
  uint8_t max; /**< The maximum number of cards at hand. */
  uint8_t len; /**< The current number of cards at hand. */
  card_sorter sorter; /**<
		       * The callback function used to determine the place of
		       * insertion of a new card.
		       */
  struct card_collection *cards; /**< The cards at hand. */
};

/** A deck of cards. */
struct card_deck_impl
{
  struct card_collection *dealt_cards; /**< Cards that have been dealt. */
  struct card_collection *cards; /**< The cards in deck. */
};

enum card_suit_rank
get_card_suit_rank (const card *c)
{
  enum card_suit_rank csr;
  enum card_suit cs = get_card_suit (c);
  enum card_rank cr = get_card_rank (c);

  if (cs == INVALID_SUIT || cr == INVALID_RANK)
    {
      return INVALID_CARD;
    }

  switch (cs)
    {
    case SPADE:
      csr = SPADE_ACE;
      break;
    case HEART:
      csr = HEART_ACE;
      break;
    case DIAMOND:
      csr = DIAMOND_ACE;
      break;
    case CLUB:
      csr = CLUB_ACE;
      break;
    }

  return csr + cr;
}

enum card_rank
get_card_rank (const card *c)
{
  uint8_t r = (c->card & RANK_BITS);

  if (r < ACE_BITS || r > K_BITS)
    {
      return INVALID_RANK;
    }

  return r - 1;
}

enum card_suit
get_card_suit (const card *c)
{
  uint8_t s = c->card & SUIT_BITS;

  if (s < SPADE_BITS || s > CLUB_BITS)
    {
      return INVALID_SUIT;
    }

  return (s >> 5) - 1;
}

const card *
create_card (enum card_suit_rank csr)
{
  card *c;

  c = malloc (sizeof (*c));

  if (c == NULL)
    {
      return NULL;
    }

  c->card = INVALID_CARD_BITS;

  if (csr >= SPADE_ACE && csr <= SPADE_K)
    {
      c->card |= SPADE_BITS;
    }
  else if (csr >= HEART_ACE && csr <= HEART_K)
    {
      c->card |= HEART_BITS;
    }
  else if (csr >= DIAMOND_ACE && csr <= DIAMOND_K)
    {
      c->card |= DIAMOND_BITS;
    }
  else if (csr >= CLUB_ACE && csr <= CLUB_K)
    {
      c->card |= CLUB_BITS;
    }

  switch (csr)
    {
    case SPADE_ACE:
    case HEART_ACE:
    case DIAMOND_ACE:
    case CLUB_ACE:
      c->card |= ACE_BITS;
      break;
    case SPADE_2:
    case HEART_2:
    case DIAMOND_2:
    case CLUB_2:
      c->card |= R2_BITS;
      break;
    case SPADE_3:
    case HEART_3:
    case DIAMOND_3:
    case CLUB_3:
      c->card |= R3_BITS;
      break;
    case SPADE_4:
    case HEART_4:
    case DIAMOND_4:
    case CLUB_4:
      c->card |= R4_BITS;
      break;
    case SPADE_5:
    case HEART_5:
    case DIAMOND_5:
    case CLUB_5:
      c->card |= R5_BITS;
      break;
    case SPADE_6:
    case HEART_6:
    case DIAMOND_6:
    case CLUB_6:
      c->card |= R6_BITS;
      break;
    case SPADE_7:
    case HEART_7:
    case DIAMOND_7:
    case CLUB_7:
      c->card |= R7_BITS;
      break;
    case SPADE_8:
    case HEART_8:
    case DIAMOND_8:
    case CLUB_8:
      c->card |= R8_BITS;
      break;
    case SPADE_9:
    case HEART_9:
    case DIAMOND_9:
    case CLUB_9:
      c->card |= R9_BITS;
      break;
    case SPADE_10:
    case HEART_10:
    case DIAMOND_10:
    case CLUB_10:
      c->card |= R10_BITS;
      break;
    case SPADE_J:
    case HEART_J:
    case DIAMOND_J:
    case CLUB_J:
      c->card |= J_BITS;
      break;
    case SPADE_Q:
    case HEART_Q:
    case DIAMOND_Q:
    case CLUB_Q:
      c->card |= Q_BITS;
      break;
    case SPADE_K:
    case HEART_K:
    case DIAMOND_K:
    case CLUB_K:
      c->card |= K_BITS;
      break;
    }

  if (c->card == INVALID_CARD_BITS)
    {
      free (c);
      return NULL;
    }

  return c;
}

const card *
strtocard (const char *str)
{
  enum card_suit_rank csr;

  switch (toupper (str[0]))
    {
    case 'S':
      csr = SPADE_ACE;
      break;
    case 'H':
      csr = HEART_ACE;
      break;
    case 'D':
      csr = DIAMOND_ACE;
      break;
    case 'C':
      csr = CLUB_ACE;
      break;
    }

  if (str[1] >= '2' && str[1] <= '9')
    {
      return create_card (csr + str[1] - '1');
    }

  switch (toupper (str[1]))
    {
    case 'A':
      return create_card (csr);
    case '1':
      return create_card (csr + 9);
    case 'J':
      return create_card (csr + 10);
    case 'Q':
      return create_card (csr + 11);
    case 'K':
      return create_card (csr + 12);
    }

  return NULL;
}

void
destroy_card (const card **c_ptr)
{
  if (*c_ptr == NULL)
    {
      return;
    }

  free ((void *) *c_ptr);
  *c_ptr = NULL;
}

enum card_rank
strtorank (const char *str)
{
  enum card_rank cr = ACE;

  if (str[0] >= '2' && str[0] <= '9')
    {
      cr += str[0] - '1';
    }
  else
    {
      switch (toupper (str[0]))
	{
	case 'A':
	  break;
	case '1':
	  cr += 9;
	  break;
	case 'J':
	  cr += 10;
	  break;
	case 'Q':
	  cr += 11;
	  break;
	case 'K':
	  cr += 12;
	  break;
	default:
	  return INVALID_RANK;
	}
    }

  return cr;
}

int
sort_card_after (const card *before, const card *new, const card *after)
{
  if (after == NULL)
    {
      return 1;
    }

  return 0;
}

int
sort_card_by_rank (const card *before, const card *new, const card *after)
{
  enum card_rank r = get_card_rank (new);

  if (after == NULL
      || ((before == NULL || r > get_card_rank (before))
	  && r <= get_card_rank (after)))
    {
      return 1;
    }

  return 0;
}

card_hand *
create_hand (unsigned char max, card_sorter sorter)
{
  struct card_hand_impl *h = malloc (sizeof (*h));

  if (h == NULL)
    {
      return NULL;
    }

  if (sorter == NULL)
    {
      sorter = sort_card_after;
    }

  h->max = max;
  h->len = 0;
  h->sorter = sorter;
  h->cards = NULL;

  return h;
}

void
reset_hand (card_hand *h)
{
  h->len = 0;

  destroy_collection (&h->cards, NULL);
}

void
insert_into_hand (card_hand *h, const card *c)
{
  if (h->max == h->len)
    {
      return;
    }

  insert_into_collection (&h->cards, c, h->sorter);
  h->len++;
}

unsigned long
count_cards_in_hand (const card_hand *h)
{
  return h->len;
}

unsigned long
get_max_of_hand (const card_hand *h)
{
  return h->max;
}

enum card_rank
get_max_rank_of_hand (const card_hand *h)
{
  struct card_collection *itr = NULL;
  enum card_rank cr = INVALID_RANK;

  if (h->len == 0)
    {
      return cr;
    }

  while (iterate_collection (h->cards, &itr))
    {
      enum card_rank this_cr = get_card_rank (itr->c);

      if (cr == INVALID_RANK)
	{
	  cr = this_cr;
	}
      else if (this_cr > cr)
	{
	  cr = this_cr;
	}
    }

  return cr;
}

void
iterate_hand (card_hand *h, void *arg, card_iterator itr_fn)
{
  struct card_collection *itr = NULL;
  unsigned long pos = 0;

  while (iterate_collection (h->cards, &itr))
    {
      if (itr_fn (arg, h, h->len, pos, itr->c))
	{
	  break;
	}
      pos++;
    }
}

void
remove_from_hand (card_hand *h, enum card_suit_rank c)
{
  struct card_collection *itr = NULL;
  struct card_collection *entry_to_remove = NULL;

  while (iterate_collection (h->cards, &itr))
    {
      if (get_card_suit_rank (itr->c) == c)
	{
	  detach_from_collection (itr);
	  entry_to_remove = itr;
	  itr = itr->prev;
	  destroy_collection_entry (&entry_to_remove, NULL);
	  h->len--;
	}
    }
}

void
destroy_hand (card_hand **h_ptr)
{
  if (*h_ptr == NULL)
    {
      return;
    }

  destroy_collection (&(*h_ptr)->cards, NULL);
  free ((void *) *h_ptr);

  *h_ptr = NULL;
}

int
is_card_in_deck (enum card_suit_rank c, const card_deck *d)
{
  struct card_collection *itr = NULL;

  while (iterate_collection (d->cards, &itr))
    {
      if (get_card_suit_rank (itr->c) == c)
	{
	  return 1;
	}
    }

  return 0;
}

const card *
deal_from_deck (card_deck *d)
{
  /* Dealing does not remove the card from the deck so that the user can use
   * the card without caring for memory management. The card will be freed
   * when the deck is destroyed.
   */
  struct card_collection *head = d->cards;

  if (head == NULL)
    {
      return NULL;
    }

  if (head == head->next) /* the last card */
    {
      d->cards = NULL;
    }
  else
    {
      d->cards = head->next;
      detach_from_collection (head);
    }
  append_into_collection (&d->dealt_cards, head);

  return head->c;
}

void
strip_card_from_deck (enum card_suit_rank c, card_deck *d)
{
  /* Since the user references the card not, free the card immediately. */
  struct card_collection *itr = NULL;

  while (iterate_collection (d->cards, &itr))
    {
      if (get_card_suit_rank (itr->c) == c)
	{
	  if (d->cards == itr)
	    {
	      d->cards = itr->next;
	    }
	  remove_from_collection (&itr, destroy_card_payload);
	  break;
	}
    }
}

card_deck *
create_shuffled_deck (void)
{
  /* 1. Creates a list of 52 cards.
   * 2. Draws a random number n from 1 to 52.
   * 3. Cross the n-th card from the list and insert the card at the bottom of
   *    the deck.
   * 4. Draws a random number n from 1 to 51.
   * 5. Cross the n-th uncrossed card from the list and insert the card at the
   *    bottom of the deck.
   * 6. Continue doing so until there is only 1 card left that simply is
   *    inserted at the bottom of the deck
   */
  struct card_deck_impl *deck;
  const card *c;
  long i;
  long card_left_count = CARD_COUNT;
  long next_card_idx;
  struct
  {
    enum card_suit_rank card;
    uint8_t is_withdrawn;
  }
  card_left [CARD_COUNT]; /* [1] */

  deck = malloc (sizeof (*deck));
  if (deck == NULL)
    {
      return NULL;
    }
  deck->cards = NULL;

  for (i = 0; i < CARD_COUNT; i++)
    {
      card_left[i].card = i;
      card_left[i].is_withdrawn = 0;
    }

  while (card_left_count > 0)
    {
      long card_idx = 0;

      next_card_idx = lrand48 () % card_left_count--;

      for (i = 0; i < CARD_COUNT; i++)
	{
	  if (card_left[i].is_withdrawn)
	    {
	      continue;
	    }

	  if (card_idx == next_card_idx)
	    {
	      break;
	    }

	  card_idx++;
	}

      card_left[i].is_withdrawn = 1; /* [2] */
      c = create_card (card_left[i].card);
      if (c == NULL)
	{
	  destroy_deck (&deck);
	  return NULL;
	}
      if (insert_into_collection (&deck->cards, c, sort_card_after)) /* [3] */
	{
	  destroy_deck (&deck);
	  return NULL;
	}
    }

  deck->dealt_cards = NULL;

  return deck;
}

void
destroy_deck (card_deck **d_ptr)
{
  if (*d_ptr == NULL)
    {
      return;
    }

  destroy_collection (&(*d_ptr)->cards, destroy_card_payload);
  destroy_collection (&(*d_ptr)->dealt_cards, destroy_card_payload);
  free (*d_ptr);
  *d_ptr = NULL;
}
