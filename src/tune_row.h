// Drift Parts: the Drift Angle Kit's "Steering lock" row on the game's Tuning screen (see tune_row.c).
#ifndef DRIFT_PARTS_TUNE_ROW_H
#define DRIFT_PARTS_TUNE_ROW_H
#include <windows.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *title, *label, *help;  // header text, row label, help text (plain text)
    float min, max, marker;            // the slider's range in degrees, and its grey marker
    float (*get)(void);                // the driven car's lock
    int (*set)(float degrees);         // nonzero when taken
    int (*fitted)(void);               // the driven car has the kit: the rows show only then
    void (*log)(const char *line);
} TuneRowSpec;

// Adds the property and chains onto Extra Options' UI hook. 1 when both are in place.
int tune_row_install(unsigned char *image, size_t image_size, const TuneRowSpec *row);

#endif
