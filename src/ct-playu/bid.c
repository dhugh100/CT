#include "play.h"
#include "game.h"
#include "strategy.h"

// Check that the entered character is a legal bid value (0, 2, 3, or 4)
// - Returns true and flushes stdin if valid; false otherwise
bool validate_bid(State *s, const char str[])
{

    // Check for valid bid characters
    if (str[0] != '0' && str[0] != '2' && str[0] != '3' && str[0] != '4') {
        printf("Bad bid character entered\n");
        printf( "Bid must be one of 0,2,3,4\n");
        flush_if_needed(str);
        return false;
    }
    flush_if_needed(str);
    return true;
}

// Prompt the user repeatedly until a valid bid is entered
// - Returns the bid as an integer (0=pass, 2, 3, or 4)
char get_user_bid(State *s)
{
    char bid_input[2] = { 0 };
    bool ok_bid = false;
    char bid = -1;
    while (!ok_bid) {
        printf( "Enter bid (0 for pass, 2, 3, 4)\n");
        fgets(bid_input, 2, stdin);
        ok_bid = validate_bid(s, bid_input);
    }
    bid = bid_input[0] - '0';
    return bid;                 // Convert char to int
}

// Run the bid phase for one hand
// - Both players bid in turn order; AI uses strategy, human is prompted
// - Applies bids to game state and announces the outcome
void bid_phase(State *s, Strat *strat, long strat_cnt)
{
    printf("===== Bid starting, first bidder is %s =====\n", s->dealer == 0 ? "you" : "me");
    UC bid = 0;
    char log_buff[256] = {0x00};

    // Get bids
    if (s->to_act == 0) {
        // Get my bid 
        bid = get_best_action(strat, strat_cnt, s);
        if (bid == 0xff) {
            bid = 0;  // For now just pass if no model bid, but could use heuristic bid here instead
            sprintf(log_buff,"Stage:Bid, actor:Me, bid:%u, defaulting to pass\n", bid>0?bid+1:0);log_msg(log_buff);
        }
        else {
            sprintf(log_buff, "Stage:Bid, actor:Me, bid:%u, model bid found\n", bid>0?bid+1:0);log_msg(log_buff);
        }

        apply_bid(s, bid);

        printf( "My bid is %d\n", s->bid[0] > 0 ? s->bid[0] + 1 : 0); // Display bid as 0, 2, 3, or 4

        // Get bid from user
        bid = get_user_bid(s);

        // Bid in training is stored as 0 (pass) 1, 2, 3 for easier handling
        apply_bid(s, bid > 0 ? bid - 1 : 0);

        printf( "Your bid is %d\n", s->bid[1] > 0 ? s->bid[1] + 1 : 0);
        sprintf(log_buff, "Stage:Bid, actor:You, bid:%u\n", bid);log_msg(log_buff);
    } else {
        // Get bid from user
        bid = get_user_bid(s);

        // Bid in training is stored as 0 (pass) 1, 2, 3 for easier handling
        apply_bid(s, bid > 0 ? bid - 1 : 0);

        printf( "Your bid is %d\n", s->bid[1] > 0 ? s->bid[1] + 1 : 0);
        sprintf(log_buff, "Stage:Bid, actor:You, bid:%u\n", bid);log_msg(log_buff);

        // Get my bid
        bid = get_best_action(strat, strat_cnt, s);
        if (bid == 0xff) {
            bid = 0;
            sprintf(log_buff,"Stage:Bid, actor:Me, bid:%u, defaulting to pass\n", bid>0?bid+1:0);log_msg(log_buff);
        }
        else {
            sprintf(log_buff, "Stage:Bid, actor:Me, bid:%u, strategy bid found\n", bid>0?bid+1:0);log_msg(log_buff);
        }
        apply_bid(s, bid);
        printf( "My bid is %d\n", s->bid[0] > 0 ? s->bid[0] + 1 : 0); // Display bid as 0, 2, 3, or 4
    }

    // Announce bid results
    if (s->bid_stolen) {
        printf( "Bid was stolen by %s as dealer takes a tie\n", s->winning_bidder == 0 ? "me" : "you");
    } else if (s->bid_forced) {
        printf( "Dealer (%s) is forced to bid, assigned a bid of 2\n", s->dealer == 0 ? "me" : "you");
    } else {
        printf( "%s won the bid with %u\n", s->winning_bidder == 0 ? "I" : "You", s->winning_bid > 0 ? s->winning_bid + 1 : 0); // Display bid as 0, 2, 3, or 4  
    }
    sprintf(log_buff, "Stage:Bid, winner:%s, winning_bid:%u, bid_forced:%s, bid_stolen:%s\n", 
            s->winning_bidder==0?"Me":"You", s->winning_bid>0?s->winning_bid+1:0, 
            s->bid_forced == 1 ? "True" : "False", s->bid_stolen == 1 ? "True" : "False"); log_msg(log_buff);
}
