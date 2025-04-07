/**
 * @file app.h (template)
 * @author Denys Stovbun (denis.stovbun@lanars.com)
 * @brief Configurable constants
 * @version 0.1
 * @date 2025-04-07
 *
 *
 *
 */
#ifndef App_H
#define App_H

// Version
#define VERSION_MAJOR       @version_major@
#define VERSION_MINOR       @version_minor@
#define VERSION_REV         @version_rev@
#define VERSION_STR         "@version_major@.@version_minor@.@version_rev@"

// File locations
#define LOG_FILENAME        "@log_file@"

void selfLog (const char* fmt, ...);

#endif // App_H