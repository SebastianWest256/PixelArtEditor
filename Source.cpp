#include <SDL.h>
#include <cmath>
#include <ctime>
#include <iostream>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <fstream>
#include "MathFunctions.cpp"
#include "RenderFunctions.cpp"
#include "InputHandling.cpp"

const int SCREEN_WIDTH = 1000;
const int SCREEN_HEIGHT = 800;
const int FPS = 60;
const int FRAME_DELAY = 1000 / FPS;

void initialize_SDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {

        exit(1);
    }
}

SDL_Window* create_window() {
    SDL_Window* window = SDL_CreateWindow("MapRender", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {

        SDL_Quit();
        exit(1);
    }
    return window;
}

SDL_Surface* get_window_surface(SDL_Window* window) {
    SDL_Surface* screen = SDL_GetWindowSurface(window);
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0x00, 0x00, 0x00));
    return screen;
}

input handle_events(bool& running, input current_input) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (current_input.mouse_pressed)current_input.mouse_reset = false;
        current_input.mouse_x = event.button.x;
        current_input.mouse_y = event.button.y;
        if (event.type == SDL_QUIT) {
            running = false;
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                current_input.mouse_pressed = true;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                current_input.mouse_pressed = false;
                current_input.mouse_reset = true;
            }
        }
        else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            SDL_Keycode keyCode = event.key.keysym.sym;
            auto it = key_map.find(keyCode);
            if (it != key_map.end()) {
                int index = it->second;
                current_input.key_pressed[index] = (event.type == SDL_KEYDOWN);
                if (SDL_KEYUP) {
                    current_input.key_reset[index] = true;
                }
            }
        }
    }
    return current_input;
}

