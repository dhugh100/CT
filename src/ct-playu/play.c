#include <time.h>
#include "bid.h"
#include "play.h"
#include "deck.h"
#include "hand.h"

// Log a message with a timestamp to the log file
void log_msg(const char* message)
{   
    FILE *log_file = fopen("playu.log", "a"); // Open log file in append mode
    if (!log_file) {
        fprintf(stderr, "Error opening log file\n");
        return;
    }
    time_t current_time;
    struct tm *local_time;
    char time_string[100]; // Buffer to hold the formatted time string

    // Get current time
    time(&current_time);
    // Convert to local time components
    local_time = localtime(&current_time);

    // Format the time into a string
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", local_time);

    // Write the timestamp and message to the file
    fprintf(log_file, "[%s] %s", time_string, message);

    // Close the file
    fclose(log_file);
}
 
// Compare two cards for qsort
// - Sort by suit first, then by rank, both ascending (low to high)
int compare_cards(const void *a, const void *b) 
{
    Card *cardA = (Card *) a;
    Card *cardB = (Card *) b;
    if (cardA->suit != cardB->suit) {
        return cardA->suit - cardB->suit;       // Sort by suit first
    }   
    return cardA->rank - cardB->rank;   // Then by rank
}

// Get rid of any extra input in the buffer
void flush_input_buffer()
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// Only flush if fgets didn't already consume the newline
void flush_if_needed(const char *buf)
{
    if (strchr(buf, '\n') == NULL)
        flush_input_buffer();
}

// Format all cards in an array as a space-separated ASCII string (e.g. "CA H7 S2")
// - Breaks when rank==0 (empty/played cards)
void card_string(const Card *card_array, int array_num,  char *buff, int buff_size)
{
    char str[4] = { 0 };
    UC i = 0;
    for (; i < array_num; i++) {
        if (strlen(buff) + 4 >= buff_size) {
            fprintf(stderr, "Buffer overflow in card_string\n");
            return;
        }
        // 0 means out of cards, so skip and end string
        if (card_array[i].rank == 0) {
            break;   
        } else {
            sprintf(str, "%c%c ", "CDHS"[card_array[i].suit],
                    "  23456789TJQKA"[card_array[i].rank]);
            strcat(buff, str);
        }
    }
    if (strlen(buff) > 0) buff[strlen(buff)-1] = '\0'; // Replace last space with a null
}

// Run a full interactive game to completion
// - Alternates dealer each hand, plays until one player reaches winning score
// - P0 is the AI (uses strat), P1 is the human player
int play_game(Strat *strat, long strat_cnt, UC winning_score, UC dealer, unsigned int seed)
{
    // Track running game score
    int game_score[2] = { 0 }; // Game score for each player

    // Setup game state
    State game_state = {0};
    State *s = &game_state;

    char log_buff[256] = { 0 };

    // Game loop - play hands until one player reaches winning score
    UC hand_num = 0;
    while (game_score[0] < winning_score && game_score[1] < winning_score) {

        // Initialize game state for new hand
        memset(s, 0x00, sizeof(State)); // Clear game state for new hand
        s->dealer = dealer;
        s->to_act = 1 - dealer; // First to act is non-dealer
        s->seed = seed += 1000; // Update seed for next hand
        
        hand_num++;
        // Make and deal cards
        s->seed += hand_num * 1000; // Seed the random number generator seed
        make_cards_and_deal(s);

        // Sort hands to make it easier for user to see in display
        qsort(s->hand[0].card, HAND_SIZE, sizeof(Card), compare_cards);
        qsort(s->hand[1].card, HAND_SIZE, sizeof(Card), compare_cards);
        
        char card_buff[2][48] = { 0 };
        card_string(s->cards_won[0], HAND_SIZE, card_buff[0], sizeof(card_buff[0]));
        card_string(s->cards_won[1], HAND_SIZE, card_buff[1], sizeof(card_buff[1]));   
        sprintf(log_buff, "Stage:Deal, dealt Me:%s, You:%s\n", card_buff[0], card_buff[1]); log_msg(log_buff);

        // Bid phase
        bid_phase(s, strat, strat_cnt);

        // Play phase
        UC leading = true; // Track for screen and log
        while (!s->hand_done) {
            play_hand(s, strat, strat_cnt, leading);
            leading = 1 - leading; // flip every iteration
        }

        // Score the hand
        score(s);

        // Adjust the score for display purposes
        // - In training score, bids are 0123, while for the user in user play they are 0,2,3,4
        // - Ignore the score return designed for CFR work and use the t_score values to determine score for display and game score tracking 
        printf("===== Hand %u complete =====\n",hand_num);

        card_string(s->cards_won[0], HAND_SIZE * 2, card_buff[0], sizeof(card_buff[0]));
        card_string(s->cards_won[1], HAND_SIZE * 2, card_buff[1], sizeof(card_buff[1]));
        printf("Cards won: Me: %s, You: %s\n", card_buff[0], card_buff[1]);

        printf("Hand Score: Me %i, You %i\n", s->t_score[0], s->t_score[1]);
        sprintf(log_buff, "Stage:Hand Score: Me:%i, You:%i\n", s->t_score[0], s->t_score[1]); log_msg(log_buff);

        game_score[0] += s->t_score[0];
        game_score[1] += s->t_score[1];
        printf("Game Score: Me: %i, You: %i\n", game_score[0], game_score[1]); 
        sprintf(log_buff, "Stage:Game Score: Me: %i, You: %i\n", game_score[0], game_score[1]); log_msg(log_buff);

        dealer = 1 - dealer; // Rotate dealer each hand
    }
    printf("===== Game over =====\nMy score:%i, Your score:%i\n", game_score[0], game_score[1]); 
    sprintf(log_buff, "Game over: Me:%i, You:%i\n", game_score[0], game_score[1]); log_msg(log_buff);
    return 0;
}     
