// Drift Parts: the Drift Angle Kit's "Steering lock" row on the game's Tuning screen (Alignment).
//
// Two pieces, both the way Enhanced Tuning (Pantsedit, GPL-3.0) adds its Tuning tab, adapted:
//
//   A native property. Every slider on the Tuning screen binds to a property the UI looks up by name
//   on the page's data type. A copy of the game's float-property template, with our name, getter and
//   setter, is linked into the property lists of the two types the screen uses (the slider rows'
//   "Tuning"/"Benchmark" type, set up by game+0xE98CD0, and the tab bar's "Categories" one,
//   game+0xE97AE0). Nothing of the game's is changed; a name is added.
//
//   The row. The game loads each screen's XAML through one call (game+0x16EBB37), which Extra Options
//   points at a small jump of its own (game+0x13FE354: mov rax, imm64; jmp rax) so it can edit screens.
//   We chain onto that jump: the file is loaded as usual (by the game, Extra Options and anything else
//   chained before us), then, for the Tuning screen only, a header and one slider row go in after the
//   Alignment panel's last row (Front Caster), plus a number converter for its value. Anything unexpected (the
//   jump missing, an anchor not found exactly once, the row already there) leaves the file as loaded.
#include <stdio.h>
#include <string.h>

#include "tune_row.h"

enum {
    FLOAT_PROPERTY = 0xED6E90,     // returns the game's float property template
    REGISTER_PROPERTY = 0x155EE30, // (registry, link)
    OBJECT_INIT = 0x143EB80,
    OBJECT_CLEANUP = 0x1442050,
    DELETE_OBJECT = 0x257730,
    BOX_FLOAT = 0x3F5990,          // (out, &value) -> boxed float
    BOOL_PROPERTY = 0xEA7810,      // returns a bool property ("IsReadOnly", as the Tuning rows' lock icons use)
    BOX_BOOL = 0x4702F0,           // (out, &value) -> boxed bool
    PROPERTY_CHANGED = 0xCF9C60,   // (page, property): refresh its bindings
    RESOURCE_CALL = 0x16EBB37,     // the loader's call, into Extra Options' jump
    RESOURCE_THUNK = 0x13FE354,
    NATIVE_ALLOCATE = 0x3229C0,
    NATIVE_FREE = 0x31C7B0,
    MAX_XAML = 1024 * 1024,
};

// Each type's set-up registers a known property with `lea rcx,[rip+registry]` then
// `call register_property`; both are checked before ours goes in.
static const struct {
    uint32_t init, registry, lea, call;
} page_types[] = {
    {0xE98CD0, 0x3541A50, 0xE98DBC, 0xE98DC3},  // slider rows (Tuning/Benchmark)
    {0xE97AE0, 0x35417E0, 0xE97BDA, 0xE97BE1},  // tab bar (Categories)
};

typedef struct {
    void **vtable;
    unsigned char header[0x20];
    void *getter, *setter, *unused;
    const char *name;
    void *type;
    uint64_t flags;
} NativeProperty;
_Static_assert(sizeof(NativeProperty) == 0x58, "native property layout");

typedef struct Link {
    NativeProperty *property;
    struct Link *next;
} Link;

typedef struct {
    void **vtable;
    unsigned char header[0x20];
    char *bytes;
    int length;
    int capacity;
} NativeResource;

typedef void **(*BoxFloatFn)(void **, const float *);
typedef void **(*GetResourceFn)(void *, void **);
typedef void *(*AllocateFn)(size_t);
typedef void (*ObjectFn)(void *);

static unsigned char *game;
static TuneRowSpec spec;
static NativeProperty property, fitted_property;  // the lock, and whether the kit is fitted (the rows' visibility)
static Link links[2], fitted_links[2];
static GetResourceFn chained;
static void *resource_vtable[7];
static char row_text[4096];

#define PROPERTY_NAME "DriftParts_SteeringLock"
#define FITTED_NAME "DriftParts_AngleKitFitted"
static const char screen_class[] = "x:Class=\"ForzaUI.Scenes.Anthem.A_TuningOptions\"";
static const char converter_anchor[] = "<Storyboard x:Key=\"A_Storyboard_VisualState_FWD\">";
static const char converter_text[] = "<local:NumUnitConverter x:Key=\"DriftParts_Degrees\" FractionDigits=\"0\" ShowUnit=\"False\"/>\r\n\r\n            ";
static const char last_row_anchor[] = "Tuning.CasterFront.IsReadOnly";  // the Alignment panel's last row
static const char row_end[] = "</ItemsControl>";

