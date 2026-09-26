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
** FILE NAME:       main.c
**
** DESCRIPTION:     Implements the ltfs filesystem daemon.
**
** AUTHORS:         Brian Biskeborn
**                  IBM Almaden Research Center
**                  bbiskebo@us.ibm.com
**
**                  Lucas C. Villa Real
**                  IBM Almaden Research Center
**                  lucasvr@us.ibm.com
**
*************************************************************************************
**
**  (C) Copyright 2015, 2016 Hewlett Packard Enterprise Development LP
**  07/06/10 Mount cartridge as Read Only if it is flagged as having been written
**            past the Early Warning EOM point
**  10/13/17 Added support for SNIA 2.4
**
*************************************************************************************
**
** Copyright (C) 2012 OSR Open Systems Resources, Inc.
** 
************************************************************************************* 
*/

#include <dirent.h>

#include "ltfs_fuse.h"
#include "libltfs/ltfs.h"
#include "ltfs_copyright.h"
#include "libltfs/pathname.h"
#include "libltfs/plugin.h"
#include "libltfs/dcache.h"
#include "libltfs/index_criteria.h"
#if 0
#include "libltfs/ltfssnmp.h"
#endif /* 0 */
#include "libltfs/kmi.h"
#include "libltfs/tape.h"

volatile char *copyright = LTFS_COPYRIGHT_0"\n"LTFS_COPYRIGHT_1"\n"LTFS_COPYRIGHT_2"\n";

/* Defined in src/ltfs.c */
extern struct fuse_operations ltfs_ops;
/* Defined in messages/ */
extern char bin_ltfs_dat[];

/**
 * Command line parsing
 */
enum {
	KEY_HELP,
	KEY_VERSION,
	KEY_VERBOSE,
	KEY_ADVANCED_HELP,
};

#define LTFS_OPT(templ,offset,value) { templ, offsetof(struct ltfs_fuse_data, offset), value }
#define LTFS_OPT_KEY(templ, key)     { templ, -1U, key }

/* Forward declarations */
int single_drive_main(struct fuse_args *args, struct ltfs_fuse_data *priv);

static struct fuse_opt ltfs_options_pass1[] = {
	LTFS_OPT("config_file=%s",         config_file, 0),
	LTFS_OPT_KEY("-a",                 KEY_ADVANCED_HELP),
	FUSE_OPT_KEY("-h",                 KEY_HELP),
	FUSE_OPT_KEY("--help",             KEY_HELP),
	FUSE_OPT_KEY("-V",                 KEY_VERSION),
	FUSE_OPT_KEY("--version",          KEY_VERSION),
	FUSE_OPT_END
};

static struct fuse_opt ltfs_options[] = {
	LTFS_OPT("devname=%s",             devname, 0),
	LTFS_OPT("work_directory=%s",      work_directory, 0),
	LTFS_OPT("atime",                  atime, 1),
	LTFS_OPT("noatime",                atime, 0),
	LTFS_OPT("tape_backend=%s",        tape_backend_name, 0),
	LTFS_OPT("iosched_backend=%s",     iosched_backend_name, 0),
	LTFS_OPT("kmi_backend=%s",         kmi_backend_name, 0),
	LTFS_OPT("umask=%s",               force_umask, 0),
	LTFS_OPT("fmask=%s",               force_fmask, 0),
	LTFS_OPT("dmask=%s",               force_dmask, 0),
	LTFS_OPT("uid=%s",                 force_uid, 0),
	LTFS_OPT("gid=%s",                 force_gid, 0),
	LTFS_OPT("min_pool_size=%s",       force_min_pool, 0),
	LTFS_OPT("max_pool_size=%s",       force_max_pool, 0),
	LTFS_OPT("rules=%s",               index_rules, 0),
	LTFS_OPT("quiet",                  verbose, LTFS_WARN),
	LTFS_OPT("trace",                  verbose, LTFS_DEBUG),
	LTFS_OPT("syslogtrace",            verbose, LTFS_DEBUG * 100 + LTFS_DEBUG),
	LTFS_OPT("fulltrace",              verbose, LTFS_TRACE),
	LTFS_OPT("verbose=%d",             verbose, 0),
	LTFS_OPT("eject",                  eject, 1),
	LTFS_OPT("noeject",                eject, 0),
	LTFS_OPT("sync_type=%s",           sync_type_str, 0),
	LTFS_OPT("force_mount_no_eod",     skip_eod_check, 1),
	/*LTFS_OPT("device_list",            device_list, 1),*/
	LTFS_OPT("rollback_mount=%s",      rollback_str, 0),
	LTFS_OPT("release_device",         release_device, 1),
	LTFS_OPT("allow_other",            allow_other, 1),
	LTFS_OPT("noallow_other",          allow_other, 0),
	LTFS_OPT("capture_index",          capture_index, 1),
	LTFS_OPT("symlink_type=%s",        symlink_str, 0),
	LTFS_OPT("scsi_append_only_mode=%s", str_append_only_mode, 0),
	LTFS_OPT_KEY("-a",                 KEY_ADVANCED_HELP),
	FUSE_OPT_KEY("-h",                 KEY_HELP),
	FUSE_OPT_KEY("--help",             KEY_HELP),
	FUSE_OPT_KEY("-V",                 KEY_VERSION),
	FUSE_OPT_KEY("--version",          KEY_VERSION),
	FUSE_OPT_END
};

