/**
 * See fleece/COPYRIGHT for copyright information.
 *
 * This file is a part of Fleece.
 *
 * Fleece is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3.0 of the License, or (at your option)
 * any later version.
 *  
 * This software is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software; if not, see www.gnu.org/licenses
 */

#include "options.h"

/* Global definitions for argc and argv */
int Options::argc = 0;
char** Options::argv = NULL;

void Options::Parse(int argc, char** argv) {
    assert(!Options::argv);

    /* Make a global copy of argc and argv */
    Options::argc = argc;
    Options::argv = new char*[argc];

    /* Copy all of the arguments */
    for (int i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]);
        Options::argv[i] = new char[len + 1];
        Options::argv[i][len] = 0;
        strncpy(Options::argv[i], argv[i], len);
    }

}

const char* Options::Get(const char* str) {
    assert(argv); /* parse must be called first */

    for (int i = 0; i < argc; i++) {
        if (!strncmp(str, Options::argv[i], strlen(str))) {
            return Options::argv[i] + strlen(str);
        }
    }

    return NULL;
}

void Options::Destroy()
{
    for (int i = 0;i < Options::argc;i++)
        delete [] argv[i];
    delete [] argv;
}
