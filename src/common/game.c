// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "game.h"
#include "util.h"

// Legal bids 
// - Return number of legal bid actions and populate output array with bid amounts
// - Bids are 0 (pass), 1, 2, 3, interpreted as 0 (pass), 2, 3, 4 in game logic
int legal_bid(State *s, UC out[4])
{
    UC _1st_bidder = 1 - s->dealer;
    UC _2nd_bidder = s->dealer;

    // First bidder is non-dealer
    if (s->to_act == _1st_bidder) {
        out[0] = 0; // Pass
        out[1] = 1;
        out[2] = 2;
        out[3] = 3;
        return 4;
    }
    // Second bidder's action depends on first bidder
    // - Can pass if first bidder bid == 0, apply logic will force the bid to 1(2)
    // - Otherwise 2nd bidder can match 1st (steal) or outbid
    else if (s->to_act == _2nd_bidder) {
        switch (s->bid[_1st_bidder]) {
            case 0: // First bidder passed
                out[0] = 0; // Apply should change to forced 1(2)
                return 1;
            case 1: // First bidder bid 1(2)
                out[0] = 1; // Can steal
                out[1] = 2; // Can outbid 
                out[2] = 3; // Can outbid 
                return 3;
            case 2: // First bidder bid 2(3)
                out[0] = 2; // Can steal 
                out[1] = 3; // Can outbid 
                return 2;
            case 3: // First bidder bid 3(4)
                out[0] = 3; // Can steal 
                return 1;
            default:
                assert(false && "Invalid bid value");
                return 0;
        }
    }
    else {
        assert(false && "Invalid player to act");
        return 0;
    }
}

// Update state based on bids
// - Apply bid logic for first and second bidder
// - Update winning bidder, winning bid, leader, to_act, stage
int apply_bid(State *s, UC bid)
{
    // Record bid for player
    s->bid[s->to_act] = bid;

    // Setup variables for clarity on 1st and 2nd bidder
    UC _1st_bidder = 1 - s->dealer;
    UC _2nd_bidder = s->dealer;

    // Apply 1st bid logic - any bid is good 
    if (s->to_act == _1st_bidder) {
        s->to_act = _2nd_bidder;
    }
    // Apply 2nd bidder logic
    else {
        s->stage = PLAY; // Move to play stage for next action
        s->trump = PRE_TRUMP; // Trump not declared until first card played

        if (bid == 0) { // Second bidder passed 
            if (s->bid[_1st_bidder] == 0) {
                // First bidder passed, 2nd bidder (dealer) must bid two
                s->winning_bidder = _2nd_bidder;
                s->winning_bid = 1; // Forced 1(2)
                s->bid[_2nd_bidder] = 1;  // Change bid to forced 1(2)
                s->bid_forced = true;
                s->leader = _2nd_bidder;
                s->to_act = _2nd_bidder;
            } else {
                // Second bidder passed, 1st bidder not 0, first bidder wins
                s->winning_bidder = _1st_bidder;
                s->winning_bid = s->bid[_1st_bidder];
                s->leader = _1st_bidder;
                s->to_act = _1st_bidder;
            }
        }
        // Tie positive bid - always steal
        else if (bid == s->bid[_1st_bidder]) {
            // Steal for dealer
            s->winning_bidder = _2nd_bidder;
            s->winning_bid = s->bid[_2nd_bidder];
            s->bid_stolen = true;
            s->leader = _2nd_bidder;
            s->to_act = _2nd_bidder;
        }
        // Not tie or pass, see who outbids who
        else if (bid > s->bid[_1st_bidder]) {
            // Select high bidder
            s->winning_bidder = _2nd_bidder;
            s->winning_bid = s->bid[_2nd_bidder];
            s->leader = _2nd_bidder;
            s->to_act = _2nd_bidder;
        }
        else if (bid < s->bid[_1st_bidder]) {
            // Select high bidder
            s->winning_bidder = _1st_bidder;
            s->winning_bid = s->bid[_1st_bidder];
            s->leader = _1st_bidder;
            s->to_act = _1st_bidder;
        }
        else {
            assert(false && "Invalid bid comparison");
        }
    }
    return 0;
}

// Remove card from player's hand at index
// - Shift cards down to fill gap
void remove_card(State *sp, char p, char index)
{
    // Move cards starting from index "down" one offset
    for (char i = index; i + 1 < HAND_SIZE; i++) {
        sp->hand[p].card[i].suit = sp->hand[p].card[i+1].suit;
        sp->hand[p].card[i].rank = sp->hand[p].card[i+1].rank;
    }

    // Make the last card in the array default (0)
    sp->hand[p].card[HAND_SIZE - 1].suit = 0;
    sp->hand[p].card[HAND_SIZE - 1].rank = 0;
}