void single_drive_advanced_usage(const char *default_driver, struct ltfs_fuse_data *priv)
{
	ltfsresult("14401I");                        /* LTFS options: */
	ltfsresult("14413I", LTFS_CONFIG_FILE);      /* -o config_file=<file> */
	ltfsresult("14404I", LTFS_DEFAULT_WORK_DIR); /* -o work_directory=<dir> */
	ltfsresult("14414I");                        /* -o atime */
	ltfsresult("14440I");                        /* -o noatime */
	ltfsresult("14415I", default_driver);        /* -o tape_backend=<name> */
	ltfsresult("14416I", config_file_get_default_plugin("iosched", priv->config)); /* -o iosched_backend=<name> */
	/* We have disabled all messages related to 'kmi' by default. */
	/*ltfsresult("14455I", config_file_get_default_plugin("kmi", priv->config));*/ /* -o kmi_backend=<name> */
	ltfsresult("14417I");                        /* -o umask=<mode> */
	ltfsresult("14418I");                        /* -o fmask=<mode> */
	ltfsresult("14419I");                        /* -o dmask=<mode> */
	ltfsresult("14420I", LTFS_MIN_CACHE_SIZE_DEFAULT); /* -o min_pool_size=<num> */
	ltfsresult("14421I", LTFS_MAX_CACHE_SIZE_DEFAULT); /* -o max_pool_size=<num> */
	ltfsresult("14422I"); /* -o rules=<rule[,rule]> */
	ltfsresult("14423I"); /* -o quiet */
	ltfsresult("14405I"); /* -o trace */
	ltfsresult("14467I"); /* -o syslogtrace */
	ltfsresult("14424I"); /* -o fulltrace */
	ltfsresult("14441I", LTFS_INFO); /* -o verbose=<num> */
	ltfsresult("14425I"); /* -o eject */
	ltfsresult("14439I"); /* -o noeject */
	ltfsresult("14480I"); /* -o sync_type=type */
	ltfsresult("14443I"); /* -o force_mount_no_eod */
	/*ltfsresult("14436I");*/ /* -o device_list */
	ltfsresult("14437I"); /* -o rollback_mount */
	ltfsresult("14448I"); /* -o release_device */
	ltfsresult("14456I"); /* -o capture_index */
	ltfsresult("14463I"); /* -o scsi_append_only_mode=<on|off> */
	ltfsresult("14406I"); /* -a */
	/* TODO: future use for WORM */
	/* set worm rollback flag and rollback_str by this option */
	/* ltfsresult("14468I"); */ /* -o rollback_mount_no_eod */
}

void usage(char *progname, struct ltfs_fuse_data *priv)
{
	int ret;
	const char *default_driver = config_file_get_default_plugin("driver", priv->config);
	const char *default_device = NULL;

	if (! priv->advanced_help) {
		if (! priv->tape_backend_name)
			priv->tape_backend_name = default_driver;

		ret = plugin_load(&priv->driver_plugin, "driver", priv->tape_backend_name, priv->config);
		if (ret == 0)
			default_device = ltfs_default_device_name(priv->driver_plugin.ops);

		ltfsresult("14400I", progname);                   /* usage: %s mountpoint [options] */
		fprintf(stderr, "\n");
		ltfsresult("14401I");                             /* LTFS options: */
		if (default_device)
			ltfsresult("14402I", default_device);         /* -o devname=<dev> */
		else
			ltfsresult("14403I");                         /* -o devname=<dev> */
		ltfsresult("14404I", LTFS_DEFAULT_WORK_DIR);      /* -o work_directory=<dir> */
		ltfsresult("14405I");                             /* -o trace */
		ltfsresult("14425I");                             /* -o eject */
		ltfsresult("14480I", LONG_MAX / 60);              /* -o sync_type=type */
		ltfsresult("14443I");                             /* -o force_mount_no_eod */
		/*ltfsresult("14436I");*/                         /* -o device_list */
		ltfsresult("14437I");                             /* -o rollback_mount */
		ltfsresult("14448I");                             /* -o release_device */
		ltfsresult("14461I");                             /* -o symlink_type=type */
		ltfsresult("14406I");                             /* -a */
		ltfsresult("14407I");                             /* -V, --version */
		ltfsresult("14408I");                             /* -h, --help */
		fprintf(stderr, "\n");
		ltfsresult("14409I");                             /* FUSE options: */
		ltfsresult("14410I");                             /* -o umask=M */
		ltfsresult("14411I");                             /* -o uid=N */
		ltfsresult("14412I");                             /* -o gid=N */
		fprintf(stderr, "\n");
		fprintf(stderr, "\n");

		if (ret == 0)
			plugin_unload(&priv->driver_plugin);
	} else {
		fprintf(stderr, "\n");
		single_drive_advanced_usage(default_driver, priv);
		fprintf(stderr, "\n");
		plugin_usage(progname, "driver", priv->config);
		/* We will not be printing any messages related to 'kmi'. */
		/*plugin_usage("kmi", priv->config);*/
	}
}

