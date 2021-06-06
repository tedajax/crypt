#include "curves.h"
#include "debug_gui.h"
#include "parson.h"
#include "sprite_renderer.h"
#include "stb_ds.h"
#include "tx_math.h"
#include "tx_types.h"
#include <ccimgui.h>

struct curve {
    str_id id;
    vec2* points;
};

vec2 curve_eval_at(const struct curve* curve, int32_t idx, float t);
vec2 curve_eval(const struct curve* curve, float t);
void curve_set_points(struct curve* curve, vec2* points, size_t n);

JSON_Value* curve_serialize_json(const struct curve* curve);
void curve_deserialize_json(JSON_Value* json, struct curve* out);
JSON_Value* curve_db_serialize_json(struct curve* db);
struct curve* curve_db_deserialize_json(JSON_Value* json);
struct curve* curve_db_load(const char* filename);
void curve_db_save(struct curve* db, const char* filename);

void curve_set_points(struct curve* curve, vec2* points, size_t n)
{
    arrsetlen(curve->points, 0);
    arrsetcap(curve->points, n);
    for (size_t i = 0; i < n; ++i) {
        arrput(curve->points, points[i]);
    }
}

vec2 curve_eval_at(const struct curve* curve, int32_t idx, float t)
{
    int32_t len = (int32_t)arrlen(curve->points);
    if (idx < 0 || idx >= len) {
        return (vec2){0};
    }

    int range_min = -1;
    int range_max = 2;

    if (idx + range_min < 0) {
        range_min = 0;
    }

    int32_t i[4] = {
        max(idx - 1, 0),
        max(idx, 0),
        min(idx + 1, len - 1),
        min(idx + 2, len - 1),
    };

    vec2 p[4] = {
        curve->points[i[0]],
        curve->points[i[1]],
        curve->points[i[2]],
        curve->points[i[3]],
    };

    float s[4] = {
        ((-t + 2) * t - 1) * t * 0.5f,
        (((3 * t - 5) * t) * t + 2) * 0.5f,
        ((-3 * t + 4) * t + 1) * t * 0.5f,
        ((t - 1) * t * t) * 0.5f,
    };

    vec2 ret = {0};

    for (int i = 0; i < 4; ++i) {
        ret.x += p[i].x * s[i];
        ret.y += p[i].y * s[i];
    }

    return ret;
}

vec2 curve_eval(const struct curve* curve, float t)
{
    int32_t len = (int32_t)arrlen(curve->points);
    int32_t i = (int32_t)t;
    return curve_eval_at(curve, i, t - i);
}

struct curve* curve_db = NULL;
struct curve* active_curve = NULL;

void TestCurve(ecs_iter_t* it)
{
    static float curve_time = 0.0f;

    if (!active_curve) {
        return;
    }

    vec2* points = active_curve->points;
    size_t len = arrlen(points);

    curve_time += it->delta_time;
    while (curve_time > (float)len) {
        curve_time -= (float)len;
    }

    for (size_t i = 0; i < len - 1; ++i) {
        draw_line_col(points[i], points[i + 1], k_color_green);
    }

    static int32_t segments = 8;
    const float step = 1.0f / segments;

    for (size_t i = 0; i < len; ++i) {
        float t = 0.0f;
        for (t = 0; t < 1; t += step) {
            vec2 p0 = curve_eval_at(active_curve, (int32_t)i, t);
            vec2 p1 = curve_eval_at(active_curve, (int32_t)i, t + step);
            draw_line_col(p0, p1, k_color_rose);
        }
    }

    vec2 timepos = curve_eval(active_curve, curve_time);
    vec2 size = (vec2){0.5f, 0.5f};
    vec2 p0 = vec2_sub(timepos, size);
    vec2 p1 = vec2_add(timepos, size);
    draw_rect_col(p0, p1, k_color_azure);
}

typedef struct curve_debug_gui_context {
    void* poopy;
} curve_debug_gui_context;

char name_buf[256] = {0};

void curve_debug_gui(ecs_world_t* world, void* ctx)
{
    curve_debug_gui_context* context = (curve_debug_gui_context*)ctx;

    if (igButton("Save", (ImVec2){80, 30})) {
        curve_db_save(curve_db, "assets/curve_db.json");
    }
    igSameLine(0, -1);
    if (igButton("Load", (ImVec2){80, 30})) {
        active_curve = NULL;
        if (curve_db) {
            arrfree(curve_db);
        }
        curve_db = curve_db_load("assets/curve_db.json");
    }

    igSeparator();

    igBeginColumns("curve_editor_cols", 2, ImGuiColumnsFlags_None);
    igSetColumnWidth(0, 160);
    {
        ImDrawList* draw_list = igGetWindowDrawList();
        size_t len = arrlen(curve_db);
        for (size_t i = 0; i < len; ++i) {
            const char* name = str_id_cstr(curve_db[i].id);
            uint32_t flags = ImGuiButtonFlags_None;

            if (&curve_db[i] == active_curve) {
                flags |= ImGuiButtonFlags_Disabled;

                ImVec2 p0;
                igGetCursorScreenPos(&p0);
                ImVec2 p1 = p0;
                p1.x += 140;
                p1.y += 30;

                ImDrawList_AddRectFilled(
                    draw_list, p0, p1, 0x7F00FFFF, 0.f, ImDrawCornerFlags_None);
            }

            if (igButtonEx(name, (ImVec2){140, 30}, flags)) {
                active_curve = &curve_db[i];
                const char* name = str_id_cstr(active_curve->id);
                strncpy(name_buf, name, 256);
            }
        }
    }
    igNextColumn();
    {
        if (active_curve) {
            const char* name = str_id_cstr(active_curve->id);
            igText("Name:");
            igSameLine(0, -1);
            igInputTextEx(
                "", "", name_buf, 256, (ImVec2){140, 30}, ImGuiInputTextFlags_None, NULL, NULL);
            igSameLine(0, -1);
            if (igButton("+", (ImVec2){30, 30})) {
                active_curve->id = str_id_store(name_buf);
            }

            igSeparator();

            igText("Points:");

            size_t len = arrlen(active_curve->points);
            for (size_t i = 0; i < len; ++i) {
                vec2* pt = &active_curve->points[i];
                char buf[6];
                snprintf(buf, 6, "%02llu", i);
                igText(buf);
                igSameLine(0, -1);
                igSeparatorEx(ImGuiSeparatorFlags_Vertical);
                igSameLine(0, -1);
                igPushIDInt((int)i + 0xf00d);
                igInputFloat2("", &pt->x, "%0.2f", ImGuiInputTextFlags_None);
                igPopID();
            }

            igPushIDStr("Add point");
            if (igButton("+", (ImVec2){30, 30})) {
                arrput(active_curve->points, ((vec2){0}));
            }
            igPopID();
        }
    }
    igEndColumns();
}