// Return the history type of the played card
// Led/Response, Trump/Other, Rank
UC match_history_to_card(UC hf, Card c)
{
    UC bucket = 0;
    if (c.rank >= 12 && c.rank <=14) bucket = 0;        // High: A], K, Q
    else if (c.rank >= 10 && c.rank <= 11) bucket = 1;   // Special: J, 10
    else if (c.rank >= 5 && c.rank <=9) bucket = 2;    // Medium: 9-5
    else if (c.rank >= 2 && c.rank <=4) bucket = 3;  // Low: 4-2

    // Determine history type based on flags and bucket
    if (hf & LT) 
        return (bucket == 0) ? LTH : (bucket == 1) ? LTS : (bucket == 2) ? LTM : LTL;
    else if (hf & RT) 
        return (bucket == 0) ? RTH : (bucket == 1) ? RTS : (bucket == 2) ? RTM : RTL;
    else if (hf & LO) 
        return (bucket == 0) ? LOH : (bucket == 1) ? LOS : (bucket == 2) ? LOM : LOL;
    else if (hf & RO) 
        return (bucket == 0) ? ROH : (bucket == 1) ? ROS : (bucket == 2) ? ROM : ROL;
}  


bool match_card_to_action(Card c, UC action, UC trump)
{
    // Determine which bucket this card belongs to
    UC bucket = 0;
    if (c.rank >= 12 && c.rank <= 14) bucket = 0;       // High: A, K, Q
    else if (c.rank >= 10 && c.rank <= 11) bucket = 1; // Special: J, 10
    else if (c.rank >= 5 && c.rank <= 9)  bucket = 2;   // Medium: 9-5
    else if (c.rank >= 2 && c.rank <= 4) bucket = 3;    // Low: 4-2

    switch (action) {
        // Pre-trump actions (trump not declared yet)
        case PH:
            return (bucket == 0);
        case PS:
            return (bucket == 1);
        case PM:
            return (bucket == 2);
        case PL:
            return (bucket == 3);
        
        // Trump actions
        case TH:
            return (c.suit == trump && bucket == 0);
        case TS:
            return (c.suit == trump && bucket == 1);
        case TM:
            return (c.suit == trump && bucket == 2);
        case TL:
            return (c.suit == trump && bucket == 3);
        
        // Other (non-trump) actions
        case OH:
            return (c.suit != trump && bucket == 0);
        case OS:
            return (c.suit != trump && bucket == 1);
        case OM:
            return (c.suit != trump && bucket == 2);
        case OL:
            return (c.suit != trump && bucket == 3);
        
        default:
            assert(false && "Invalid action in match_action");
            return false;
    }
}   

// Return an index to a card in hand for the player
// - Bind a card to the action by returning an index to a card in the player's hand that matches the action
// - If multiple cards match the action, return a random index to one of the matching cards
UC bind_card_index_to_action(State *s, UC action)
{
    UC qty = HAND_SIZE - s->trick_num;
    UC pool[qty];
    UC pi = 0;
    memset(&pool, 0x00, sizeof(pool));

    // Loop through cards in hand
    Card c;
    for (UC i = 0; i < qty; i++) {
        c = s->hand[s->to_act].card[i];
        if (match_card_to_action(c, action, s->trump)) {
            pool[pi] = i;
            pi++;
        }
    }
    assert(pi > 0 && "No matching cards for action");

    // Always return the first to be deterministic vs random
    return pool[0];
}

// Check if a card play is legal given the state and rules of the game
bool is_legal_play(State *s, Card c)
{
    UC card_qty = HAND_SIZE - s->trick_num; // Number of cards left in hand for player to act

    // Leader is easy for either pre-trump declaration or after
    if (s->leader == s->to_act) return true;

    // Responder more complicated
    else {
        // Responder:
        // - If have led suit, a) follow or b) play trump
        // - if don't have led suit, can play any card
        bool has_suit = false;
        for (UC i = 0; i < card_qty; i++) {
            if (s->hand[s->to_act].card[i].suit == s->led_suit) {
                has_suit = true;
                break;
            }
        }
        if (has_suit) {
            if (c.suit == s->led_suit) return true;
            else if (c.suit == s->trump) return true;
            else return false;
        }
        else {
            return true;
        }
    }
}
// Return legal plays as abstracted actions 
// - Abstracted action must be possible with existing cards AND legal given context
// - Max 12 legal actions (Trump/Other/Pre-trump × High/Special/Medium/Low)
// - FLAG is used to indicate if action was added already, only add action once
// - Card index bind to action during play will handle multiple cards for actions
int legal_play(State *s, unsigned char *o)
{
    bool seen[256] = {false}; // Track seen actions to avoid duplicates
    UC card_qty = HAND_SIZE - s->trick_num;
    UC p = s->to_act; // Shorten
    UC oi = 0; // Action index for output array
    unsigned short flag = 0; // Flag to track if action class already added (needs 12 bits now)

    for (int i = 0; i < card_qty; i++) {
        Card c = s->hand[p].card[i];
        if (!is_legal_play(s, c)) continue; // Skip if not legal

        UC action = 0;
        
        // Pre-trump, not yet declared
        if (s->trump == PRE_TRUMP) {
            if (c.rank >= 12 && c.rank <= 14) action = PH;       // A, K, Q
            else if (c.rank >= 10 && c.rank <= 11) action = PS;  // J, 10
            else if (c.rank >= 5 && c.rank <= 9) action = PM;    // 9-5
            else if (c.rank >= 2 && c.rank <= 4) action = PL;    // 4-2
        }
        // Trump declared
        else if (s->trump == c.suit) {
            if (c.rank >= 12 && c.rank <= 14) action = TH;       // A, K, Q
            else if (c.rank >= 10 && c.rank <= 11) action = TS;  // J, 10
            else if (c.rank >= 5 && c.rank <= 9) action = TM;    // 9-5
            else if (c.rank >= 2 && c.rank <= 4) action = TL;    // 4-2
        }
        // Other suit
        else {
            if (c.rank >= 12 && c.rank <= 14) action = OH;       // A, K, Q
            else if (c.rank >= 10 && c.rank <= 11) action = OS;  // J, 10
            else if (c.rank >= 5 && c.rank <= 9) action = OM;    // 9-5
            else if (c.rank >= 2 && c.rank <= 4) action = OL;    // 4-2
        }
        
        // Check if this action type already added
        if (!seen[action]) {
            seen[action] = true;
            o[oi] = action;
            oi++;
        }
        
    }

    assert(oi > 0 && "No legal plays found");
    return oi;
}