mode_t parse_mode(char *input)
{
	if (! input || strlen(input) != 3)
		return (mode_t)-1;
	if (input[0] < '0' || input[0] > '7' ||
		input[1] < '0' || input[1] > '7' ||
		input[2] < '0' || input[2] > '7')
		return (mode_t)-1;
	return ((input[0] - '0') << 6) | ((input[1] - '0') << 3) | (input[2] - '0');
}

uid_t parse_uid(const char *input)
{
	return 0;
}

gid_t parse_gid(const char *input)
{
	return 0;
}

size_t parse_size_t(const char *input)
{
	const char *i;
	if (input[0] == '\0')
		return 0;
	for (i=input; *i; i++)
		if (*i < '0' || *i > '9')
			return 0;
	return strtoull(input, NULL, 10);
}

/**
 * Parse permissions, including mount-time overrides. This is roughly the same behavior as
 * NTFS-3g, except that fmask and dmask override umask regardless of argument order.
 */
int permissions_setup(struct ltfs_fuse_data *priv)
{
	mode_t mode;

	/* Set defaults */
	priv->perm_override = false;
	priv->mount_uid = geteuid();
	priv->mount_gid = getegid();
	priv->file_mode = S_IFREG | 0777;
	priv->dir_mode = S_IFDIR | 0777;

	/* User ID override */
	if (priv->force_uid) {
		priv->perm_override = true;
		priv->mount_uid = parse_uid(priv->force_uid);
		if (priv->mount_uid == (uid_t)-1) {
			/* Invalid UID */
			ltfsmsg(LTFS_ERR, "14079E", priv->force_uid);
			return -1;
		}
		free(priv->force_uid);
	}

	/* Group ID override */
	if (priv->force_gid) {
		priv->perm_override = true;
		priv->mount_gid = parse_gid(priv->force_gid);
		if (priv->mount_gid == (gid_t)-1) {
			/* Invalid GID */
			ltfsmsg(LTFS_ERR, "14080E", priv->force_gid);
			return -1;
		}
		free(priv->force_gid);
	}

	/* Global (file and directory) permissions override */
	if (priv->force_umask) {
		priv->perm_override = true;
		mode = parse_mode(priv->force_umask);
		if (mode == (mode_t)-1) {
			/* Invalid umask */
			ltfsmsg(LTFS_ERR, "14006E", priv->force_umask);
			return -1;
		}
		priv->file_mode = (S_IFREG | 0777) & ~mode;
		priv->dir_mode = (S_IFDIR | 0777) & ~mode;
		free(priv->force_umask);
	}

	/* File permissions override */
	if (priv->force_fmask) {
		priv->perm_override = true;
		mode = parse_mode(priv->force_fmask);
		if (mode == (mode_t)-1) {
			/* Invalid fmask */
			ltfsmsg(LTFS_ERR, "14007E", priv->force_fmask);
			return -1;
		}
		priv->file_mode = (S_IFREG | 0777) & ~mode;
		free(priv->force_fmask);
	}

	/* Directory permissions override */
	if (priv->force_dmask) {
		priv->perm_override = true;
		mode = parse_mode(priv->force_dmask);
		if (mode == (mode_t)-1) {
			/* Invalid dmask */
			ltfsmsg(LTFS_ERR, "14008E", priv->force_dmask);
			return -1;
		}
		priv->dir_mode = (S_IFDIR | 0777) & ~mode;
		free(priv->force_dmask);
	}

/* Uncomment to apply the current umask to the default permissions, as vfat does.
	mode = umask(0);
	umask(mode);
	if (! priv->force_umask && ! priv->force_fmask)
		priv->file_mode = (S_IFREG | 0777) & ~mode;
	if (! priv->force_umask && ! priv->force_dmask)
		priv->dir_mode = (S_IFDIR | 0777) & ~mode;
*/

	return 0;
}

