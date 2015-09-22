#ifdef _MSC_VER
#define _CRT_NONSTDC_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#define strtok_r strtok_s
#endif

#include "path.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define PATH_MAX_HOPS   256
#define PATH_MAX_LENGTH 4096

static path_t* construct_path       (const char* pathname, bool force_dir);
static void    convert_to_directory (path_t* path);
static void    update_pathname      (path_t* path);

struct path
{
	char*  filename;
	char** hops;
	size_t num_hops;
	char*  pathname;
};

path_t*
path_new(const char* pathname)
{
	return construct_path(pathname, false);
}

path_t*
path_new_dirname(const char* pathname)
{
	return construct_path(pathname, true);
}

path_t*
path_dup(const path_t* path)
{
	return path_new(path_cstr(path));
}

void
path_free(path_t* path)
{
	size_t i;
	
	if (path == NULL)
		return;
	
	for (i = 0; i < path->num_hops; ++i)
		free(path->hops[i]);
	free(path->hops);
	free(path->filename);
	free(path->pathname);
	free(path);
}

const char*
path_cstr(const path_t* path)
{
	// important note:
	// path_cstr() must be implemented such that path_new(path_cstr(path))
	// results in a perfect duplicate. in practice this means directory paths
	// will always be printed with a trailing separator.

	return path->pathname;
}

const char*
path_filename_cstr(const path_t* path)
{
	return path->filename;
}

const char*
path_hop_cstr(const path_t* path, size_t idx)
{
	return path->hops[idx];
}

bool
path_is_rooted(const path_t* path)
{
	const char* first_hop = path->num_hops >= 1 ? path->hops[0] : "Unforgivable!";
	return strlen(first_hop) >= 2 && first_hop[1] == ':'
		|| strcmp(first_hop, "") == 0;
}

size_t
path_num_hops(const path_t* path)
{
	return path->num_hops;
}

path_t*
path_append(path_t* path, const char* pathname, bool force_dir_path)
{
	char* parse;
	char* token;
	char  *p_filename;

	char   *p;
	size_t i;

	if (path->filename != NULL)
		goto on_error;

	// normalize path to forward slash delimiters
	parse = strdup(pathname);
	p = parse;
	do {
		if (*p == '\\') *p = '/';
	} while (*++p != '\0');

	// parse the filename out of the path, if any
	if (!(p_filename = strrchr(parse, '/')))
		p_filename = parse;
	else
		++p_filename;
	path->filename = *p_filename != '\0'
		? strdup(p_filename) : NULL;

	*p_filename = '\0';  // strip filename
	if (parse[0] == '/') {
		// pathname is absolute, replace wholesale instead of appending
		for (i = 0; i < path->num_hops; ++i)
			free(path->hops[i]);
		path->num_hops = 0;
		path->hops[path->num_hops++] = strdup("");
	}

	// strtok_r() collapses consecutive separator characters, but this is okay: POSIX
	// requires multiple slashes to be treated as a single separator anyway.
	token = strtok_r(parse, "/", &p);
	while (token != NULL) {
		path->hops[path->num_hops++] = strdup(token);
		token = strtok_r(NULL, "/", &p);
	}
	free(parse);
	
	if (force_dir_path)
		convert_to_directory(path);
	path_collapse(path, false);
	update_pathname(path);
	return path;

on_error:
	return NULL;
}

path_t*
path_cat(path_t* path, const path_t* tail)
{
	return path_append(path, path_cstr(tail), false);
}

path_t*
path_collapse(path_t* path, bool collapse_double_dots)
{
	int         depth = 0;
	const char* hop;
	bool        is_rooted;
	
	size_t i;

	is_rooted = path_is_rooted(path);
	for (i = 0; i < path_num_hops(path); ++i) {
		++depth;
		hop = path_hop_cstr(path, i);
		if (strcmp(hop, ".") == 0) {
			--depth;
			path_remove_hop(path, i--);
		}
		else if (strcmp(hop, "..") == 0) {
			depth -= 2;
			if (!collapse_double_dots)
				continue;
			path_remove_hop(path, i--);
			if (i > 0 || !is_rooted)  // don't strip the root directory
				path_remove_hop(path, i--);
		}
	}
	update_pathname(path);
	return path;
}

path_t*
path_rebase(path_t* path, const path_t* root)
{
	char** new_hops;
	size_t num_root_hops;

	size_t i;
	
	if (path_filename_cstr(root) != NULL)
		return NULL;
	if (path_is_rooted(path))
		return path;
	new_hops = malloc(PATH_MAX_HOPS * sizeof(char*));
	num_root_hops = path_num_hops(root);
	for (i = 0; i < path_num_hops(root); ++i)
		new_hops[i] = strdup(root->hops[i]);
	for (i = 0; i < path_num_hops(path); ++i)
		new_hops[i + num_root_hops] = path->hops[i];
	free(path->hops);
	path->hops = new_hops;
	path->num_hops += num_root_hops;
	
	path_collapse(path, false);
	return path;
}

path_t*
path_relativize(path_t* path, const path_t* root)
{
	path_t* root_path;

	size_t i;

	root_path = path_strip(path_dup(root));
	if (path_num_hops(path) < path_num_hops(root))
		return NULL;
	for (i = 0; i < path_num_hops(root_path); ++i)  // all hops must match
		if (strcmp(path_hop_cstr(root_path, 0), path_hop_cstr(path, 0)) != 0)
			return NULL;
	while (path_num_hops(root_path) > 0
		&& strcmp(path_hop_cstr(root_path, 0), path_hop_cstr(path, 0)) == 0)
	{
		path_remove_hop(root_path, 0);
		path_remove_hop(path, 0);
	}
	path_free(root_path);
	return path;
}

path_t*
path_remove_hop(path_t* path, size_t idx)
{
	size_t i;

	free(path->hops[idx]);
	--path->num_hops;
	for (i = idx; i < path->num_hops; ++i)
		path->hops[i] = path->hops[i + 1];
	update_pathname(path);
	return path;
}

path_t*
path_strip(path_t* path)
{
	free(path->filename);
	path->filename = NULL;
	update_pathname(path);
	return path;
}

static path_t*
construct_path(const char* pathname, bool force_dir)
{
	path_t* path;

	path = calloc(1, sizeof(path_t));
	path->hops = malloc(PATH_MAX_HOPS * sizeof(char*));
	path_append(path, pathname, force_dir);
	return path;
}

static void
convert_to_directory(path_t* path)
{
	if (path->filename == NULL)
		return;
	path->hops[path->num_hops++] = path->filename;
	path->filename = NULL;
}

static void
update_pathname(path_t* path)
{
	// notes: * the pathname generated here, when used to construct a new path object,
	//          must result in the same type of path (file, directory, etc.). see note
	//          on path_cstr() above.
	//        * the canonical path separator is the forward slash (/).
	//        * directory paths are always reported with a trailing slash.

	char  buffer[PATH_MAX_LENGTH];  // FIXME: security risk

	size_t i;

	strcpy(buffer, path->num_hops == 0 && path->filename == NULL
		? "./" : "");
	for (i = 0; i < path->num_hops; ++i) {
		if (i > 0) strcat(buffer, "/");
		strcat(buffer, path->hops[i]);
	}
	if (path->num_hops > 0) strcat(buffer, "/");
	if (path->filename != NULL)
		strcat(buffer, path->filename);
	free(path->pathname);
	path->pathname = strdup(buffer);
}