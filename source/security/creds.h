#ifndef HELLO_CREDS_H
#define HELLO_CREDS_H

// simple credential entry
typedef struct {
    char site[256];
    char username[128];
    char password[128];
} CredEntry;

// Load credentials from sdmc:/DBFM/WEB/Passwords/Passwords.json
// Returns number of entries and allocates entries array which must be freed by caller via free_creds
int load_credentials(CredEntry **out_entries);
// Save credentials array to the json file
int save_credentials(CredEntry *entries, int count);
// Free array allocated by load_credentials
void free_creds(CredEntry *entries, int count);

#endif // HELLO_CREDS_H