void GameCurvesImport(ecs_world_t* world)
{
    ECS_MODULE(world, GameCurves);

    ECS_IMPORT(world, DebugGui);

    curve_db = curve_db_load("assets/curve_db.json");

    DEBUG_PANEL(
        world,
        CurveEditor,
        ImGuiWindowFlags_None,
        "shift+c",
        curve_debug_gui,
        curve_debug_gui_context,
        {0});

    ECS_SYSTEM(world, TestCurve, EcsOnUpdate, : TestCurve);
}

#include "parson.h"

JSON_Value* curve_serialize_json(const struct curve* curve)
{
    JSON_Value* root = json_value_init_object();
    JSON_Object* obj = json_value_get_object(root);

    const char* name = str_id_cstr(curve->id);
    if (name) {
        json_object_set_string(obj, "name", name);
    } else {
        json_object_set_string(obj, "name", "default");
    }

    JSON_Array* points_array = json_value_get_array(json_value_init_array());
    size_t len = arrlen(curve->points);
    for (size_t i = 0; i < len; ++i) {
        JSON_Object* point_object = json_value_get_object(json_value_init_object());
        json_object_set_number(point_object, "x", curve->points[i].x);
        json_object_set_number(point_object, "y", curve->points[i].y);
        json_array_append_value(points_array, json_object_get_wrapping_value(point_object));
    }

    json_object_set_value(obj, "points", json_array_get_wrapping_value(points_array));

    return root;
}

const char* k_curve_json_scheme = "{"
                                  "\"name\": \"\","
                                  "\"points\": ["
                                  "{"
                                  "\"x\": 0, \"y\": 0"
                                  "}"
                                  "]"
                                  "}";

void curve_deserialize_json(JSON_Value* json, struct curve* out)
{
    JSON_Value* schema = json_parse_string(k_curve_json_scheme);
    if (json != NULL && json_validate(schema, json) == JSONSuccess) {
        JSON_Object* curve_obj = json_value_get_object(json);

        const char* name_str = json_object_get_string(curve_obj, "name");

        if (name_str) {
            str_id id = str_id_store(name_str);
            out->id = id;
        } else {
            out->id = str_id_empty;
        }

        JSON_Array* points_array = json_object_get_array(curve_obj, "points");
        if (points_array == NULL) {
            goto cleanup;
        }

        vec2* points = NULL;
        size_t len = json_array_get_count(points_array);
        for (size_t i = 0; i < len; ++i) {
            JSON_Object* point_object = json_array_get_object(points_array, i);
            double x = json_object_get_number(point_object, "x");
            double y = json_object_get_number(point_object, "y");
            arrput(points, ((vec2){(float)x, (float)y}));
        }
        curve_set_points(out, points, len);
        arrfree(points);
    }

cleanup:
    json_value_free(schema);
}

JSON_Value* curve_db_serialize_json(struct curve* db)
{
    if (!db) {
        return NULL;
    }

    size_t len = arrlen(db);
    if (len == 0) {
        return NULL;
    }

    JSON_Value* root = json_value_init_object();
    JSON_Object* db_obj = json_value_get_object(root);

    JSON_Value* curve_node = json_value_init_array();
    JSON_Array* curve_arr = json_value_get_array(curve_node);
    for (size_t i = 0; i < len; ++i) {
        JSON_Value* curve_node = curve_serialize_json(&db[i]);
        json_array_append_value(curve_arr, curve_node);
    }
    json_object_set_value(db_obj, "curves", json_array_get_wrapping_value(curve_arr));

    return root;
}

struct curve* curve_db_deserialize_json(JSON_Value* json)
{
    struct curve* db = NULL;

    if (!json) {
        return NULL;
    }

    JSON_Object* db_obj = json_value_get_object(json);
    JSON_Array* curve_arr = json_object_get_array(db_obj, "curves");
    size_t len = json_array_get_count(curve_arr);

    if (len == 0) {
        return NULL;
    }

    arrsetlen(db, len);
    memset(db, 0, sizeof(struct curve) * len);
    for (size_t i = 0; i < len; ++i) {
        JSON_Value* curve_node =
            json_object_get_wrapping_value(json_array_get_object(curve_arr, i));
        curve_deserialize_json(curve_node, &db[i]);
    }

    return db;
}

struct curve* curve_db_load(const char* filename)
{
    JSON_Value* json = json_parse_file(filename);
    return curve_db_deserialize_json(json);
}

void curve_db_save(struct curve* db, const char* filename)
{
    JSON_Value* root = curve_db_serialize_json(db);
    json_serialize_to_file_pretty(root, filename);
    json_value_free(root);
}
