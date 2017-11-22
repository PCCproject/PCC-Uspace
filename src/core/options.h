
/*
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

#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include <assert.h>
#include <iostream>
#include <string.h>

namespace Options {
    /**
     * Make global copies of argc and argv for use
     * by get().
     */
    void parse(int argc, char** argv);

    /**
     * Get the argument that starts with str. Returns
     * the argument on success, NULL on failure.
     */
    const char* get(const char* str);

    /**
     * Deallocates memory allocated by parse().
     */
    void destroy();
}

#endif
