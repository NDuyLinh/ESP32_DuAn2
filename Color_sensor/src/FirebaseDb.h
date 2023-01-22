#ifndef FIREBASEDB_H
#define FIREBASEDB_H

#include "Datatypes.h"

/* Connection state for Firebase Database
 *
 */
typedef enum
{
    FIREBASE_INIT = 0,
    FIREBASE_CONNECTING,
    FIREBASE_CONNECTED
} t_FirebaseState;

/* Periodic task handler to manage
 * connection to Firebase Database
 *
 * input: none
 * output: none
 */
void Database_task(void);

/* Force expiration of current token
 * to trigger reconnection when network
 * issues detected
 *
 * input: none
 * output: none
 */
void Database_refreshConnection(void);

/* Get status of connection to Firebase
 *
 * input: none
 * output: connection state
 */
t_FirebaseState Database_getState(void);

/* Push a color record to Firebase database
 *
 * input: pointer to color object
 * output: operation result: true for success, false otherwise
 */
bool Database_pushObjectColor(t_Color * objectColor);

#endif /* FIREBASEDB_H */
