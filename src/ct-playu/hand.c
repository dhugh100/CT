#include "hand.h"
#include "play.h"
#include "game.h"
#include "strategy.h"

// Convert a two-character ASCII card (e.g. 'C','A') into a formatted Card struct
void convert_ascii_to_formatted(Card *c, const char suit, const char rank)
{
    c->suit = (suit == 'C') ? C : 
              (suit == 'D') ? D : 
              (suit == 'H') ? H : S;
    c->rank = (rank == '2') ? 2 : 
              (rank == '3') ? 3 : 
              (rank == '4') ? 4 : 
              (rank == '5') ? 5 : 
              (rank == '6') ? 6 : 
              (rank == '7') ? 7 : 
              (rank == '8') ? 8 : 
              (rank == '9') ? 9 : 
              (rank == 'T') ? 10 :
              (rank == 'J') ? 11 :
              (rank == 'Q') ? 12 :
              (rank == 'K') ? 13 : 14; 
}

// Convert a formatted Card struct to a two-character ASCII string (e.g. "CA"), null-terminated
void convert_formatted_to_ascii(const Card c, char *str)
{
    str[0] = (c.suit == C) ? 'C' :
             (c.suit == D) ? 'D' :
             (c.suit == H) ? 'H' : 'S';
    str[1] = (c.rank == 2) ? '2' :
             (c.rank == 3) ? '3' :
             (c.rank == 4) ? '4' :
             (c.rank == 5) ? '5' :
             (c.rank == 6) ? '6' :
             (c.rank == 7) ? '7' :
             (c.rank == 8) ? '8' :
             (c.rank == 9) ? '9' :
             (c.rank == 10) ? 'T' :
             (c.rank == 11) ? 'J' :
             (c.rank == 12) ? 'Q' :
             (c.rank == 13) ? 'K' : 'A';
    str[2] = '\0'; // Null terminate
}

// Search player 1's hand for a card matching the ASCII string str
// - Returns the hand index if found, 0xff if not present
UC validate_in_hand(State *s, const char str[]) {

    for (UC i = 0; i < HAND_SIZE; i++) {
        char card_str[3] = {0};
        convert_formatted_to_ascii(s->hand[s->to_act].card[i], card_str);
        if (strncasecmp(card_str, str, 2) == 0) {
            flush_if_needed(str);
            return i; // Card is in hand
        }
    }
    flush_if_needed(str);
    return 0xff;
}

// Prompt the human player to enter a card, repeating until the choice is valid
// - card_input is a caller-provided 3-byte buffer for the raw input
// - Validates that the card is in the hand and is a legal play
// - Returns the hand index of the chosen card
UC get_user_card(State *s, char *card_input) {

    UC index = 0xff;
    while (index == 0xff) {
        printf("Enter card to play\n");
        fgets(card_input, 3, stdin);
        index = validate_in_hand(s, card_input);
        if (index == 0xff) {
            printf("Not a card in hand\n");
            continue; // Skip legality check — card isn't in hand
        }
        Card c;
        convert_ascii_to_formatted(&c, card_input[0], card_input[1]);
        if (!is_legal_play(s,c)) {
            printf("Not a legal play\n");
            index = 0xff; // Reset to force re-entry
        }
    }
    return index;
}


// Handle one player's turn within a trick
// - If AI (to_act==0): looks up best action from strategy, falls back to first legal play
// - If human (to_act==1): prompts user for a card
// - leading: 1 if this is the lead play of the trick, 0 if the response
int play_hand(State *s, Strat *strat, long qty, UC leading)
{    
    UC index = 0;
    UC action = 0;

    UC save_actor = s->to_act;
    UC save_trick_num = s->trick_num;
    Card save_c;

    char log_buff[256];

    printf("\n==== %s Trick %u ===== \n", leading == 0 ? "Response" : "Lead", s->trick_num + 1);

    // Have the actor play the card
    // - Pass the card index to apply function
    if (s->to_act == 1) {
        // Show hand on screen
        char buff[48] = {0};
        card_string(s->hand[1].card, HAND_SIZE,  buff, sizeof(buff));
        printf("Your hand is %s\n",buff);

        // Get user input for card to play
        char card_input[3] = {0};
        index = get_user_card(s,(char *)&card_input); 

        // Save card for printing and apply play to game state
        save_c = s->hand[1].card[index]; 
        apply_play(s, index);  
       }         
    else {
        action = get_best_action(strat, qty, s);
        if (action == 0xff) {
            char out[6] = {0};
            int n = legal_play(s, out);
            assert(n > 0);
            action = out[0];  // Just take the 1st one 
         }
         // Get card index for action, save card for printing, and apply play to game state
         index = bind_card_index_to_action(s, action); // Get card index for action
         save_c = s->hand[0].card[index]; 
         apply_play(s,index); 
    } 

    // Print the card played, log the play, and note if trump declared 
    char str[3] = {0};
    convert_formatted_to_ascii(save_c,str);

    // For first trick, first card note trump delcared 
    if (s->trick_num == 0 && leading == 1) {
        printf("%s played %s, trump is %c\n", save_actor == 0 ? "I" : "You", str, "CDHS"[s->trump]);
        sprintf(log_buff, "Stage:Play, trick:%u, actor:%s, card:%s, trump is declared as %c\n",
                save_trick_num+1, save_actor==0?"Me":"You", str, "CDHS"[s->trump]); log_msg(log_buff);
    }
    else {
        printf("%s played %s\n", save_actor == 0 ? "I" : "You", str);
        sprintf(log_buff, "Stage:Play, trick:%u, actor:%s, card:%s\n",
            save_trick_num+1, save_actor==0?"Me":"You", str); log_msg(log_buff);
    }

    // Print the trick winner if the trick is done
    if (s->trick_num > save_trick_num || s->hand_done) {
        printf("%s won the trick\n", s->trick_winner[save_trick_num] == 0 ? "I" : "You");
        sprintf(log_buff, "Stage:Play, trick:%u, winner:%s\n", save_trick_num+1, s->trick_winner[save_trick_num]==0?"Me":"You"); log_msg(log_buff);
    }
    return 0;
}
