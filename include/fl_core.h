#ifndef FL_CORE_H
#define FL_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#define FL_PATH 1024
#define FL_NAME 512
#define FL_ID 160
#define FL_MAX_LIST 5000
#define FL_VERSION "0.3.1"

typedef enum {
    FL_OK = 0, FL_ERR_CONFIG, FL_ERR_AUTH_REQUIRED, FL_ERR_AUTH_EXPIRED,
    FL_ERR_OFFLINE, FL_ERR_PERMISSION, FL_ERR_FOLDER, FL_ERR_RATE_LIMIT,
    FL_ERR_TIMEOUT, FL_ERR_FORMAT, FL_ERR_UNSUPPORTED, FL_ERR_PATH,
    FL_ERR_EXISTS, FL_ERR_SPACE, FL_ERR_IO, FL_ERR_CANCELLED,
    FL_ERR_INTEGRITY, FL_ERR_BUSY, FL_ERR_LIMIT, FL_ERR_AUTH_ACCESS
} FlCode;

typedef struct { FlCode code; char message[256]; } FlError;
typedef struct {
    char id[FL_ID], name[FL_NAME], mime[96], md5[33], modified[64];
    int64_t size;
    bool is_dir, favorite;
} FlItem;
typedef struct {
    FlItem *items;
    size_t count;
    bool from_cache, stale;
    time_t fetched;
} FlListing;
typedef struct {
    char id[16], label[48], aliases[192], extensions[256];
} FlSystem;
typedef struct {
    FlSystem *systems;
    size_t count;
} FlSystems;
typedef struct {
    char sd_root[FL_PATH], app_dir[FL_PATH], data_dir[FL_PATH];
    char rclone_path[FL_PATH], credential_path[FL_PATH], settings_path[FL_PATH];
    char root_id[FL_ID], remote[64];
    unsigned cache_ttl, timeout_seconds;
} FlConfig;

void fl_error(FlError *error, FlCode code, const char *message);
const char *fl_code_name(FlCode code);
void fl_listing_free(FlListing *listing);
bool fl_id_valid(const char *id);
int fl_mkdirs(const char *path, FlError *error);
int fl_read_file(const char *path, size_t limit, char **out, FlError *error);
int fl_write_atomic(const char *path, const void *data, size_t length, FlError *error);
int fl_systems_load(const char *path, FlSystems *systems, FlError *error);
void fl_systems_free(FlSystems *systems);
const FlSystem *fl_system_find(const FlSystems *systems, const char *name);
bool fl_extension_allowed(const FlSystem *system, const char *filename);
int fl_sanitize_filename(const char *name, char out[FL_NAME], FlError *error);
int fl_rom_target(const FlConfig *config, const FlSystem *system, const char *filename,
                  bool create, char out[FL_PATH], FlError *error);
int fl_space_check(const char *directory, int64_t bytes, uint64_t *available, FlError *error);
int fl_listing_parse(const char *json, FlListing *listing, FlError *error);
void fl_listing_directories(FlListing *listing);
bool fl_listing_collision(const FlListing *listing, size_t selected);
int fl_favorites_folder(const FlListing *listing, char id[FL_ID], FlError *error);
int fl_library_merge(const FlListing *base, const FlListing *favorites, FlListing *out, FlError *error);
int fl_library_view(const FlListing *all, bool favorites_only, FlListing *out, FlError *error);
int fl_md5_file(const char *path, char digest[33], FlError *error);
typedef enum { FL_LOCAL_NONE, FL_LOCAL_PRESENT, FL_LOCAL_VERIFIED, FL_LOCAL_CHANGED } FlLocal;
FlLocal fl_local_check(const FlConfig *config, const FlSystem *system, const FlItem *item,
                       bool hash_existing, FlError *error);
int fl_receipt_write(const FlConfig *config, const FlItem *item, const char *target, FlError *error);

#endif