// Play a card from the hand based on match to abstracted action
// - Get an index to a card based on the action
// - Remove the played card from the hand
// - Update the played card history for the player and trick
// - Assign trump (1st card played by leader)
// - Determine the winner and next player
// - Update trick number and hand done status
// - Update trick winner array
void apply_play(State *sp, UC index)
{
    // Save some typing
    char n = HAND_SIZE - sp->trick_num;
    char p = sp->to_act;
    char tn = sp->trick_num;

    // Save in history and remove card
    sp->hp[p].card[tn].suit = sp->hand[p].card[index].suit;
    sp->hp[p].card[tn].rank = sp->hand[p].card[index].rank;
    remove_card(sp, p, index);

    // Adjust state and played history information 
    // - If leader and first play, set trump and led suit
    // - If leader and not first play, just set led suit
    // - If not leader, set response trump or non-trump 
    if (p == sp->leader && n == HAND_SIZE) {
        sp->trump = sp->hp[p].card[tn].suit;
        sp->led_suit = sp->hp[p].card[tn].suit;
        sp->h_type[p][tn] = match_history_to_card(LT,sp->hp[p].card[tn]);
    }
    else if (p == sp->leader && n < HAND_SIZE) {
        sp->led_suit = sp->hp[p].card[tn].suit;
        if (sp->hp[p].card[tn].suit == sp->trump)
            sp->h_type[p][tn] = match_history_to_card(LT,sp->hp[p].card[tn]);
        else
            sp->h_type[p][tn] = match_history_to_card(LO,sp->hp[p].card[tn]);
    }
    // Responder, set response type
    else if (p != sp->leader && (sp->hp[p].card[tn].suit == sp->trump))
        sp->h_type[p][tn] = match_history_to_card(RT,sp->hp[p].card[tn]);
    else if (p != sp->leader && (sp->hp[p].card[tn].suit != sp->trump))
        sp->h_type[p][tn] = match_history_to_card(RO,sp->hp[p].card[tn]);
    else
        assert(false && "Invalid play state update");

    // Made play, now flip to next player OR decide winner and leader
    if (p == sp->leader && n >= 1) {
        sp->to_act = 1 - p; // Toggles to opposite player
    } else {
        // End of trick    
        // - Setup variables for leader/winner decision    
        char winner = 0;
        char leader = sp->leader;
        char trump = sp->trump;
        char led_suit = sp->hp[leader].card[tn].suit;
        char led_rank = sp->hp[leader].card[tn].rank;
        char resp_suit = sp->hp[1 - leader].card[tn].suit;
        char resp_rank = sp->hp[1 - leader].card[tn].rank;

        // Decide winner
        if (resp_suit == trump && led_suit != trump)
            winner = 1 - leader; // Responder won
        else if (led_suit == trump && resp_suit != trump)
            winner = leader; // Leader won
        else if (led_suit == resp_suit) { // Need rank to determine winner
            if (led_rank > resp_rank)
                winner = leader;
            else
                winner = 1 - leader;
        } else
            winner = leader; // Responder didn't follow and not trump so leader takes it

        // Update trick win information 
        sp->trick_winner[tn] = winner;
        sp->tricks_won[winner]++;

        // Setup for next iteration
        sp->leader = winner;
        sp->trick_num += 1;

        // See if hand done
        if (sp->trick_num == HAND_SIZE)
            sp->hand_done = 1;
        else
            sp->to_act = sp->leader;
    }
}