// ---- the property ------------------------------------------------------------------------------------
static void **get_lock(void **output, NativeProperty *self, void *page) {
    (void)self, (void)page;
    float value = spec.get();
    *output = NULL;
    return ((BoxFloatFn)(game + BOX_FLOAT))(output, &value);
}

static void **get_fitted(void **output, NativeProperty *self, void *page) {
    (void)self, (void)page;
    unsigned char value = spec.fitted() ? 1 : 0;
    *output = NULL;
    return ((void **(*)(void **, const unsigned char *))(game + BOX_BOOL))(output, &value);
}

static void set_lock(NativeProperty *self, void *page, void *boxed) {
    float value = -1;
    if (boxed) memcpy(&value, (unsigned char *)boxed + 0x28, sizeof value);  // a boxed float's payload
    if (!(value == value) || value < spec.min || value > spec.max) return;
    value = (float)(int)(value + 0.5f);  // whole degrees
    if (spec.set(value)) ((void (*)(void *, void *))(game + PROPERTY_CHANGED))(page, self);
}

static int add_property(size_t image_size) {
    for (unsigned i = 0; i < 2; ++i) {
        int32_t disp, rel;
        if (image_size <= page_types[i].registry) return 0;
        memcpy(&disp, game + page_types[i].lea + 3, 4);
        memcpy(&rel, game + page_types[i].call + 1, 4);
        if (memcmp(game + page_types[i].lea, "\x48\x8d\x0d", 3) || game + page_types[i].lea + 7 + disp != game + page_types[i].registry ||
            game[page_types[i].call] != 0xE8 || game + page_types[i].call + 5 + rel != game + REGISTER_PROPERTY)
            return 0;
    }
    for (unsigned i = 0; i < 2; ++i) ((void (*)(void))(game + page_types[i].init))();  // guarded by the game's own flags
    // The two template functions and the bool boxer start as written against.
    static const unsigned char template_start[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x30}, box_start[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8B, 0xD9};
    if (memcmp(game + BOOL_PROPERTY, template_start, sizeof template_start) || memcmp(game + FLOAT_PROPERTY, template_start, sizeof template_start) ||
        memcmp(game + BOX_BOOL, box_start, sizeof box_start))
        return 0;
    NativeProperty *template_property = ((NativeProperty * (*)(void))(game + FLOAT_PROPERTY))();
    NativeProperty *bool_template = ((NativeProperty * (*)(void))(game + BOOL_PROPERTY))();
    if (!template_property || !template_property->vtable || !bool_template || !bool_template->vtable) return 0;
    ((ObjectFn)(game + OBJECT_INIT))(&fitted_property);
    fitted_property.vtable = bool_template->vtable;
    memcpy((unsigned char *)&fitted_property + 0x28, (unsigned char *)bool_template + 0x28, 0x30);
    fitted_property.name = FITTED_NAME;
    fitted_property.getter = (void *)get_fitted;
    fitted_property.setter = NULL;  // read-only
    ((ObjectFn)(game + OBJECT_INIT))(&property);
    property.vtable = template_property->vtable;
    memcpy((unsigned char *)&property + 0x28, (unsigned char *)template_property + 0x28, 0x30);
    property.name = PROPERTY_NAME;
    property.getter = (void *)get_lock;
    property.setter = (void *)set_lock;
    for (unsigned i = 0; i < 2; ++i) {
        links[i].property = &property;
        ((void (*)(void *, Link *))(game + REGISTER_PROPERTY))(game + page_types[i].registry, &links[i]);
        fitted_links[i].property = &fitted_property;
        ((void (*)(void *, Link *))(game + REGISTER_PROPERTY))(game + page_types[i].registry, &fitted_links[i]);
    }
    return 1;
}

// ---- the row -----------------------------------------------------------------------------------------
static const char *find(const char *text, size_t length, const char *needle) {
    size_t n = strlen(needle);
    for (size_t i = 0; n <= length && i + n <= length; ++i)
        if (text[i] == needle[0] && !memcmp(text + i, needle, n)) return text + i;
    return NULL;
}

static const char *find_unique(const char *text, size_t length, const char *needle) {
    const char *first = find(text, length, needle);
    if (!first) return NULL;
    return find(first + 1, length - (size_t)(first + 1 - text), needle) ? NULL : first;
}

