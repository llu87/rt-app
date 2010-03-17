/* 
This file is part of rt-app - https://launchpad.net/rt-app
Copyright (C) 2010  Giacomo Bagnoli <g.bagnoli@asidev.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/ 

#include "rt-app_parse_config.h"

#define PFX "[json] "
#define PFL "         "PFX
#define PIN PFX"    "
#define PIN2 PIN"    "
#define JSON_FILE_BUF_SIZE 4096

/* redefine foreach as in <json/json_object.h> but to be ANSI
 * compatible */
#define foreach(obj, entry, key, val, idx)				\
	for ( ({ idx = 0; entry = json_object_get_object(obj)->head;});	\
		({ if (entry) { key = (char*)entry->k;			\
				val = (struct json_object*)entry->v;	\
			      };					\
		   entry;						\
		 }							\
		);							\
		({ entry = entry->next; idx++; })			\
	    )								

#define set_default_if_needed(key, value, have_def, def_value) do {	\
	if (!value) {							\
		if (have_def) {						\
			log_debug(PIN "key: %s <default> %d", key, def_value);\
			return def_value;				\
		} else {						\
			log_critical(PFX "Key %s not found", key);	\
			exit(EXIT_INV_CONFIG);				\
		}							\
	}								\
} while(0)	

#define set_default_if_needed_str(key, value, have_def, def_value) do {	\
	if (!value) {							\
		if (have_def) {						\
			if (!def_value) {				\
				log_debug(PIN "key: %s <default> NULL", key);\
				return NULL;				\
			}						\
			log_debug(PIN "key: %s <default> %s",		\
				  key, def_value);			\
			return strdup(def_value);			\
		} else {						\
			log_critical(PFX "Key %s not found", key);	\
			exit(EXIT_INV_CONFIG);				\
		}							\
	}								\
}while (0)

static inline void
assure_type_is(struct json_object *obj,
	       struct json_object *parent,
	       const char *key,
	       enum json_type type)
{
	if (!json_object_is_type(obj, type)) {
		log_critical("Invalid type for key %s", key);
		log_critical("%s", json_object_to_json_string(parent));
		exit(EXIT_INV_CONFIG);
	}
}

static inline struct json_object* 
get_in_object(struct json_object *where, 
	      const char *what,
	      boolean nullable)
{
	struct json_object *to;	
	to = json_object_object_get(where, what);
	if (is_error(to)) {
		log_critical(PFX "Error while parsing config:\n" PFL 
			  "%s", json_tokener_errors[-(unsigned long)to]);
		exit(EXIT_INV_CONFIG);
	}
	if (!nullable && strcmp(json_object_to_json_string(to), "null") == 0) {
		log_critical(PFX "Cannot find key %s", what);
		exit(EXIT_INV_CONFIG);
	}
	return to;
}

static inline int
get_int_value_from(struct json_object *where,
		   const char *key,
		   boolean have_def,
		   int def_value)
{
	struct json_object *value;
	int i_value;
	value = get_in_object(where, key, have_def);
	set_default_if_needed(key, value, have_def, def_value);
	assure_type_is(value, where, key, json_type_int);
	i_value = json_object_get_int(value);
	json_object_put(value);
	log_debug(PIN "key: %s, value: %d, type <int>", key, i_value);
	return i_value;
}

static inline int
get_bool_value_from(struct json_object *where,
		    const char *key,
		    boolean have_def,
		    int def_value)
{
	struct json_object *value;
	boolean b_value;
	value = get_in_object(where, key, have_def);
	set_default_if_needed(key, value, have_def, def_value);
	assure_type_is(value, where, key, json_type_boolean);
	b_value = json_object_get_boolean(value);
	json_object_put(value);
	log_debug(PIN "key: %s, value: %d, type <bool>", key, b_value);
	return b_value;
}

static inline char*
get_string_value_from(struct json_object *where,
		      const char *key,
		      boolean have_def,
		      const char *def_value)
{
	struct json_object *value;
	char *s_value;
	value = get_in_object(where, key, have_def);
	set_default_if_needed_str(key, value, have_def, def_value);
	if (json_object_is_type(value, json_type_null)) {
		log_debug(PIN "key: %s, value: NULL, type <string>", key);
		return NULL;
	}
	assure_type_is(value, where, key, json_type_string);
	s_value = strdup(json_object_get_string(value));
	json_object_put(value);
	log_debug(PIN "key: %s, value: %s, type <string>", key, s_value);
	return s_value;
}

static void
parse_resources(struct json_object *resources, rtapp_options_t *opts)
{
	// TODO
}

