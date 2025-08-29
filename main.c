#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

// Game configuration
typedef struct {
    double gravity;
    double engine_force;
    int initial_fuel;
    int display_delta_v;
} GameConfig;

// Landing radar data
typedef struct {
    int active;
    int turns_remaining;
    double terrain_height[21];
    double safe_landing_x;
    double safe_landing_score;
} LandingRadar;

typedef struct {
    double A, B; // A: horizontal position, B: altitude
    double vel_h, vel_v;
    double prev_vel_h, prev_vel_v; // For delta V calculation
    int C; // Fuel
    int engines_on;
    double time_step;
    LandingRadar radar;
} GameState;

// Function Prototypes
void init_game(GameState* state, GameConfig* config);
void display_status(GameState* state, GameConfig* config);
char get_command(void);
void update_physics(GameState* state, GameConfig* config, char move_command);
int check_landing(GameState* state);
void save_result(GameState* state, const char* result);
void configure_game(GameConfig* config);
void activate_landing_radar(GameState* state);
void display_landing_radar(GameState* state);
void generate_terrain_data(GameState* state);
double calculate_landing_safety(GameState* state, double x_pos);
void display_visualizer(GameState* state);
void handle_game_turn(GameState* state, GameConfig* config, char command, int* game_over);

int main(int argc, char* argv[]) {
    GameConfig config = {1.6, 3.0, 50, 0}; // Default: moon gravity, 3 m/s² thrust, 50 fuel
    GameState state;
    char command;
    int game_over = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--delta-v") == 0 || strcmp(argv[i], "-d") == 0) {
            config.display_delta_v = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Moon Lander - Command Line Options:\n");
            printf("  --delta-v, -d    Display velocity changes as Delta V\n");
            printf("  --help, -h       Show this help message\n");
            return 0;
        }
    }

    srand(time(NULL));

    printf("=== MOON LANDER WITH DYNAMIC RADAR VISUALS ===\n");
    printf("Olivetti Programma 101 Style Implementation\n");
    printf("Commands: V-Start, W-Engines On, S-Engines Off, Y-Left Burn, Z-Right Burn\n");
    printf("          X-Drift (skip burn), R-Activate Radar, C-Configure, Q-Quit\n");
    printf("Display Mode: %s\n", config.display_delta_v ? "Delta V" : "m/s");
    printf("\nNOTE: Use Radar (R) to activate the visual display, which zooms in on approach.\n");
    printf("Press 'V' to begin a new game.\n");

    while (1) {
        command = get_command();

        if (game_over && toupper(command) != 'V' && toupper(command) != 'Q' && toupper(command) != 'C') {
            printf("Game over. Press 'V' to start a new game or 'Q' to quit.\n");
            continue;
        }

        switch (toupper(command)) {
            case 'V':
                init_game(&state, &config);
                game_over = 0;
                printf("\n=== NEW GAME STARTED ===\n");
                display_status(&state, &config);
                break;

            case 'C':
                configure_game(&config);
                printf("\nConfiguration updated. Press 'V' to start a new game with these settings.\n");
                break;

            case 'Q':
                printf("Thanks for playing Moon Lander!\n");
                return 0;

            case 'W':
                if (game_over) break;
                state.engines_on = 1;
                printf(">>> Main Engines ON. <<<\n");
                break;

            case 'S':
                if (game_over) break;
                state.engines_on = 0;
                printf(">>> Main Engines OFF. <<<\n");
                break;

            case 'R':
                if (game_over) break;
                if (state.C > 0) {
                    activate_landing_radar(&state);
                    state.C--; // Radar consumes fuel
                    display_status(&state, &config);
                    if (state.C <= 0) printf("\n*** WARNING: FUEL DEPLETED. ***\n");
                } else {
                    printf("No fuel remaining! Cannot activate radar.\n");
                }
                break;

            case 'Y':
            case 'Z':
            case 'X':
                if (!game_over) {
                    handle_game_turn(&state, &config, toupper(command), &game_over);
                }
                break;

            default:
                printf("Unknown command. Use: V, W, S, Y, Z, X, R, C, Q\n");
                break;
        }
    }
    return 0;
}

