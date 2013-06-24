/*
 * This file is distributed as part of the SkySQL Gateway.  It is free
 * software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright SkySQL Ab 2013
 */
#include <stdlib.h>
#include <string.h>
#include <users.h>
#include <atomic.h>

/**
 * @file users.c User table maintenance routines
 *
 * @verbatim
 * Revision History
 *
 * Date		Who		Description
 * 23/06/13	Mark Riddoch	Initial implementation
 *
 * @endverbatim
 */

/**
 * The hash function we user for storing users.
 *
 * @param key	The key value, i.e. username
 * @return The hash key
 */
static int
user_hash(char *key)
{
	return (*key + *(key + 1));
}

/**
 * Allocate a new users table
 *
 * @return The users table
 */
USERS *
users_alloc()
{
USERS 	*rval;

	if ((rval = malloc(sizeof(USERS))) == NULL)
		return NULL;

	if ((rval->data = hashtable_alloc(52, user_hash, strcmp)) == NULL)
	{
		free(rval);
		return NULL;
	}

	hashtable_memory_fns(rval->data, (HASHMEMORYFN)strdup, (HASHMEMORYFN)free);

	return rval;
}

/**
 * Remove the users table
 *
 * @param users	The users table to remove
 */
void
users_free(USERS *users)
{
	hashtable_free(users->data);
	free(users);
}

/**
 * Add a new user to the user table. The user name must be unique
 *
 * @param users		The users table
 * @param user		The user name
 * @param auth		The authentication data
 * @return	The number of users added to the table
 */
int
users_add(USERS *users, char *user, char *auth)
{
int	add;

	atomic_add(&users->stats.n_adds, 1);
	add = hashtable_add(users->data, user, auth);
	atomic_add(&users->stats.n_entries, add);
	return add;
}

/**
 * Delete a user from the user table.
 *
 * @param users		The users table
 * @param user		The user name
 * @return	The number of users deleted from the table
 */
int
users_delete(USERS *users, char *user)
{
int	del;

	atomic_add(&users->stats.n_deletes, 1);
	del = hashtable_delete(users->data, user);
	atomic_add(&users->stats.n_entries, del * -1);
	return del;
}

/**
 * Fetch the authentication data for a particular user from the users table
 *
 * @param users		The users table
 * @param user		The user name
 * @return	The authentication data or NULL on error
 */
char
*users_fetch(USERS *users, char *user)  
{
	atomic_add(&users->stats.n_fetches, 1);
	return hashtable_fetch(users->data, user);
}
