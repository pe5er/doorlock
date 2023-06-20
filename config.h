#define MANAGER_AP "esp32-roller-shutter-setup"
#define HOSTNAME "esp32-roller-shutter"

// files to store card/fob data in
#define CARD_TMPFILE "/cards.tmp"
#define CARD_FILE "/cards.dat"
#define LOG_FILE "/log.dat"

#define TIMER_INITIAL 10 * 1000 // how long will the system stay unlocked for if no buttons are pressed
#define TIMER_SANITY 30 * 1000 // how long will the system stay unlocked before locking, regardless of the user's actions
#define TIMER_BUTTON 5 * 1000 // how much longer will the system stay unlocked after a button has been released