void handle_game_turn(GameState* state, GameConfig* config, char command, int* game_over) {
    if (state->C <= 0) {
        printf("No fuel remaining! Lander is now drifting.\n");
        state->engines_on = 0;
        command = 'X'; // Force drift if out of fuel
    }

    if ((command == 'Y' || command == 'Z') && !state->engines_on) {
        printf("Cannot burn. Main engines are OFF (use 'W' to turn on).\n");
        return; // Not a full turn, so return early
    }

    if (state->radar.active && state->radar.turns_remaining > 0) {
        printf("\n[Radar data from previous position]\n");
        display_landing_radar(state);
    }

    update_physics(state, config, command);
    if (command != 'X') state->C--;

    if (state->radar.active && state->radar.turns_remaining > 0) {
        state->radar.turns_remaining--;
        if (state->radar.turns_remaining <= 0) {
            state->radar.active = 0;
            printf(">>> Landing radar signal lost. Visuals deactivated. <<<\n");
        }
    }

    display_status(state, config);

    int landing_result = check_landing(state);
    if (landing_result != 0) {
        if (landing_result == 1) {
            printf("\n*** THE EAGLE HAS LANDED! SUCCESSFUL LANDING! ***\n");
            save_result(state, "SUCCESS");
        } else {
            printf("\n*** CRASHED! High impact speed. ***\n");
            save_result(state, "CRASHED");
        }
        *game_over = 1;
    } else if (state->C <= 0) {
        printf("\n*** WARNING: FUEL DEPLETED. ***\n");
    }
}

void init_game(GameState* state, GameConfig* config) {
    state->A = (double)(rand() % 200 - 100);
    state->B = (double)(rand() % 500 + 100);
    state->vel_h = (double)(rand() % 20 - 10) / 2.0;
    state->vel_v = (double)(rand() % 20 - 15);
    state->prev_vel_h = state->vel_h;
    state->prev_vel_v = state->vel_v;
    state->C = config->initial_fuel;
    state->engines_on = 0;
    state->time_step = 1.0;
    state->radar.active = 0;
    state->radar.turns_remaining = 0;
    generate_terrain_data(state);
}

void generate_terrain_data(GameState* state) {
    for (int i = 0; i < 21; i++) {
        double x_pos = -100 + (i * 10);
        double base_height = 0.0;
        double variation = sin(x_pos * 0.1) * 5 + cos(x_pos * 0.05) * 3;
        double hazard = (rand() % 100 < 15) ? (rand() % 20 - 10) * 0.5 : 0.0;
        state->radar.terrain_height[i] = base_height + variation + hazard;
    }

    double best_safety = -1, best_x = 0;
    for (int i = 1; i < 20; i++) {
        double x_pos = -100 + (i * 10);
        double safety = calculate_landing_safety(state, x_pos);
        if (safety > best_safety) {
            best_safety = safety;
            best_x = x_pos;
        }
    }
    state->radar.safe_landing_x = best_x;
    state->radar.safe_landing_score = best_safety;
}

double calculate_landing_safety(GameState* state, double x_pos) {
    int index = (int)((x_pos + 100) / 10);
    if (index < 0 || index >= 21) return 0;
    double safety = 100.0;
    safety -= fabs(state->radar.terrain_height[index]) * 10;
    if (index > 0 && index < 20) {
        double slope_left = fabs(state->radar.terrain_height[index] - state->radar.terrain_height[index - 1]);
        double slope_right = fabs(state->radar.terrain_height[index + 1] - state->radar.terrain_height[index]);
        safety -= (slope_left + slope_right) * 5;
    }
    return fmax(0, safety);
}

void activate_landing_radar(GameState* state) {
    printf("\n=== ACTIVATING LANDING RADAR (1 fuel consumed) ===\n");
    state->radar.active = 1;
    state->radar.turns_remaining = 3;
    display_landing_radar(state);
}

void display_landing_radar(GameState* state) {
    if (!state->radar.active) return;

    printf("\n--- LANDING RADAR DATA (Valid for %d more turns) ---\n", state->radar.turns_remaining);
    printf("RECOMMENDED LANDING ZONE: A=%.1f m (Safety: %.0f%%)\n",
           state->radar.safe_landing_x, state->radar.safe_landing_score);
    double distance_to_safe = fabs(state->A - state->radar.safe_landing_x);
    printf("Distance to recommended zone: %.1f m\n", distance_to_safe);
    if (distance_to_safe > 50) printf("ADVISORY: Recommend horizontal maneuvering\n");
    else if (distance_to_safe < 10) printf("ADVISORY: On approach to safe zone\n");
    printf("-----------------------------------------------\n");
}