// The new text, or 0 to keep the original. `out` holds `capacity` bytes.
static size_t edit_screen(const char *text, size_t length, char *out, size_t capacity) {
    if (!find(text, length, screen_class) || find(text, length, PROPERTY_NAME)) return 0;
    const char *converter_at = find_unique(text, length, converter_anchor);
    const char *rear = find_unique(text, length, last_row_anchor);
    const char *row_at = rear ? find(rear, length - (size_t)(rear - text), row_end) : NULL;
    if (!converter_at || !row_at || row_at < converter_at) {
        spec.log("DRIFT PARTS: the Tuning screen isn't as expected; no Steering lock row.\r\n");
        return 0;
    }
    row_at += strlen(row_end);
    size_t a = (size_t)(converter_at - text), b = (size_t)(row_at - text), c = strlen(converter_text), r = strlen(row_text);
    if (length + c + r + 1 > capacity) return 0;
    memcpy(out, text, a);
    memcpy(out + a, converter_text, c);
    memcpy(out + a + c, text + a, b - a);
    memcpy(out + b + c, row_text, r);
    memcpy(out + b + c + r, text + b, length - b);
    out[length + c + r] = 0;
    return length + c + r;
}

static void release_object(void *object) {
    if (object && InterlockedDecrement((volatile LONG *)((unsigned char *)object + 0x10)) == 0) ((ObjectFn)(game + DELETE_OBJECT))(object);
}

static void *delete_resource(NativeResource *resource, unsigned flags) {
    ((ObjectFn)(game + OBJECT_CLEANUP))(resource);
    if (flags & 1) ((ObjectFn)(game + NATIVE_FREE))(resource);
    return resource;
}

static void **get_resource(void *reader, void **output) {
    chained(reader, output);
    NativeResource *original = *output;
    if (!original || !original->bytes || original->length <= 0 || original->length > MAX_XAML / 2 ||
        !find(original->bytes, (size_t)original->length, screen_class))
        return output;
    size_t capacity = (size_t)original->length + sizeof row_text + 1024;
    NativeResource *replacement = ((AllocateFn)(game + NATIVE_ALLOCATE))(sizeof(NativeResource) + capacity);
    if (!replacement) return output;
    char *bytes = (char *)(replacement + 1);
    size_t length = edit_screen(original->bytes, (size_t)original->length, bytes, capacity);
    if (!length) {
        ((ObjectFn)(game + NATIVE_FREE))(replacement);
        return output;
    }
    // The same shape as the loader's own resource; the parser owns it by reference count.
    ((ObjectFn)(game + OBJECT_INIT))(replacement);
    memcpy(resource_vtable, original->vtable - 1, sizeof resource_vtable);
    resource_vtable[1] = (void *)delete_resource;
    replacement->vtable = resource_vtable + 1;
    replacement->bytes = bytes;
    replacement->length = (int)length;
    replacement->capacity = (int)capacity;
    *output = replacement;
    release_object(original);
    spec.log("DRIFT PARTS: Steering lock row added to the Tuning screen.\r\n");
    return output;
}

static int chain_loader(size_t image_size) {
    if (image_size <= RESOURCE_CALL + 5) return 0;
    unsigned char *call = game + RESOURCE_CALL, *thunk = game + RESOURCE_THUNK;
    int32_t rel;
    memcpy(&rel, call + 1, 4);
    if (call[0] != 0xE8 || call + 5 + rel != thunk || thunk[0] != 0x48 || thunk[1] != 0xB8 || thunk[10] != 0xFF || thunk[11] != 0xE0) return 0;
    uint64_t target;
    memcpy(&target, thunk + 2, 8);
    if (!target) return 0;
    chained = (GetResourceFn)(uintptr_t)target;
    // The 8-byte target sits inside one cache line, so one store swaps it atomically.
    DWORD old;
    if (!VirtualProtect(thunk, 12, PAGE_EXECUTE_READWRITE, &old)) return 0;
    *(volatile uint64_t *)(thunk + 2) = (uint64_t)(uintptr_t)get_resource;
    VirtualProtect(thunk, 12, old, &old);
    FlushInstructionCache(GetCurrentProcess(), thunk, 12);
    return 1;
}

