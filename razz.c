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

#include <stdio.h>
#include <stdlib.h>
#include "card.h"

#define RAZZ_CARD_IN_HAND_COUNT 7

struct decided_cards
{
  uint8_t my_card_count; /**< The number of my cards. */
  const card *my_cards[3]; /**< My initial three cards. */
  uint8_t opponent_card_count; /**< The total number of opponents' cards. */
  const card *opponent_cards[7]; /**< The initial card of the opponent. */
};

int
process_args (unsigned long *game_count,
	      enum card_rank *desired_rank,
	      card_deck *deck,
	      struct decided_cards *decided_cards,
	      int argc,
	      char **argv)
{
  int i, end;
  enum card_rank rank;
  enum card_suit_rank csr;

  if (argc < 5 || argc > 12)
    {
      fprintf (stderr, "Invalid argument count\n");
      return 1;
    }

  *game_count = atoi (*argv++);

  rank = strtorank (*argv++);
  if (rank < R5 || rank > K)
    {
      fprintf (stderr, "Invalid desired rank (5 <= R <= K)\n");
      return 1;
    }
  *desired_rank = rank;

  end = 3;
  decided_cards->my_card_count = end;  
  for (i = 0; i < end; i++)
    {
      const card *my_card;

      if ((my_card = strtocard (*argv++)) == NULL)
	{
	  fprintf (stderr, "Invalid my card specification #%d\n", i + 1);
	  return 1;
	}

      csr = get_card_suit_rank (my_card);

      if (!is_card_in_deck (csr, deck))
	{
	  fprintf (stderr, "Duplicated my card specification #%d\n", i + 1);
	  return 1;
	}

      strip_card_from_deck (csr, deck);

      decided_cards->my_cards[i] = my_card;
    }

  i = 0;
  while (*argv != NULL)
    {
      const card *opponent_card;

      if ((opponent_card = strtocard (*argv++)) == NULL)
	{
	  fprintf (stderr, "Invalid opponent card specification #%d\n", i + 1);
	  return 1;
	}

      csr = get_card_suit_rank (opponent_card);

      if (!is_card_in_deck (csr, deck))
	{
	  fprintf (stderr, "Duplicated opponent card specification #%d\n",
		   i + 1);
	  return 1;
	}

      strip_card_from_deck (csr, deck);

      decided_cards->opponent_cards[i] = opponent_card;

      i++;
    }
  decided_cards->opponent_card_count = i;

  return 0;
}

/**
 * Complete my hand with the predetermined cards and cards dealt from the deck.
 * The predetermined cards must not contain any duplicate.
 *
 * @param [in] my_hand the hand to be completed.
 * @param [in] decided_cards the predetermined cards for my hand.
 * @param [in] deck the deck from which additional cards are dealt.
 */
void
complete_hand (card_hand *my_hand, struct decided_cards *decided_cards,
	       card_deck *deck)
{
  int i, end = decided_cards->my_card_count;

  for (i = 0; i < end; i++)
    {
      insert_into_hand (my_hand, decided_cards->my_cards[i]);
    }

  end = RAZZ_CARD_IN_HAND_COUNT - end;
  for (i = 0; i < end; i++)
    {
      insert_into_hand (my_hand, deal_from_deck (deck));
    }
}

/** Removes a duplicated rank from a hand. */
static enum itr_action
duplicated_rank_remover (unsigned long len, unsigned long pos, const card *c)
{
  static enum card_rank prev_rank;
  enum card_rank curr_rank = get_card_rank (c);

  if (pos == 0)
    {
      prev_rank = curr_rank;
      return CONTINUE;
    }

  if (prev_rank == curr_rank)
    {
      return REMOVE_AND_CONTINUE;
    }

  prev_rank = curr_rank;

  return CONTINUE;
}

/** Keeps only the first five cards in a hand. */
static enum itr_action
length_trimmer (unsigned long len, unsigned long pos, const card *c)
{
  if (pos >= 5)
    {
      return REMOVE_AND_CONTINUE;
    }

  return CONTINUE;
}

/** Prints all cards in the hand. */
static enum itr_action
card_printer (unsigned long len, unsigned long pos, const card *c)
{
  printf ("%4s", cardtostr (get_card_suit_rank (c)));
  return CONTINUE;
}

/**
 * Determines whether or not a particular hand has a particular rank or even
 * better.
 *
 * @param [in] hand the hand whose rank is to be determined.
 * @param [in] desired_rank the desired rank.
 *
 * @return zero if the hand does not have the desired rank
 *         or non-zero if the hand has the desired rank.
 */