void display_visualizer(GameState* state) {
    const int VIS_WIDTH = 61;
    const int VIS_HEIGHT = 16;
    const double WORLD_X_MIN = -100.0, WORLD_X_MAX = 100.0;

    double world_view_height_m;
    double world_y_min;

    if (state->B < 60.0) { // Low altitude: "Landing Mode"
        world_view_height_m = 40.0;
        world_y_min = -15.0;
    } else { // High altitude: "Approach Mode"
        world_view_height_m = 150.0;
        double world_y_max = state->B + 30.0;
        world_y_min = world_y_max - world_view_height_m;
    }

    char canvas[VIS_HEIGHT][VIS_WIDTH + 1];
    memset(canvas, ' ', sizeof(canvas));
    for (int i = 0; i < VIS_HEIGHT; i++) canvas[i][VIS_WIDTH] = '\0';

    double prev_terrain_h = 0.0;
    for (int x = 0; x < VIS_WIDTH; x++) {
        double world_x = WORLD_X_MIN + (x / (double)(VIS_WIDTH - 1)) * (WORLD_X_MAX - WORLD_X_MIN);
        double pos_in_array = (world_x - WORLD_X_MIN) / 10.0;
        int index1 = fmax(0, fmin(20, (int)floor(pos_in_array)));
        int index2 = fmax(0, fmin(20, (int)ceil(pos_in_array)));

        // Interpolation for nicer visual
        double terrain_h = (index1 == index2) ? state->radar.terrain_height[index1] : state->radar.terrain_height[index1] + (state->radar.terrain_height[index2] - state->radar.terrain_height[index1]) * (pos_in_array - index1);

        int canvas_y = (VIS_HEIGHT - 1) - (int)round(((terrain_h - world_y_min) / world_view_height_m) * (VIS_HEIGHT - 1));

        if (canvas_y >= 0 && canvas_y < VIS_HEIGHT) {
            char terrain_char = '_';
            if (x > 0) {
                if (terrain_h > prev_terrain_h + 0.5) terrain_char = '/';
                if (terrain_h < prev_terrain_h - 0.5) terrain_char = '\\';
            }
            prev_terrain_h = terrain_h;
            canvas[canvas_y][x] = terrain_char;
            for (int fill_y = canvas_y + 1; fill_y < VIS_HEIGHT; fill_y++) canvas[fill_y][x] = '#';
        }
    }

    int lander_x = (int)round(((state->A - WORLD_X_MIN) / (WORLD_X_MAX - WORLD_X_MIN)) * (VIS_WIDTH - 1));
    int lander_y = (VIS_HEIGHT - 1) - (int)round(((state->B - world_y_min) / world_view_height_m) * (VIS_HEIGHT - 1));

    if (lander_y >= 0 && lander_y < VIS_HEIGHT && lander_x >= 0 && lander_x < VIS_WIDTH) {
        if (canvas[lander_y][lander_x] == ' ') canvas[lander_y][lander_x] = 'A';
        if (state->engines_on && lander_y < VIS_HEIGHT - 1 && canvas[lander_y + 1][lander_x] == ' ') {
            canvas[lander_y + 1][lander_x] = '*';
        }
    }

    printf("\n.---[ RADAR VISUALS ]-----------------------------------------------.\n");
    for (int i = 0; i < VIS_HEIGHT; i++) {
        printf("| %s | %+.0fm\n", canvas[i], (world_y_min + world_view_height_m) - (i / (double)(VIS_HEIGHT - 1) * world_view_height_m));
    }
    printf("`------------------------------------------------------------------´\n");
    printf("  %-30s 0m %28s\n", "-100m", "+100m");
}

void display_status(GameState* state, GameConfig* config) {
    if (state->radar.active) {
        display_visualizer(state);
    }

    printf("\n--- LANDER STATUS ---\n");
    printf("A (X pos): %8.1f m\n", state->A);
    printf("B (Alt):   %8.1f m\n", state->B);

    if (config->display_delta_v) {
        double delta_h = state->vel_h - state->prev_vel_h;
        double delta_v = state->vel_v - state->prev_vel_v;
        printf("ΔV H:      %8.1f m/s\n", delta_h);
        printf("ΔV V:      %8.1f m/s\n", delta_v);
    } else {
        printf("Vel H:     %8.1f m/s  %s\n", state->vel_h, state->vel_h > 0 ? "->" : "<-");
        printf("Vel V:     %8.1f m/s  %s\n", state->vel_v, state->vel_v < 0 ? "v (Down)" : "^ (Up)");
    }

    printf("C (Fuel):  %8d burns\n", state->C);
    printf("Engines:   %s\n", state->engines_on ? "ON" : "OFF");

    if (state->radar.active) {
        printf("Radar:     ACTIVE (%d turns remaining)\n", state->radar.turns_remaining);
    } else {
        printf("Radar:     INACTIVE (use 'R' for visuals)\n");
    }
    printf("---------------------\n");
}