static void escape_xml(char *out, size_t size, const char *text) {
    size_t n = 0;
    for (; *text && n + 7 < size; ++text) {
        const char *entity = *text == '&' ? "&amp;" : *text == '<' ? "&lt;" : *text == '>' ? "&gt;" : *text == '"' ? "&quot;" : NULL;
        if (entity) {
            memcpy(out + n, entity, strlen(entity));
            n += strlen(entity);
        } else {
            out[n++] = *text;
        }
    }
    out[n] = 0;
}

int tune_row_install(unsigned char *image, size_t image_size, const TuneRowSpec *row) {
    if (!image || !row || !row->get || !row->set || !row->fitted || !row->log) return 0;
    game = image;
    spec = *row;
    char title[64], label[64], help[768];
    escape_xml(title, sizeof title, spec.title);
    escape_xml(label, sizeof label, spec.label);
    escape_xml(help, sizeof help, spec.help);
    // The game's own row shapes (Alignment's headers and rows), with Enhanced Tuning's slider pair:
    // the value, and the default as the grey marker. Both show only while the kit is fitted (the game's own
    // bool-to-visibility converter, as its rows' lock icons use).
#define VISIBLE_WITH_KIT " Visibility=\"{Binding " FITTED_NAME ", Converter={StaticResource A_FZBooleanToVisibilityConverter}}\""
    int n = snprintf(row_text, sizeof row_text,
        "\r\n                                <ItemsControl Style=\"{StaticResource A_Style_ItemsControl_1x3_TuningSpaced_Header}\"" VISIBLE_WITH_KIT ">\r\n"
        "                                    <local:FZTextBlock Style=\"{StaticResource A_Style_FZTextBlock_Tune_ItemTitle}\" Text=\"%s\" HorizontalAlignment=\"Left\" Grid.Column=\"0\"/>\r\n"
        "                                    <local:FZTextBlock Style=\"{StaticResource A_Style_FZTextBlock_Tune_ItemTitle}\" Text=\"{local:Loc Tuning.IDS_Low}\" HorizontalAlignment=\"Left\" Grid.Column=\"1\"/>\r\n"
        "                                    <local:FZTextBlock Style=\"{StaticResource A_Style_FZTextBlock_Tune_ItemTitle}\" Text=\"{local:Loc Tuning.IDS_High}\" HorizontalAlignment=\"Right\" Grid.Column=\"1\"/>\r\n"
        "                                </ItemsControl>\r\n"
        "                                <ItemsControl Style=\"{StaticResource A_Style_ItemsControl_1x3_TuningSpaced}\" Tag=\"%s\"" VISIBLE_WITH_KIT ">\r\n"
        "                                    <local:FZTextBlock Style=\"{StaticResource A_Style_FZTextBlock_Tune_ItemLabel}\" Text=\"%s\" HorizontalAlignment=\"Left\" Grid.Column=\"0\"/>\r\n"
        "                                    <Grid Grid.Column=\"1\">\r\n"
        "                                        <Slider Value=\"{Binding " PROPERTY_NAME ", Mode=TwoWay}\" Minimum=\"%g\" Maximum=\"%g\" SmallChange=\"1\" LargeChange=\"5\" Style=\"{StaticResource A_Style_Slider_Tuning_Adjust}\"/>\r\n"
        "                                        <Slider Value=\"%g\" Minimum=\"%g\" Maximum=\"%g\" Style=\"{StaticResource A_Style_Slider_Tuning_Adjust_History}\"/>\r\n"
        "                                    </Grid>\r\n"
        "                                    <local:FZTextBlock Style=\"{StaticResource A_Style_FZTextBlock_Tune_ItemLabel}\" Text=\"{Binding " PROPERTY_NAME ", Converter={StaticResource DriftParts_Degrees}}\" HorizontalAlignment=\"Right\" Grid.Column=\"2\"/>\r\n"
        "                                </ItemsControl>",
        title, help, label, spec.min, spec.max, spec.marker, spec.min, spec.max);
    if (n <= 0 || n >= (int)sizeof row_text) return 0;
    if (!add_property(image_size)) {
        spec.log("DRIFT PARTS: the Tuning screen's types aren't as expected; no Steering lock row.\r\n");
        return 0;
    }
    if (!chain_loader(image_size)) {
        spec.log("DRIFT PARTS: Extra Options' UI hook isn't there; no Steering lock row.\r\n");
        return 0;
    }
    spec.log("DRIFT PARTS: Steering lock row ready for the Tuning screen.\r\n");
    return 1;
}