int
is_rank_desirable (card_hand *hand, enum card_rank desired_rank)
{
  enum card_rank r;
  unsigned long cards_count;

#ifndef NDEBUG
  iterate_hand (hand, card_printer);
#endif
  iterate_hand (hand, duplicated_rank_remover);
#ifndef NDEBUG
  printf (" -> ");
  iterate_hand (hand, card_printer);
#endif

  cards_count = count_cards_in_hand (hand);
  if (cards_count < 5) // too many pairs in hand
    {
#ifndef NDEBUG
      printf ("\n");
#endif
      return 0;
    }

  iterate_hand (hand, length_trimmer);
  r = get_max_rank_of_hand (hand);

#ifndef NDEBUG
  if (cards_count > 5)
    {
      printf ("\t");
    }
  else
    {
      printf ("\t\t");
    }
  printf ("-> ");
  iterate_hand (hand, card_printer);
  printf (": %2s\n",
	  ranktostr (r));
#endif

  if (get_max_rank_of_hand (hand) == desired_rank)
    {
      return 1;
    }

  return 0;
}

/**
 * Strips the deck from decided cards. The given decided cards must not contain
 * any duplicate.
 *
 * @param [in] deck the deck to be stripped out.
 * @param [in] decided_cards the non-duplicated cards to be stripped out.
 */
void
strip_deck (card_deck *deck, struct decided_cards *decided_cards)
{
  int i, end;

  end = decided_cards->my_card_count;
  for (i = 0; i < end; i++)
    {
      strip_card_from_deck (get_card_suit_rank (decided_cards->my_cards[i]),
			    deck);
    }

  end = decided_cards->opponent_card_count;
  for (i = 0; i < end; i++)
    {
      strip_card_from_deck (get_card_suit_rank (decided_cards->opponent_cards[i]),
			    deck);
    }
}

/**
 * Returns the probability of getting a hand with a particular rank or better
 * in a number of simulations.
 *
 * @param [in] desirable_rank_count the count of getting the right hand.
 * @param [in] game_count the total number of simulated games.
 *
 * @return the probability.
 */
double
calc_probability (unsigned long desirable_rank_count, unsigned long game_count)
{
  return (double) desirable_rank_count / (double) game_count;
}

int
main (int argc, char **argv, char **envp)
{
  unsigned long i;
  struct decided_cards decided_cards;
  unsigned long game_count;
  enum card_rank desired_rank;
  unsigned long desirable_rank_count = 0;
  card_hand *my_hand;
  card_deck *deck;

  srand48 (time (NULL));

  if (argc < 6 || argc > 13)
    {
      fprintf (stderr,
	       "Usage: razz GAME_COUNT DESIRED_RANK\n"
	       "\tCARD1 CARD2 CARD3\n"
	       "\t[OPP1_CARD [OPP2_CARD [... [OPP7_CARD]]]]\n"
	       "\n"
	       "You specify the desired rank with the following symbols:\n"
	       "\t5, 6, ..., 10, J, Q, K\n"
	       "You specify a card with the following symbols:\n"
	       "\tSA, S2, ..., S10, SJ, SQ, SK for spade ace to spade king\n"
	       "\tHA, H2, ..., H10, HJ, HQ, HK for heart ace to spade king\n"
	       "\tCA, C2, ..., C10, CJ, CQ, CK for club ace to spade king\n"
	       "\tDA, D2, ..., D10, DJ, DQ, DK for diamond ace to spade king\n"
	       "\n"
	       "The probability of getting a hand whose rank is\n"
	       "\tequal to the desired rank will be output to stdout.\n"
	       "The range of the probability is [0.0000, 1.0000]\n");
      exit (EXIT_FAILURE);
    }

  my_hand = create_hand (RAZZ_CARD_IN_HAND_COUNT, sort_card_by_rank);
  if (my_hand == NULL)
    {
      fprintf (stderr, "Cannot create a hand\n");
      exit (EXIT_FAILURE);
    }

  deck = create_shuffled_deck ();
  if (deck == NULL)
    {
      fprintf (stderr, "Cannot create a shuffled deck\n");
      destroy_hand (&my_hand);
      exit (EXIT_FAILURE);
    }  

  if (process_args (&game_count, &desired_rank, deck, &decided_cards,
		    argc - 1, &argv[1]))
    {
      destroy_hand (&my_hand);
      destroy_deck (&deck);
      exit (EXIT_FAILURE);
    }  

  for (i = 0; i < game_count; i++)
    {
      reset_hand (my_hand);
      complete_hand (my_hand, &decided_cards, deck);
      
      if (is_rank_desirable (my_hand, desired_rank))
	{
	  desirable_rank_count++;
 	}

      destroy_deck (&deck);

      deck = create_shuffled_deck ();
      if (deck == NULL)
	{
	  fprintf (stderr, "Cannot create a shuffled deck\n");
	  break;
	}
      strip_deck (deck, &decided_cards);
    }

  destroy_hand (&my_hand);
  destroy_deck (&deck);

  printf ("%.4f\n", calc_probability (desirable_rank_count, i));

  exit (EXIT_SUCCESS);
}
