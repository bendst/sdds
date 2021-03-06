/*
 * Locator.c
 *
 *  Created on: 10 Aug 2012
 *      Author: k_beckma
 */
#include "sDDS.h"
#include "Locator.h"
#include "LocatorDB.h"
#include "sdds_types.h"
#include "Log.h"

rc_t
Locator_contains(List_t* list, Locator_t* l) {

    if ((list == NULL) || (l == NULL)) {
        return SDDS_RT_BAD_PARAMETER;
    }

    Locator_t* data = (Locator_t*) list->first_fn(list);

    while (data != NULL) {
        // locator is allready in the list
        if (Locator_isEqual(data, l) == true) {
            return SDDS_RT_OK;
        }
        data = list->next_fn(list);
    }

    return SDDS_RT_FAIL;
}

rc_t
Locator_print(Locator_t* loc) {
    char addr[NI_MAXHOST];
    rc_t ret = Locator_getAddress(loc, addr, NI_MAXHOST);
    printf("Locator {%s}\n", addr);
    return ret;
	return SDDS_RT_OK;
}