int ltfs_parse_options(void *priv_data, const char *arg, int key, struct fuse_args *outargs)
{
	struct ltfs_fuse_data *priv = (struct ltfs_fuse_data *) priv_data;
	const char *fuse_options[] = { "-f", "-d", "-s", NULL };
	bool valid_fuse_option = false;
	int i;

	switch(key) {
		case KEY_VERSION:
			ltfsresult("14464I", LTFS_VENDOR_NAME SOFTWARE_PRODUCT_NAME, PACKAGE_VERSION, LTFS_BUILD_VERSION);
			ltfsresult("14058I", "LTFS Format Specification", LTFS_INDEX_VERSION_STR);
			exit(0);
		case KEY_ADVANCED_HELP:
			priv->advanced_help = true;
			valid_fuse_option = false;
			/* fall through */
		case FUSE_OPT_KEY_OPT:
		case FUSE_OPT_KEY_NONOPT:
			for (i=0; arg && fuse_options[i]; ++i) {
				if (! strcmp(arg, fuse_options[i])) {
					valid_fuse_option = true;
					break;
				}
			}
			if (! priv->advanced_help) {
				if (! valid_fuse_option && key == FUSE_OPT_KEY_OPT && arg && arg[0] == '-') {
					/* invalid option */
					ltfsmsg(LTFS_ERR, "9010E", arg);
				} else
					break;
			}
			/* fall through */
		case KEY_HELP:
		default:
			if (! priv->first_parsing_pass) {
				fuse_opt_add_arg(outargs, "-h");
				if (priv->advanced_help)
					fuse_main(outargs->argc, outargs->argv, &ltfs_ops, NULL);
				usage(outargs->argv[0], priv);
				exit(key == KEY_HELP ? 0 : 1);
			}
	}

	return 1;
}

static int create_workdir(struct ltfs_fuse_data *priv)
{
	struct stat statbuf;
	int ret;

	ret = stat(priv->work_directory, &statbuf);
	if (ret < 0) {
		ret = mkdir_p(priv->work_directory, S_IRWXU | S_IRWXG | S_IRWXO);
		if (ret < 0) {
			/* Failed to create work directory */
			ltfsmsg(LTFS_ERR, "14004E", ret);
			return ret;
		}
	} else if (! S_ISDIR(statbuf.st_mode)) {
		/* Path exists but is not a directory */
		ltfsmsg(LTFS_ERR, "14005E", priv->work_directory);
		return -ENOTDIR;
	}

	return 0;
}

int validate_sync_option(struct ltfs_fuse_data *priv)
{
	char *sync_time_str, *end_time_str;

	/* Search time description and devide option string*/
	if (priv->sync_type_str) {
		priv->sync_time = -1;
		sync_time_str = strchr(priv->sync_type_str, '@');
		if (sync_time_str) {
			*sync_time_str = '\0';
			sync_time_str++;
		}
	} else {
		priv->sync_type = LTFS_SYNC_TIME;
		priv->sync_time = LTFS_SYNC_PERIOD_DEFAULT;
		return 0;
	}

	/* Detect sync type */
	if (strcasecmp(priv->sync_type_str, "time") == 0)
		priv->sync_type = LTFS_SYNC_TIME;
	else if (strcasecmp(priv->sync_type_str, "close") == 0)
		priv->sync_type = LTFS_SYNC_CLOSE;
	else if (strcasecmp(priv->sync_type_str, "unmount") == 0)
		priv->sync_type = LTFS_SYNC_UNMOUNT;
	else {
		ltfsmsg(LTFS_ERR, "14061E", priv->sync_type_str);
		return 1;
	}

	/* If type is sync by time, convert option to sync time */
	if (priv->sync_type == LTFS_SYNC_TIME) {
		if (sync_time_str && strlen(sync_time_str) != 0) {
			errno = 0;
			priv->sync_time = strtol(sync_time_str, &end_time_str, 10);
			if (sync_time_str == end_time_str) {
				ltfsmsg(LTFS_ERR, "14060E", sync_time_str);
				return 1;
			}
			if ((priv->sync_time == LONG_MAX || priv->sync_time == LONG_MIN) && errno != 0) {
				ltfsmsg(LTFS_ERR, "14067E", sync_time_str);
				return 1;
			}
			if (priv->sync_time <= 0) {
				ltfsmsg(LTFS_ERR, "14062E");
				return 1;
			}
			if (priv->sync_time > (LONG_MAX / 60) || priv->sync_time < (LONG_MIN / 60)) {
				ltfsmsg(LTFS_ERR, "14068E", priv->sync_time);
				return 1;
			}
			priv->sync_time *= 60; /* Convert minutes to seconds*/
		} else
			priv->sync_time = LTFS_SYNC_PERIOD_DEFAULT;
	} else
		priv->sync_time = LTFS_SYNC_PERIOD_DEFAULT;

	/* Restore original string*/
	if (sync_time_str) {
		sync_time_str--;
		*sync_time_str = '@';
	}

	return 0;
}

