#ifndef PARENTAL_H
#define PARENTAL_H

#include <stddef.h>
#include "settings.h"

int parental_is_enabled(void);
int parental_check_pin(const char *pin);
void parental_log_action(const char *action, const char *details);
void parental_maybe_report(void);
int parental_force_report(void);

#endif
