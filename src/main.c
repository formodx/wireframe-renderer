#include <math.h>
#include <stdlib.h>
#include "raylib.h"


static struct{
    float fov_x, fov_y;
    float speed_z;
    float near_plane, far_plane;
    Vector3 position;
} camera;
static int screen_width, screen_height;
static float angle_x, angle_y, angle_z;
static Model model;
static BoundingBox bounding_box;


static inline float radians(float degrees){
    return degrees * PI / 180.0f;
}


static Vector3 rotate_x(Vector3 vertex, float degrees){
    float distance = sqrtf(vertex.y*vertex.y + vertex.z*vertex.z);
    float angle = atan2f(vertex.z, vertex.y) + radians(degrees);

    return (Vector3) {
        vertex.x,
        cosf(angle) * distance,
        sinf(angle) * distance
    };
}


static Vector3 rotate_y(Vector3 vertex, float degrees){
    float distance = sqrtf(vertex.x*vertex.x + vertex.z*vertex.z);
    float angle = atan2f(vertex.z, vertex.x) + radians(degrees);

    return (Vector3) {
        cosf(angle) * distance,
        vertex.y,
        sinf(angle) * distance
    };
}


static Vector3 rotate_z(Vector3 vertex, float degrees){
    float distance = sqrtf(vertex.x*vertex.x + vertex.y*vertex.y);
    float angle = atan2f(vertex.y, vertex.x) + radians(degrees);

    return (Vector3) {
        cosf(angle) * distance,
        sinf(angle) * distance,
        vertex.z
    };
}


// model => world
static Vector3 to_world_coordinates(Vector3 vertex){
    vertex.x -= (bounding_box.max.x+bounding_box.min.x) * 0.5f;
    vertex.y -= (bounding_box.max.y+bounding_box.min.y) * 0.5f;
    vertex.z -= (bounding_box.max.z+bounding_box.min.z) * 0.5f;

    vertex = rotate_x(vertex, angle_x);
    vertex = rotate_y(vertex, angle_y);
    vertex = rotate_z(vertex, angle_z);

    return vertex;
}


// world => camera
static Vector3 to_camera_coordinates(Vector3 vertex){
    return (Vector3) {
        vertex.x - camera.position.x,
        vertex.y - camera.position.y,
        vertex.z - camera.position.z
    };
}


// camera => clip
static Vector3 to_clip_coordinates(Vector3 vertex){
    return (Vector3) {
        vertex.x / tanf(camera.fov_x*0.5f),
        vertex.y / tanf(camera.fov_y*0.5f),
        -vertex.z
    };
}


// clip => ndc
static inline Vector2 to_normalized_device_coordinates(Vector3 vertex){
    return (Vector2) {
        vertex.x / vertex.z,
        vertex.y / vertex.z
    };
}


// ndc => screen
static inline Vector2 to_screen_coordinates(Vector2 vertex){
    return (Vector2) {
        (vertex.x+1.0f) * 0.5f * screen_width,
        (1.0f-vertex.y) * 0.5f * screen_height
    };
}


static void draw_edge(Vector3 begin, Vector3 end){
    Vector3 world1 = to_world_coordinates(begin);
    Vector3 world2 = to_world_coordinates(end);

    Vector3 camera1 = to_camera_coordinates(world1);
    Vector3 camera2 = to_camera_coordinates(world2);

    Vector3 clip1 = to_clip_coordinates(camera1);
    Vector3 clip2 = to_clip_coordinates(camera2);

    if(
        fabsf(clip1.x)>clip1.z ||
        fabsf(clip1.y)>clip1.z ||
        clip1.z<camera.near_plane ||
        clip1.z>camera.far_plane
    ) return;
    if(
        fabsf(clip2.x)>clip2.z ||
        fabsf(clip2.y)>clip2.z ||
        clip2.z<camera.near_plane ||
        clip2.z>camera.far_plane
    ) return;

    Vector2 ndc1 = to_normalized_device_coordinates(clip1);
    Vector2 ndc2 = to_normalized_device_coordinates(clip2);

    Vector2 screen1 = to_screen_coordinates(ndc1);
    Vector2 screen2 = to_screen_coordinates(ndc2);
    DrawLineV(screen1, screen2, (Color) {120, 216, 232, 255});
}