char get_command(void) {
    char command;
    printf("\nCommand: ");
    scanf(" %c", &command);
    while (getchar() != '\n');
    return command;
}

void update_physics(GameState* state, GameConfig* config, char move_command) {
    double dt = state->time_step;
    state->prev_vel_h = state->vel_h;
    state->prev_vel_v = state->vel_v;

    state->vel_v -= config->gravity * dt;

    if (state->engines_on) {
        if (move_command == 'Y') {
            state->vel_v += config->engine_force * dt;
            state->vel_h += config->engine_force * dt * 0.3;
        } else if (move_command == 'Z') {
            state->vel_v += config->engine_force * dt;
            state->vel_h -= config->engine_force * dt * 0.3;
        }
    }

    state->A += state->vel_h * dt;
    state->B += state->vel_v * dt;

    if (state->B < 0) state->B = 0;
}

int check_landing(GameState* state) {
    const double SAFE_VERTICAL_SPEED = 2.0;
    const double SAFE_HORIZONTAL_SPEED = 1.5;

    if (state->B <= 0) {
        double terrain_penalty = 0;
        if (state->A >= -100 && state->A <= 100) {
            int index = (int)((state->A + 100) / 10.0);
            if (index < 0) index = 0;
            if (index > 20) index = 20;
            terrain_penalty = fabs(state->radar.terrain_height[index]) * 0.2;
        }

        if (fabs(state->vel_v) < (SAFE_VERTICAL_SPEED - terrain_penalty) &&
            fabs(state->vel_h) < (SAFE_HORIZONTAL_SPEED - terrain_penalty)) {
            return 1; // Success
        } else {
            return -1; // Crash
        }
    }
    return 0; // Still flying
}

void save_result(GameState* state, const char* result) {
    FILE* fp = fopen("lander_results.txt", "a");
    if (fp) {
        time_t now = time(NULL);
        char time_str[26];
        ctime_r(&now, time_str);
        time_str[strlen(time_str) - 1] = 0; // Remove newline

        fprintf(fp, "[%s] - %s\n", time_str, result);
        fprintf(fp, "  Final Position: H=%.1f m, V=%.1f m\n", state->A, state->B);
        fprintf(fp, "  Impact Velocity: H=%.1f m/s, V=%.1f m/s\n", state->vel_h, state->vel_v);
        fprintf(fp, "  Fuel Remaining: %d burns\n", state->C);
        fprintf(fp, "  Landing Zone Safety: %.0f%% (at A=%.1f m)\n",
                calculate_landing_safety(state, state->A), state->A);
        fprintf(fp, "\n");
        fclose(fp);
        printf("Result saved to lander_results.txt\n");
    } else {
        printf("Error: Could not save result to file.\n");
    }
}

void configure_game(GameConfig* config) {
    int choice = 0;
    while (choice != 5) {
        printf("\n=== GAME CONFIGURATION ===\n");
        printf("1. Gravity:       %.2f m/s²\n", config->gravity);
        printf("2. Engine Force:  %.2f m/s²\n", config->engine_force);
        printf("3. Initial Fuel:  %d burns\n", config->initial_fuel);
        printf("4. Display Mode:  %s\n", config->display_delta_v ? "Delta V" : "m/s");
        printf("5. Return to game\n");
        printf("Choose setting to change (1-5): ");

        if (scanf("%d", &choice) != 1) choice = 0;
        while (getchar() != '\n'); // Clear input buffer

        switch (choice) {
            case 1:
                printf("Enter new gravity (e.g., 1.6 for Moon): ");
                scanf("%lf", &config->gravity);
                break;
            case 2:
                printf("Enter new engine force (m/s²): ");
                scanf("%lf", &config->engine_force);
                break;
            case 3:
                printf("Enter new initial fuel: ");
                scanf("%d", &config->initial_fuel);
                break;
            case 4:
                config->display_delta_v = !config->display_delta_v;
                printf("Display mode set to %s\n", config->display_delta_v ? "Delta V" : "m/s");
                break;
            case 5:
                printf("Returning to main menu...\n");
                break;
            default:
                printf("Invalid choice.\n");
                break;
        }
        if (choice > 0 && choice < 5) while (getchar() != '\n');
    }
}
