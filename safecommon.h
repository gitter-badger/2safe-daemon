#ifndef SAFECOMMON_H
#define SAFECOMMON_H

#define APP_NAME "2Safe"
#define ORG_NAME "ROSA"
#define DEFAULT_ROOT_NAME "2safe"
#define LOCAL_STATE_DATABASE "local.db"
#define REMOTE_STATE_DATABASE "remote.db"
#define SAFE_DIR ".2safe"
#define SOCKET_FILE "control.sock"
#define TOKEN_LIFESPAN 24 * 60 * 60 // 24 hours

#define SET_SETTINGS_TYPE "set_settings"
#define GET_SETTINGS_TYPE "get_settings"
#define REPLY_SETTINGS_TYPE "settings"
#define ACTION_TYPE "action"
#define API_CALL "api_call"
#define NOOP "noop"

#define DIR_CREATED_EVENT "dir_created"
#define DIR_MOVED_EVENT "dir_moved"
#define FILE_MOVED_EVENT "file_moved"
#define FILE_UPLOADED_EVENT "file_uploaded"

// cheating
#define TRASH_ID "227931033757"

#endif // SAFECOMMON_H