static int show_device_list(struct ltfs_fuse_data *priv)
{
	int ret;

	/* Load tape backend */
	if (priv->tape_backend_name == NULL)
		priv->tape_backend_name = config_file_get_default_plugin("driver", priv->config);
	ret = plugin_load(&priv->driver_plugin, "driver", priv->tape_backend_name, priv->config);
	if (ret < 0) {
		ltfsmsg(LTFS_ERR, "14054E", ret);
		return 1;
	}

	/* Print device list */
	ret = ltfs_print_device_list(priv->driver_plugin.ops);

	/* Unload tape backend */
	plugin_unload(&priv->driver_plugin);

	return ret ? 1 : 0;
}

int main(int argc, char **argv)
{
	int ret, i, cmd_args_len;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct ltfs_fuse_data *priv = (struct ltfs_fuse_data *) calloc(1, sizeof(struct ltfs_fuse_data));
	char *lang, **mount_options, **snmp_options, *cmd_args;
	void *message_handle;

	/* Suppress warning on Windows builds. */
	(void) lang;
	(void) mount_options;

	priv->verbose = LTFS_INFO;
	priv->allow_other = (geteuid() == 0) ? 1 : 0;
	priv->pid_orig = getpid();


	/* Start up libltfs with the default logging level. User overrides are
	 * processed later, after command line parsing. */
	openlog("ltfs", LOG_PID, LOG_USER);
	ret = ltfs_init(LTFS_INFO, true, true);
	if (ret < 0) {
		/* Failed to initialize libltfs */
		ltfsmsg(LTFS_ERR, "10000E", ret);
	}

	/* Register messages with libltfs */
	ret = ltfsprintf_load_plugin("bin_ltfs", bin_ltfs_dat, &message_handle);
	if (ret < 0) {
		ltfsmsg(LTFS_ERR, "10012E", ret);
		return 1;
	}

	if (! priv) {
		ltfsmsg(LTFS_ERR, "10001E", "main: private data");
		return 1;
	}

	/* Parse command line options to pick up the configuration file */
	priv->first_parsing_pass = true;
	ret = fuse_opt_parse(&args, priv, ltfs_options_pass1, ltfs_parse_options);
	if (ret < 0) {
		ltfsmsg(LTFS_ERR, "9001E");
		return 1;
	}

	/* Load the configuration file */
	ret = config_file_load(priv->config_file, &priv->config);
	if (ret < 0) {
		ltfsmsg(LTFS_ERR, "10008E", ret);
		return 1;
	}

	/* We don't use this in HPE-SOS. The code is present but it will always endup indicating that
	 * snmp is disabled.
	 */
	/* Get snmp option value from configuration file */
	snmp_options = config_file_get_options("snmp", priv->config);
	if (snmp_options) {
		priv->snmp_enabled = false;
		for (i=0; snmp_options[i]; ++i) {
			if (! strcmp(snmp_options[i], "enabled"))
				priv->snmp_enabled = true;
			else if (! strncmp(snmp_options[i], "deffile ", 8)) {
				ret = asprintf(&priv->snmp_deffile, "%s", snmp_options[i]+8);
				if (ret < 0) {
					ltfsmsg(LTFS_ERR, "10001E", "library_main: snmp_deffile");
					priv->snmp_enabled = false;
					break;
				}
			}
		}
		if (priv->snmp_enabled) {
#if 0
			ltfs_snmp_init(priv->snmp_deffile);
#endif /* 0 */
		}
	}
	
	/* Not supported on Windows. TODO: Verify this again.*/
	/* Bring in extra mount options set in the config file */
	mount_options = config_file_get_options("single-drive", priv->config);
	if (! mount_options)
		return 1;
	for (i=0; mount_options[i]; ++i) {
		ret = fuse_opt_insert_arg(&args, i+1, mount_options[i]);
		if (ret < 0) {
			/* Could not enable FUSE option */
			ltfsmsg(LTFS_ERR, "14001E", mount_options[i], ret);
			return 1;
		}
		free(mount_options[i]);
	}
	free(mount_options);

	/* Parse command line options again, this time for real */
	priv->first_parsing_pass = false;
	ret = fuse_opt_parse(&args, priv, ltfs_options, ltfs_parse_options);
	if (ret < 0) {
		ltfsmsg(LTFS_ERR, "9001E");
		return 1;
	}

	/* Set the logging level */
	if (priv->verbose > 100)
		ltfs_set_syslog_level(priv->verbose / 100);
	ltfs_set_log_level(priv->verbose % 100);

	/* LTFS starting */
	ltfsmsg(LTFS_INFO, "14000I", LTFS_VENDOR_NAME SOFTWARE_PRODUCT_NAME, PACKAGE_VERSION, priv->verbose);
	ltfsmsg(LTFS_INFO, "14058I", "LTFS Format Specification", LTFS_INDEX_VERSION_STR);

	/* Show command line arguments */
	for (i = 0, cmd_args_len = 0 ; i < argc; i++) {
		cmd_args_len += strlen(argv[i]) + 1;
	}
	cmd_args = calloc(1, cmd_args_len + 1);
	if (!cmd_args) {
		/* Memory allocation failed */
		ltfsmsg(LTFS_ERR, "10001E", "ltfs (arguments)");
		return -ENOMEM;
	}
	strcat(cmd_args, argv[0]);
	for (i = 1; i < argc; i++) {
		strcat(cmd_args, " ");
		strcat(cmd_args, argv[i]);
	}
	ltfsmsg(LTFS_INFO, "14104I", cmd_args);
	free(cmd_args);

	/* Show build time information */
	ltfsmsg(LTFS_INFO, "14105I", BUILD_SYS_FOR);
	ltfsmsg(LTFS_INFO, "14106I", BUILD_SYS_GCC);

	/* Show run time information */
	show_runtime_system_info();

	/* Show avaliable tape device list */
	if (priv->device_list) {
		ret = show_device_list(priv);
		ltfs_finish();
		return (ret != 0) ? 0 : 1;
	}

	/* Validate sync option */
	ret = validate_sync_option(priv);
	if (ret != 0)
		return 1; /* Error message was already displayed */

	/* Print the active sync mode */
	switch(priv->sync_type) {
		case LTFS_SYNC_TIME:
			ltfsmsg(LTFS_INFO, "14063I", "time", priv->sync_time);
			break;
		case LTFS_SYNC_CLOSE:
			ltfsmsg(LTFS_INFO, "14064I", "close");
			break;
		case LTFS_SYNC_UNMOUNT:
			ltfsmsg(LTFS_INFO, "14064I", "unmount");
			break;
		default:
			ltfsmsg(LTFS_ERR, "14065E", priv->sync_type);
			return 1;
	}

	/* Enable correct permissions checking in the kernel if any permission overrides are set  */
	/*********************************************************************************/
	/* HPE Change  : For Mac with OSXFUSE, use defer_permissions so that the fs is   */
	/* 16-Aug-18     left to make decisions rather than the kernel thinking it knows */
	/* CR11168       best... used to cause 'Operation not permitted' errors...       */
	/*********************************************************************************/
	ret = fuse_opt_add_arg(&args, "-odefault_permissions");
	if (ret < 0) {
		/* Could not enable FUSE option */
		ltfsmsg(LTFS_ERR, "14001E", "default_permissions", ret);
		return 1;
	}

	/* Let all users access this filesystem by default */
	if (priv->allow_other) {
		ret = fuse_opt_add_arg(&args, "-oallow_other");
		if (ret < 0) {
			/* Could not enable FUSE option */
			ltfsmsg(LTFS_ERR, "14001E", "allow_other", ret);
			return 1;
		}
	}

	/* Unlink objects from the file system instead of having them renamed to .fuse_hidden */
	ret = fuse_opt_add_arg(&args, "-ohard_remove");
	if (ret < 0) {
		/* Could not enable FUSE option */
		ltfsmsg(LTFS_ERR, "14001E", "hard_remove", ret);
		return 1;
	}

	/* perform reads synchronously */
	ret = fuse_opt_add_arg(&args, "-osync_read");
	if (ret < 0) {
		/* Could not enable FUSE option */
		ltfsmsg(LTFS_ERR, "14001E", "sync_read", ret);
		return 1;
	}


#if FUSE_VERSION >= 28
	/* For FUSE 2.8 or higher, automatically enable big_writes */
	ret = fuse_opt_add_arg(&args, "-obig_writes");
	if (ret < 0) {
		/* Could not enable FUSE option */
		ltfsmsg(LTFS_ERR, "14001E", "big_writes", ret);
		return 1;
	}
#endif

	/* Set up permissions based on mount options and current user information */
	ret = permissions_setup(priv);
	if (ret < 0) {
		/* Failed to set up permissions */
		ltfsmsg(LTFS_ERR, "14002E", ret);
		usage(argv[0], priv);
		return 1;
	}

	/* Bring in some configuration defaults if needed */
	if (priv->tape_backend_name == NULL) {
		priv->tape_backend_name = config_file_get_default_plugin("driver", priv->config);
		if (priv->tape_backend_name == NULL) {
			/* No driver plugin configured and no default found */
			ltfsmsg(LTFS_ERR, "14056E");
			return 1;
		}
	}
	if (priv->iosched_backend_name == NULL)
		priv->iosched_backend_name = config_file_get_default_plugin("iosched", priv->config);
	if (priv->iosched_backend_name && strcmp(priv->iosched_backend_name, "none") == 0)
		priv->iosched_backend_name = NULL;
	if (priv->kmi_backend_name == NULL)
		priv->kmi_backend_name = config_file_get_default_plugin("kmi", priv->config);
	if (priv->kmi_backend_name && strcmp(priv->kmi_backend_name, "none") == 0)
		priv->kmi_backend_name = NULL;
	if (priv->work_directory == NULL || ! strcmp(priv->work_directory, ""))
		priv->work_directory = LTFS_DEFAULT_WORK_DIR;
	if (priv->force_min_pool) {
		priv->min_pool_size = parse_size_t(priv->force_min_pool);
		if (priv->min_pool_size == 0) {
			ltfsmsg(LTFS_ERR, "14109E");
			return 1;
		}
	} else
		priv->min_pool_size = LTFS_MIN_CACHE_SIZE_DEFAULT;
	if (priv->force_max_pool) {
		priv->max_pool_size = parse_size_t(priv->force_max_pool);
		if (priv->max_pool_size == 0) {
			ltfsmsg(LTFS_ERR, "14110E");
			return 1;
		}
	} else
		priv->max_pool_size = LTFS_MAX_CACHE_SIZE_DEFAULT;
	if (priv->min_pool_size > priv->max_pool_size) {
		/* Min pool size cannot be greater than max pool size */
		ltfsmsg(LTFS_ERR, "14003E", priv->min_pool_size, priv->max_pool_size);
		return 1;
	}

	/*
	 * Make sure at least one parameter was provided (mount point).  
	 * If exactly one param, check if it's an accessible directory.
	 * With more than one param, have to defer to fuse_main to find if valid.
	 */
	if (argc < 2) {
		ltfsmsg(LTFS_ERR, "14200E");  /* missing mountpoint parameter */
		usage (argv[0], priv);
		return 1;

	}
	/* OSR
	 *
	 * In our MinGW environment, the mount point does not exist when
	 * the file system is executed
	 */

	/* Make sure work directory exists */
	ret = create_workdir(priv);
	if (ret < 0) {
		if (priv->work_directory) {
			free((void *)priv->work_directory);
			priv->work_directory = NULL;
			priv->work_directory = LTFS_DEFAULT_WORK_DIR;
		}
	}

	/* Load plugins */
	ret = plugin_load(&priv->driver_plugin, "driver", priv->tape_backend_name, priv->config);
	if (ret < 0) {
		ltfsmsg(LTFS_ERR, "14054E", ret);
		return 1;
	}
	if (priv->iosched_backend_name) {
		ret = plugin_load(&priv->iosched_plugin, "iosched", priv->iosched_backend_name,
			priv->config);
		if (ret < 0) {
			ltfsmsg(LTFS_ERR, "14055E", ret);
			return 1;
		}
	}
	if (priv->kmi_backend_name) {
		ret = plugin_load(&priv->kmi_plugin, "kmi", priv->kmi_backend_name,
			priv->config);
		if (ret < 0) {
			ltfsmsg(LTFS_ERR, "14057E", ret);
			return 1;
		}
	}

	/* Make sure we have a device name */
	if (! priv->devname) {
		priv->devname = ltfs_default_device_name(priv->driver_plugin.ops);
		if (! priv->devname) {
			/* The backend \'%s\' does not have a default device */
			ltfsmsg(LTFS_ERR, "14009E", priv->tape_backend_name);
			return 1;
		}
	}

	/* Initialize filesystem component in libltfs */
	ret = ltfs_fs_init();
	if (ret)
		return 1;

	/* Invoke the single drive main operations */
	ret = ltfs_mutex_init(&priv->file_table_lock);
	if (ret) {
		/*  Cannot initialize open file table */
		ltfsmsg(LTFS_ERR, "14114E");
		return 1;
	}
	ret = single_drive_main(&args, priv);

	/* Send a trap of LTFS termination */
#if 0
	if (priv->snmp_enabled)
		ltfs_snmp_finish();
#endif /* 0 */

	/* Unload plugins */
	if (priv->iosched_backend_name)
		plugin_unload(&priv->iosched_plugin);
	if (priv->kmi_backend_name)
		plugin_unload(&priv->kmi_plugin);
	plugin_unload(&priv->driver_plugin);

	/* Free data structures */
	ltfsprintf_unload_plugin(message_handle);
	ltfs_finish();
	config_file_free(priv->config);
	free(priv);
	fuse_opt_free_args(&args);

	return ret;
}