void clean_up(SDL_Window* window) {
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void save_grid(std::string file_name, std::vector<std::vector<Uint32>> grid) {
    file_name += ".txt";

    std::ofstream outputFile(file_name);

    if (outputFile.is_open()) {

        for (int i = 0; i < grid[0].size(); i++) {
            for (int j = 0; j < grid.size(); j++) {

                outputFile << grid[j][i] << std::endl;

            }
        }

        outputFile.close();

    }
    else {
        std::cerr << "Failed to open the file." << std::endl;
    }
}

void load_grid(std::string file_name, std::vector<std::vector<Uint32>>* grid) {
    file_name += ".txt";
    Uint32 line_counter = 0;
    std::ifstream inputFile(file_name);
    if (inputFile.is_open()) {
        std::string line;
        while (std::getline(inputFile, line)) {
            if (std::stoi(line) == 100000000) {
                (*grid)[line_counter % grid->size()][(line_counter - line_counter % grid->size()) / grid->size()] = 100000000;
            }
            else {
                (*grid)[line_counter % grid->size()][(line_counter - line_counter % grid->size()) / grid->size()] = std::stoi(line);
            }
            line_counter++;
        }
        inputFile.close();
    }
}

bool on_button(input current_input, button button) {
    return current_input.mouse_reset && current_input.mouse_x > button.x && current_input.mouse_x < button.x_bound&& current_input.mouse_y > button.y && current_input.mouse_y < button.y_bound;
}

bool on_grid(input current_input, int x_offset, int y_offset, int cell_size, std::vector<std::vector<Uint32>> grid) {
    return current_input.mouse_x > x_offset && current_input.mouse_x < x_offset + (cell_size * grid.size()) && current_input.mouse_y > y_offset && current_input.mouse_y < y_offset + (cell_size * grid[0].size());
}

Uint32 get_final_color(Uint32 active_color, int variance) {
    int color_offset = int(random_float(0, variance * 2)) - variance;
    return get_color(std::min(std::max((get_RGB(active_color).r) + color_offset, 0), 255), std::min(std::max((get_RGB(active_color).g) + color_offset, 0), 255), std::min(std::max((get_RGB(active_color).b) + color_offset, 0), 255));
}

int main(int argc, char* argv[]) {

    srand(time(0));

    Uint32 frameStart;
    int frameTime;

    input current_input;

    initialize_SDL();

    SDL_Window* window = create_window();

    SDL_Surface* screen = get_window_surface(window);

    //init start

    int active_textbox = -1;
    bool using_textbox = false;
    bool display_grid_lines = false;
    bool symmetry_mode = false;

    Uint32 cell_size = 40;
    Uint32 x_offset = 180;
    Uint32 y_offset = 10;

    Uint32 pallet_x_offset = 870;
    Uint32 pallet_index_x = 0;
    Uint32 pallet_index_y = 0;

    Uint32 current_x_index = 0;
    Uint32 current_y_index = 0;

    Uint32 variance = 0;

    std::vector<std::vector<Uint32>> grid(16, std::vector<Uint32>(16, 100000000));

    std::vector<std::vector<Uint32>> pallet(2, std::vector<Uint32>(16));

    std::vector<textbox> textbox(std::vector<textbox>(5));

    std::vector<button> button(std::vector<button>(11));

    for (int i = 0; i < pallet[0].size(); i++) {
        for (int j = 0; j < pallet.size(); j++) {
            pallet[j][i] = get_color(random_float(0, 255), random_float(0, 255), random_float(0, 255));
        }
    }

    Uint32 active_color = pallet[0][0];

    for (int i = 0; i < 3; i++) {
        textbox[i].init(70, 481 + i * 60, 5, 3, 0x333333, 0x7777777, 0x444444);
    }

    textbox[3].init(70, 375, 5, 3, 0x333333, 0x7777777, 0x444444);
    textbox[3].text = "0";
    textbox[4].init(1, 740, 3, 32, 0x333333, 0x7777777, 0x444444);
    
    button[0].init(73, 440, 3, "COLOR", 0x333333, 0x777777, 0x444444);
    button[1].init(72, 350, 2, "VARIANCE", 0x555555, 0xBBBBBB, 0x555555);
    button[2].init(862, 660, 3, "ERASE", 0x333333, 0x777777, 0x444444);
    button[3].init(1, 1, 3, "GRID OFF", 0x333333, 0x777777, 0x444444);
    button[4].init(1, 35, 2, "SYMMETRY OFF", 0x333333, 0x777777, 0x444444);
    button[5].init(830, 760, 3, "CLEAR ALL", 0x333333, 0x777777, 0x444444);
    button[6].init(1, 660, 3, "SAVE ASSET", 0x333333, 0x777777, 0x444444);
    button[7].init(200, 660, 3, "SAVE PALLET", 0x333333, 0x777777, 0x444444);
    button[8].init(1, 700, 3, "LOAD ASSET", 0x333333, 0x777777, 0x444444);
    button[9].init(200, 700, 3, "LOAD PALLET", 0x333333, 0x777777, 0x444444);
    button[10].init(1, 60, 2, "BACKGROUND", 0x333333, 0x777777, 0x444444);

    //init end;

    SDL_UpdateWindowSurface(window);

    bool running = true;
    while (running) {

        frameStart = SDL_GetTicks();

        //loop start

        draw_rectangle(screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x444444);
        draw_grid(screen, pallet_x_offset, y_offset, cell_size, pallet, NULL);
        draw_grid(screen, x_offset, y_offset, cell_size, grid, symmetry_mode);

        if (display_grid_lines) {
            draw_grid_lines(screen, x_offset, y_offset, cell_size);
        }
        
        handle_textboxes(screen, &textbox, &active_textbox, &using_textbox, &current_input);
        handle_buttons(screen, &button);

        draw_square_outline(screen, pallet_x_offset + (pallet_index_x * cell_size), y_offset + (pallet_index_y * cell_size), cell_size, 3, 0xEEEEEE);
        draw_square_outline(screen, pallet_x_offset + (pallet_index_x * cell_size), y_offset + (pallet_index_y * cell_size), cell_size, 1, 0x222222);

        if (current_input.mouse_pressed) {

            if (on_grid(current_input, x_offset, y_offset, cell_size, grid)) {

                current_x_index = (current_input.mouse_x - x_offset) / cell_size;
                current_y_index = (current_input.mouse_y - y_offset) / cell_size;

                if (active_color != 100000000) {
                    
                    grid[current_x_index][current_y_index] = get_final_color(active_color, variance);

                    if (symmetry_mode) {
                        grid[current_x_index * -1 + 15][current_y_index] = get_final_color(active_color, variance);
                    }
                }
                else {
                    grid[current_x_index][current_y_index] = 100000000;

                    if (symmetry_mode) {
                        grid[current_x_index * -1 + 15][current_y_index] = 100000000;
                    }
                }
            }
            else if(on_grid(current_input, pallet_x_offset, y_offset, cell_size, pallet)) {

                current_x_index = (current_input.mouse_x - pallet_x_offset) / cell_size;
                current_y_index = (current_input.mouse_y - y_offset) / cell_size;

                pallet_index_x = current_x_index;
                pallet_index_y = current_y_index;

                active_color = pallet[current_x_index][current_y_index];

            }

        }

        if (current_input.mouse_pressed) {

            if (on_button(current_input, button[0])) {
                current_input.mouse_reset = false;
                if (is_integer(textbox[0].text) && is_integer(textbox[1].text) && is_integer(textbox[2].text)) {
                    pallet[pallet_index_x][pallet_index_y] = get_color(std::stoi(textbox[0].text), std::stoi(textbox[1].text), std::stoi(textbox[2].text));
                    active_color = pallet[pallet_index_x][pallet_index_y];
                }
            }
            else if (on_button(current_input, button[2])) {
                current_input.mouse_reset = false;
                pallet[pallet_index_x][pallet_index_y] = 100000000;
                active_color = pallet[pallet_index_x][pallet_index_y];
            }
            else if (on_button(current_input, button[3])) {
                display_grid_lines = !display_grid_lines;
                current_input.mouse_reset = false;

                if (button[3].text == "GRID OFF") {
                    button[3].text = "GRID ON";
                }
                else {
                    button[3].text = "GRID OFF";
                }
            }
            else if (on_button(current_input, button[4])) {
                current_input.mouse_reset = false;
                symmetry_mode = !symmetry_mode;
                current_input.mouse_pressed = false;

                if (button[4].text == "SYMMETRY OFF") {
                    button[4].text = "SYMMETRY ON";
                }
                else {
                    button[4].text = "SYMMETRY OFF";
                }
            }
           
            else if (on_button(current_input, button[5])) {
                current_input.mouse_reset = false;
               current_input.mouse_pressed = false;
                for (int i = 0; i < 16; i++) {
                    for (int j = 0; j < 16; j++) {
                        grid[j][i] = 100000000;
                    }
                }
            }
            else if (on_button(current_input, button[6])) {
                current_input.mouse_reset = false;
                save_grid(textbox[4].text, grid);
            }
            else if (on_button(current_input, button[7])) {
                current_input.mouse_reset = false;
                save_grid(textbox[4].text, pallet);
            }
            else if (on_button(current_input, button[8])) {
                current_input.mouse_reset = false;
                load_grid(textbox[4].text, &grid);
            }
            else if (on_button(current_input, button[9])) {
                current_input.mouse_reset = false;
                load_grid(textbox[4].text, &pallet);
            }
            else if (on_button(current_input, button[10])) {
                current_input.mouse_reset = false;
                for (int i = 0; i < 16; i++) {
                    for (int j = 0; j < 16; j++) {
                        grid[j][i] = get_final_color(active_color, variance);
                    }
                }
            }

        }
        
        if (is_integer(textbox[3].text)) {
            variance = std::stoi(textbox[3].text);
        }

        //loop end

        SDL_UpdateWindowSurface(window);

        frameTime = SDL_GetTicks() - frameStart;
        if (FRAME_DELAY > frameTime) {
            SDL_Delay(FRAME_DELAY - frameTime);
        }

        current_input = handle_events(running, current_input);
    }

    clean_up(window);

    return 0;
}