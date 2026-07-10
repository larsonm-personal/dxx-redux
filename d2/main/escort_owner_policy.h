#ifndef D2_ESCORT_OWNER_POLICY_H
#define D2_ESCORT_OWNER_POLICY_H

int escort_owner_request_allowed(int current_owner,
                                 int sender,
                                 int requested_owner,
                                 int sender_eligible,
                                 int requested_owner_eligible,
                                 unsigned int current_generation,
                                 unsigned int request_generation);

#endif