static void
parse_thread_data(char *name, struct json_object *obj, int idx, 
		  thread_data_t *data, const rtapp_options_t *opts)
{
	long exec, period, dline;
	char *policy;

	log_debug(PFX "Parsing thread %s [%d]", name, idx);
	data->ind = idx;
	data->lock_pages = opts->lock_pages;
	data->sched_prio = DEFAULT_THREAD_PRIORITY;
	data->cpuset = NULL;
	data->cpuset_str = NULL;
	period = get_int_value_from(obj, "period", FALSE, 0);
	if (period <= 0) {
		log_critical(PIN2 "Cannot set negative period");
		exit(EXIT_INV_CONFIG);
	}
	data->period = usec_to_timespec(period);

	exec = get_int_value_from(obj, "exec", FALSE, 0);
	if (exec > period) {
		log_critical(PIN2 "Exec must be greather than period");
		exit(EXIT_INV_CONFIG);
	}
	if (exec < 0) {
		log_critical(PIN2 "Cannot set negative exec time");
		exit(EXIT_INV_CONFIG);
	}
	data->min_et = usec_to_timespec(exec);
	data->max_et = usec_to_timespec(exec);
	
	policy = get_string_value_from(obj, "policy", TRUE, NULL);
	if (policy) {
		if (string_to_policy(policy, &data->sched_policy) != 0) {
			log_critical(PIN2 "Invalid policy %s", policy);
			exit(EXIT_INV_CONFIG);
		}
	}
	policy_to_string(data->sched_policy, data->sched_policy_descr);
	
	dline = get_int_value_from(obj, "deadline", TRUE, period);
	if (dline < exec) {
		log_critical(PIN2 "Deadline cannot be less than exec time");
		exit(EXIT_INV_CONFIG);
	}
	if (dline > period) {
		log_critical(PIN2 "Deadline cannot be greater than period");
		exit(EXIT_INV_CONFIG);
	}
	data->deadline = usec_to_timespec(dline);

}

static void
parse_tasks(struct json_object *tasks, rtapp_options_t *opts)
{
	/* used in the foreach macro */
	struct lh_entry *entry; char *key; struct json_object *val; int idx;

	log_debug(PFX "Parsing threads section");
	opts->nthreads = 0;
	foreach(tasks, entry, key, val, idx) {
		opts->nthreads++;
	}
	log_debug(PFX "Found %d threads", opts->nthreads);
	opts->threads_data = malloc(sizeof(thread_data_t) * opts->nthreads);
	foreach (tasks, entry, key, val, idx) {
		parse_thread_data(key, val, idx, &opts->threads_data[idx], opts);
	}
}

static void
parse_global(struct json_object *global, rtapp_options_t *opts)
{
	char *policy;
	log_debug(PFX "Parsing global section");
	opts->spacing = get_int_value_from(global, "spacing", TRUE, 0);
	opts->duration = get_int_value_from(global, "duration", TRUE, -1);
	opts->gnuplot = get_bool_value_from(global, "gnuplot", TRUE, 0);
	policy = get_string_value_from(global, "default_policy", 
				       TRUE, "SCHED_OTHER");
	if (string_to_policy(policy, &opts->policy) != 0) {
		log_critical(PFX "Invalid policy %s", policy);
		exit(EXIT_INV_CONFIG);
	}
	opts->logdir = get_string_value_from(global, "logdir", TRUE, NULL);
	opts->lock_pages = get_bool_value_from(global, "lock_pages", TRUE, 1);
	opts->logbasename = get_string_value_from(global, "log_basename", 
						  TRUE, "rt-app");
#ifdef AQUOSA
	opts->fragment = get_int_value_from(global, "fragment", TRUE, 1);
#endif
	
}

static void
get_opts_from_json_object(struct json_object *root, rtapp_options_t *opts)
{
	struct json_object *global, *tasks, *resources;

	if (is_error(root)) {
		log_error(PFX "Error while parsing input JSON: %s",
			 json_tokener_errors[-(unsigned long)root]);
		exit(EXIT_INV_CONFIG);
	}
	log_info(PFX "Successfully parsed input JSON");
	log_debug(PFX "root     : %s", json_object_to_json_string(root));
	
	global = get_in_object(root, "global", FALSE);
	log_debug(PFX "global   : %s", json_object_to_json_string(global));
	
	tasks = get_in_object(root, "tasks", FALSE);
	log_debug(PFX "tasks    : %s", json_object_to_json_string(tasks));
	
	resources = get_in_object(root, "resources", FALSE);
	log_debug(PFX "resources: %s", json_object_to_json_string(resources));

	parse_resources(resources, opts);
	parse_global(global, opts);
	parse_tasks(tasks, opts);
	
}

void
parse_config_stdin(rtapp_options_t *opts)
{
	/* read from stdin until EOF, write to temp file and parse
	 * as a "normal" config file */
	size_t in_length;
	char buf[JSON_FILE_BUF_SIZE];
	struct json_object *js;
	log_info(PFX "Reading JSON config from stdin...");

	in_length = fread(buf, sizeof(char), JSON_FILE_BUF_SIZE, stdin);
	buf[in_length] = '\0';
	js = json_tokener_parse(buf);
	get_opts_from_json_object(js, opts);
	return;
}

void
parse_config(const char *filename, rtapp_options_t *opts)
{
	int done;
	char *fn = strdup(filename);
	struct json_object *js;
	log_info(PFX "Reading JSON config from %s", fn);
	js = json_object_from_file(fn);
	get_opts_from_json_object(js, opts);
	return;
}
