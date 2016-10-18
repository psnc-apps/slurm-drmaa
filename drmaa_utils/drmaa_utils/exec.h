/* $Id: iter.h 13 2011-04-20 15:41:43Z mmamonski $ */
/*
 * PSNC DRMAA utilities library
 * Copyright (C) 2011-2012 Poznan Supercomputing and Networking Center
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DRMAA_UTILS__EXEC_H
#define __DRMAA_UTILS__EXEC_H

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <drmaa_utils/common.h>

/*max stdout and stderr buffer size: 1MB */
#define FSD_MAX_STREAM_BUFFER ( 1024 * 1024)

/**
 * Spawns a new command:
 * command - absolute command path
 * args - null terminated array
 * return value: exit code of the command
 */
int fsd_exec_sync(const char *command, char **args, const char *stdinb, char **stdoutb, char **stderrb);

#endif /* __DRMAA_UTILS__EXEC_H */
