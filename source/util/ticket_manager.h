#ifndef TICKET_MANAGER_H
#define TICKET_MANAGER_H

#include <switch.h>

// Ticket information
typedef struct {
    u64 title_id;
    u8 key_gen;
    u8 rights_id[16];
    bool in_use;
    char title_name[256];
} TicketInfo;

// Initialize ticket management
Result ticket_init(void);
void ticket_exit(void);

// Ticket operations
Result ticket_list(TicketInfo** out_tickets, size_t* out_count);
Result ticket_install(const char* path);
Result ticket_remove(const TicketInfo* ticket);
Result ticket_dump(const TicketInfo* ticket, const char* out_path);

// Common ticket
Result ticket_get_common(u64 title_id, void* out_ticket, size_t* out_size);
Result ticket_has_common(u64 title_id, bool* out_has);

// Title key operations
Result ticket_get_title_key(const TicketInfo* ticket, u8* out_key);
Result ticket_import_title_key(u64 title_id, const u8* key);

// Personalized ticket
Result ticket_personalize(const void* ticket_data, size_t ticket_size,
                         void* out_ticket, size_t* out_size);

#endif // TICKET_MANAGER_H