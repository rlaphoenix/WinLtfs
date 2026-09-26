/*
**  %Z% %I% %W% %G% %U%
**
**  ZZ_Copyright_BEGIN
**
**
**  Licensed Materials - Property of IBM
**
**  IBM Linear Tape File System Single Drive Edition Version 2.2.0.2 for Linux and Mac OS X
**
**  Copyright IBM Corp. 2010, 2014
**
**  This file is part of the IBM Linear Tape File System Single Drive Edition for Linux and Mac OS X
**  (formally known as IBM Linear Tape File System)
**
**  The IBM Linear Tape File System Single Drive Edition for Linux and Mac OS X is free software;
**  you can redistribute it and/or modify it under the terms of the GNU Lesser
**  General Public License as published by the Free Software Foundation,
**  version 2.1 of the License.
**
**  The IBM Linear Tape File System Single Drive Edition for Linux and Mac OS X is distributed in the
**  hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
**  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**  See the GNU Lesser General Public License for more details.
**
**  You should have received a copy of the GNU Lesser General Public
**  License along with this library; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
**  or download the license from <http://www.gnu.org/licenses/>.
**
**
**  ZZ_Copyright_END
**
*************************************************************************************
**
** COMPONENT NAME:  IBM Linear Tape File System
**
** FILE NAME:       snmp/ltfssnmp.c
**
** DESCRIPTION:     Implements the snmp trap functions.
**
** AUTHORS:         Masahide Washizawa
**                  IBM Tokyo Lab., Japan
**                  washi@jp.ibm.com
**
*************************************************************************************
*/
#include <string.h>
#include <stdlib.h>

#include "ltfssnmp.h"

#define AGENT "ltfs"
#define TABLE_FILE_MODE "rb"

#define DEFAULT_DEFFILE LTFS_BASE_DIR "LtfsSnmpTrapDef.txt"

bool ltfs_snmp_enabled = false;

struct trap_entry {
	TAILQ_ENTRY(trap_entry) list;
	char *id;
};
TAILQ_HEAD(trap_struct, trap_entry) trap_entries;

bool is_snmp_enabled()
{
	return ltfs_snmp_enabled;
}

int read_trap_def_file(char *deffile)
{
	int ret = 0;
	char line[65536];
	char *trapfile=DEFAULT_DEFFILE;
	char *strip_pos, *tok, *saveptr;
	struct trap_entry *entry;
	FILE *fp;

	TAILQ_INIT(&trap_entries);

	if (deffile != NULL)
		trapfile = deffile;

	fp = fopen(trapfile, TABLE_FILE_MODE);
	if (! fp) {
		ret = -errno;
		ltfsmsg(LTFS_ERR, "11268E", trapfile, ret);
		return ret;
	}

	/* Parse the traf definition file */
	if (!ret) {
		while(fgets(line, 65536, fp) != NULL) {
			if (strlen(line) == 65535) {
				ltfsmsg(LTFS_ERR, "11269E");
				ret = -LTFS_CONFIG_INVALID;
				return ret;
			}
			/* Ignore comments and trailing whitespace */
			strip_pos = strstr(line, "#");
			if (! strip_pos)
				strip_pos = line + strlen(line);

			while (strip_pos > line &&
				(*(strip_pos - 1) == ' ' || *(strip_pos - 1) == '\t' ||
				 *(strip_pos - 1) == '\r' || *(strip_pos - 1) == '\n'))
				--strip_pos;
			*strip_pos = '\0';

			tok = strtok_r(line, " \t\r\n", &saveptr);
			if (tok) {
				entry = (struct trap_entry *) calloc(1, sizeof(struct trap_entry));
				if (! entry) {
					ltfsmsg(LTFS_ERR, "10001E", __FUNCTION__);
					return -LTFS_NO_MEMORY;
				}
				entry->id = strdup(tok);
				TAILQ_INSERT_TAIL(&trap_entries, entry, list);
			}
		}
		fclose(fp);
	}
	return ret;
}

bool is_snmp_trapid(const char *id)
{
	struct trap_entry *entry = NULL;
	if (id == NULL)
		return false;

	TAILQ_FOREACH(entry, &trap_entries, list) {
		if (! strcmp(entry->id, id))
			return true;
	}
	return false;
}

int ltfs_snmp_init(char *snmp_deffile)
{
	return 0;
}

int ltfs_snmp_finish()
{
	struct trap_entry *entry = NULL;
	TAILQ_FOREACH(entry, &trap_entries, list)
		free(entry->id);
	return 0;
}

int send_ltfsStartTrap(void)
{
	return 0;
}

int send_ltfsStopTrap(void)
{
	return 0;
}

int send_ltfsInfoTrap(char *str)
{
	return 0;
}

int send_ltfsErrorTrap(char *str)
{
	return 0;
}