int single_drive_main(struct fuse_args *args, struct ltfs_fuse_data *priv)
{
	int ret;
	char *index_rules_utf8;
	char fsname[strlen(priv->devname) + 16];
	char *invalid_start;
	char *mountpoint = NULL;
	struct fuse_args tmpa=FUSE_ARGS_INIT(0, NULL);
	int i;
	bool is_worm;
	
	(void) i;
	(void) tmpa;
	(void) mountpoint;

	/*  Setup signal handler to terminate cleanly */
	ret = ltfs_set_signal_handlers();
	if (ret < 0) {
		ltfsmsg(LTFS_ERR, "10013E");
		return 1;
	}

	/* Validate rollback_mount option */
	if (priv->rollback_str) {
		errno = 0;
		priv->rollback_gen = strtoul(priv->rollback_str, &invalid_start, 0);
		if( (*invalid_start != '\0') || priv->rollback_gen == 0 ) {
			ltfsmsg(LTFS_ERR, "14091E", priv->rollback_str);
			return 1;
		}
	}

	/* Validate append_only_mode */
	if (priv->str_append_only_mode) {
		if (strcasecmp(priv->str_append_only_mode, "on") == 0)
			priv->append_only_mode = 1;
		else if (strcasecmp(priv->str_append_only_mode, "off") == 0)
			priv->append_only_mode = 0;
		else {
			ltfsmsg(LTFS_ERR, "14115E", priv->str_append_only_mode);
			return 1;
		}
	} else {
		priv->append_only_mode = 0;
	}

        /*
         * HPE change: CR 11358 : 08 Oct 18
         *  We use partial unload to switch out of append-only mode, so don't need to 
         *  enforce the eject requirement.  So this whole clause can be commented out.
         */
	//if (priv->eject == 0 && priv->append_only_mode != 0) {
	//	/* Append only mode need to eject a cartridge at unmount to clear the mode on drive setting */
	//	/* To avoid cartridge ejection at unmount, disable append only mode at the moount with noeject option */
	//	priv->append_only_mode = 0;
	//	/*ltfsmsg(LTFS_INFO, "14095I");*/
	//}

	/* If the local inode space is big enough, have FUSE pass through our UIDs as inode
	 * numbers instead of generating its own. */
	if (sizeof(ino_t) >= 8) {
		ret = fuse_opt_add_arg(args, "-ouse_ino");
		if (ret < 0) {
			/* Could not enable FUSE option */
			ltfsmsg(LTFS_ERR, "14001E", "use_ino", ret);
			return 1;
		}
	}

	/* Set file system name to "ltfs:devname" in case FUSE doesn't pick it up */
	snprintf(fsname, sizeof(fsname), "-ofsname=ltfs:%s", priv->devname);
	ret = fuse_opt_add_arg(args, fsname);
	if (ret < 0) {
		/* Could not enable FUSE option */
		ltfsmsg(LTFS_ERR, "14001E", "fsname", ret);
		return 1;
	}

	/* Allocate the LTFS volume structure */
	if (ltfs_volume_alloc("ltfs", &priv->data) < 0) {
		/* Could not allocate LTFS volume structure */
		ltfsmsg(LTFS_ERR, "14011E");
		return 1;
	}
	ltfs_use_atime(priv->atime, priv->data);


    /*
	 * OSR
	 *
	 * In our MinGW environment, we mount the device on access. This
	 * processing is deferred until ltfs_fuse_mount
	 *
	 */
#ifndef HPE_mingw_BUILD
	if (ltfs_device_open(priv->devname, priv->driver_plugin.ops, priv->data) < 0) {
		/* Could not open device */
		ltfsmsg(LTFS_ERR, "10004E", priv->devname);
		ltfs_volume_free(&priv->data);
		return 1;
	}

	if (priv->release_device) {
		ltfs_release_medium(priv->data);
		ltfs_device_close(priv->data);
		ltfs_volume_free(&priv->data);
		return 0;
	}
#endif /* HPE_mingw_BUILD */
	/* OSR */
	/* Save the arguments so we can parse them later at the init
	 * callback
	 */
	priv->args = args;


	/*  Cleanup signal handler */
	ret = ltfs_unset_signal_handlers();
	if (ret < 0) {
		ltfsmsg(LTFS_ERR, "10014E");
		return 1;
	}


	/* Not supported on Windows. TODO: Verify this again.*/


	/* A cached EA must not outlive a media change, even for CLI mounts. */
	ret = fuse_opt_add_arg(args, "-oEaTimeout=0");
	if (ret < 0)
		return 1;

	/* now we can safely call FUSE */
	ltfsmsg(LTFS_INFO, "14111I");
	ltfsmsg(LTFS_INFO, "14112I");
	ltfsmsg(LTFS_INFO, "14113I");
	ret = fuse_main(args->argc, args->argv, &ltfs_ops, priv);

	/*  Setup signal handler again to terminate cleanly */
	ret = ltfs_set_signal_handlers();
	if (ret < 0) {
		ltfsmsg(LTFS_ERR, "10013E");
		return 1;
	}

	/*
	 * OSR
	 *
	 * In our MinGW environment, we stay running after the device is
	 * dismounted/ejected. This processing is deferred until
	 * ltfs_fuse_unmount
	 *
	 */

	/* close the volume */


	ltfs_unset_signal_handlers();

	return ret;
}
