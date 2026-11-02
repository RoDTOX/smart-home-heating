#ifndef CONFIG_TRANSFER_H
#define CONFIG_TRANSFER_H

#include <Arduino.h>

// Configuration export / import (FR-65.1 ... FR-65.8, docs/api_reference.md "Configuration export / import").
// The export is the stored settings of the user and the installer in one JSON file (format 1). The import checks
// each value like the same setting set alone, applies what passes and reports applied / kept / skipped.
#define CONFIG_IMPORT_MAX_BYTES 16384  // FR-65.7

String configExportJson();

// Returns the HTTP status (200, or 400 for a body that is not a JSON object with "format": 1) and the answer body.
// Nothing is changed unless the status is 200.
int configImport(const String& body, String& answer);

#endif // CONFIG_TRANSFER_H