static void draw_model(){
    for(int i=0; i<model.meshCount; ++i){
        Mesh *mesh = &model.meshes[i];
        for(int j=0; j<mesh->triangleCount; ++j){
            Vector3 *v1 = NULL, *v2 = NULL, *v3 = NULL;
            if(mesh->indices == NULL){
                v1 = (Vector3 *) &mesh->vertices[j*9 + 0];
                v2 = (Vector3 *) &mesh->vertices[j*9 + 3];
                v3 = (Vector3 *) &mesh->vertices[j*9 + 6];
            }
            else{
                v1 = (Vector3 *) &mesh->vertices[mesh->indices[j*3+0] * 3];
                v2 = (Vector3 *) &mesh->vertices[mesh->indices[j*3+1] * 3];
                v3 = (Vector3 *) &mesh->vertices[mesh->indices[j*3+2] * 3];
            }

            draw_edge(*v1, *v2);
            draw_edge(*v2, *v3);
            draw_edge(*v3, *v1);
        }
    }
}


static void draw_controls(){
    int x = 20, y = 20;
    Rectangle rectangle = {x, y, 280, 210};

    DrawRectangleRounded(rectangle, 0.12f, 8, Fade((Color) {18, 28, 43, 255}, 0.88f));
    DrawRectangleRoundedLinesEx(rectangle, 0.12f, 8, 2.0f, Fade((Color) {120, 216, 232, 255}, 0.55f));

    DrawText("Controls", x + 15, y + 12, 24, (Color) {241, 245, 249, 255});

    int lineY = y + 50;
    int gap = 26;
    int font_size = 20;
    Color text_color = {190, 206, 224, 255};
    DrawText("W / S - rotate x", x + 15, lineY + gap*0, font_size, text_color);
    DrawText("A / D - rotate y", x + 15, lineY + gap*1, font_size, text_color);
    DrawText("Q / E - rotate z", x + 15, lineY + gap*2, font_size, text_color);
    DrawText("UP - camera z -", x + 15, lineY + gap*3, font_size, text_color);
    DrawText("DOWN - camera z +", x + 15, lineY + gap*4, font_size, text_color);
}


int main(){
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(1280, 720, "renderer");
    SetTargetFPS(60);

    screen_width = GetScreenWidth();
    screen_height = GetScreenHeight();

    float aspect = screen_height * 1.0f / screen_width;

    model = LoadModel("model.glb");
    bounding_box = GetModelBoundingBox(model);

    camera.fov_x = radians(120.0f);
    camera.fov_y = atanf(tanf(camera.fov_x*0.5f)*aspect) * 2.0f;
    camera.near_plane = 0.1f;
    camera.far_plane = 100.0f;
    camera.speed_z = (bounding_box.max.z-bounding_box.min.z) * 1.6f;

    float rotation_speed = 130.0f;
    while(!WindowShouldClose()){
        float delta_time = GetFrameTime();

        if(IsKeyDown(KEY_W)) angle_x -= rotation_speed * delta_time;
        if(IsKeyDown(KEY_S)) angle_x += rotation_speed * delta_time;

        if(IsKeyDown(KEY_A)) angle_y += rotation_speed * delta_time;
        if(IsKeyDown(KEY_D)) angle_y -= rotation_speed * delta_time;

        if(IsKeyDown(KEY_Q)) angle_z += rotation_speed * delta_time;
        if(IsKeyDown(KEY_E)) angle_z -= rotation_speed * delta_time;

        if(IsKeyDown(KEY_UP)) camera.position.z -= camera.speed_z * delta_time;
        if(IsKeyDown(KEY_DOWN)) camera.position.z += camera.speed_z * delta_time;

        BeginDrawing();
        ClearBackground((Color) {8, 12, 20, 255});

        draw_model();
        draw_controls();

        EndDrawing();
    }

    UnloadModel(model);

    CloseWindow();

    return 0;
}