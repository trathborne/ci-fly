/* This is a piece of junk to verify that cidb.db is readable */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <errno.h>
#include <db_185.h>

int
main (void)
{
    DB *sdb;
    DBT key, value;

    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));

    sdb = (DB *) dbopen("cidb.db", O_CREAT, 0600, DB_HASH, NULL);

    if(!sdb) {
        printf("cidb could not be opened: ");

        if (errno == EFTYPE) {
            printf("EFTYPE: Incorrect file format\n");
        } else if (errno == EINVAL) {
            printf("EINVAL: Invalid argument\n");
        } else {
            printf("Unrecognized errno: %d\n", errno);
        }

        exit(1);
    }


    return 0;
}

/*** EOF *********************************************************************/
