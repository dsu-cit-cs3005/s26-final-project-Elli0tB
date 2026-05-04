#include "Arena.h"

Arena::Arena() : height_(10), width_(10), max_rounds_(100), 
                 sleep_interval_(0.5), game_state_live_(false),
                 num_flamethrowers_(0), num_pits_(0), num_mounds_(0) {
    grid_.resize(height_ * width_, '.');
}

Arena::Arena(const int& height, const int& width) : height_(height), width_(width) {
   grid_.resize(height_ * width_, '.');
}

Arena::~Arena() {
    for (auto robot : robots_) {
        delete robot;
    }
    robots_.clear();

    for (auto handle : lib_handles_) {
        dlclose(handle);
    }
    lib_handles_.clear();
}

// --- Grid ---

int Arena::getHeight() const { return height_; }
int Arena::getWidth() const { return width_; }

void Arena::setGridSize(const int& height, const int& width) {
    height_ = height;
    width_ = width;
    grid_.resize(height_ * width_, '.');
}

int Arena::index(const int& row, const int& column) const {
    return row * width_ + column;
}

bool Arena::indexValid(const int& row, const int& column) const {
    return row >= 0 && row < height_ && column >= 0 && column < width_;
}

char Arena::getValue(const int& row, const int& column) const {
    return grid_[index(row, column)];
}

void Arena::setValue(const int& row, const int& column, const char& value) {
    grid_[index(row, column)] = value;
}

// --- Robots ---

int Arena::getRobotCount() const { 
    return robots_.size(); 
}

void Arena::addRobot(RobotBase* robot) {
    robots_.push_back(robot);
}

void Arena::removeRobot(RobotBase* robot) {
    robots_.erase(std::remove(robots_.begin(), robots_.end(), robot), robots_.end());
}

void Arena::loadRobots() {
    namespace fs = std::filesystem;

    fs::path robots_dir = "./robots";

    if (!fs::exists(robots_dir) || !fs::is_directory(robots_dir)) {
        std::cerr << "No 'robots' directory found." << std::endl;
        return;
    }

    for (const auto& entry : fs::directory_iterator(robots_dir)) {
        // Only process files matching Robot_*.cpp
        if (!entry.is_regular_file()) continue;

        std::string filename = entry.path().string();
        std::string stem     = entry.path().stem().string(); // e.g. "Robot_MyRobot"

        if (stem.find("Robot_") != 0) continue;
        if (entry.path().extension() != ".cpp") continue;

        std::string shared_lib = "./robots/" + stem + ".so";

        // Step 1: Compile the .cpp into a .so
        std::string compile_cmd = "g++ -shared -fPIC -o " + shared_lib +
                                  " " + filename +
                                  " RobotBase.o -I. -std=c++20";

        std::cout << "Compiling " << filename << " to " << shared_lib << "...\n";
        int compile_result = std::system(compile_cmd.c_str());

        if (compile_result != 0) {
            std::cerr << "Failed to compile " << filename << std::endl;
            continue;  // skip this robot, try the next one
        }

        // Step 2: Load the compiled .so into memory
        void* handle = dlopen(shared_lib.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "Failed to load " << shared_lib
                      << ": " << dlerror() << std::endl;
            continue;
        }

        // Step 3: Look up the factory function by name
        RobotFactory create_robot = (RobotFactory)dlsym(handle, "create_robot");
        if (!create_robot) {
            std::cerr << "Failed to find create_robot in " << shared_lib
                      << ": " << dlerror() << std::endl;
            dlclose(handle);
            continue;
        }

        // Step 4: Call the factory to get a robot instance and add it
        RobotBase* robot = create_robot();
        addRobot(robot);
        lib_handles_.push_back(handle);  // store handle for cleanup later

        std::cout << "Loaded robot: " << robot->m_name << std::endl;
    }
}

// --- Simulation ---

void Arena::loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Split on ':'
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        if (key == "Arena_Size") {
            std::istringstream ss(value);
            ss >> height_ >> width_;
            setGridSize(height_, width_);
        } else if (key == "Max_Rounds") {
            max_rounds_ = std::stoi(value);
        } else if (key == "Sleep_interval") {
            sleep_interval_ = std::stof(value);
        } else if (key == "Game_State_Live") {
            game_state_live_ = (value.find("true") != std::string::npos);
        } else if (key == "Flamethrowers") {
            num_flamethrowers_ = std::stoi(value);
        } else if (key == "Pits") {
            num_pits_ = std::stoi(value);
        } else if (key == "Mounds") {
            num_mounds_ = std::stoi(value);
        }
    }

    setGridSize(height_,width_);
}

void Arena::placeObstacles() {
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> rowDist(0, height_ - 1);
    std::uniform_int_distribution<> colDist(0, width_ - 1);

    auto placeN = [&](int count, char symbol) {
        int placed = 0;
        while (placed < count) {
            int r = rowDist(gen);
            int c = colDist(gen);
            if (getValue(r, c) == '.') {
                setValue(r, c, symbol);
                ++placed;
            }
        }
    };

    placeN(num_flamethrowers_, 'F');
    placeN(num_pits_,          'P');
    placeN(num_mounds_,        'M');
}

void Arena::placeRobots() {
    const std::string ROBOT_CHARS = "@#$%^&*!~+";
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> rowDist(0, height_ - 1);
    std::uniform_int_distribution<> colDist(0, width_ - 1);

    for (int i = 0; i < (int)robots_.size(); ++i) {
        // Pick a random empty cell
        int r, c;
        do {
            r = rowDist(gen);
            c = colDist(gen);
        } while (getValue(r, c) != '.');

        // Assign unique character — fall back to index number if we run out
        char tag = (i < (int)ROBOT_CHARS.size()) ? ROBOT_CHARS[i] : ('0' + i);

        robots_[i]->move_to(r, c);
        robots_[i]->set_boundaries(height_, width_);
        setValue(r, c, tag);

        // Store the tag in the leaderboard entry for this robot
        robot_tags_[robots_[i]->m_name] = tag;
    }
}

void Arena::generateArena(const std::string& config_file){
    std::cout << "Loading config..." << std::endl;
    loadConfig(config_file);
    std::cout << "Arena size: " << height_ << "x" << width_ << std::endl;
    std::cout << "Loading robots..." << std::endl;
    loadRobots();
    std::cout << "Robots loaded: " << robots_.size() << std::endl;
    std::cout << "Placing obstacles..." << std::endl;
    placeObstacles();
    std::cout << "Placing robots..." << std::endl;
    placeRobots();
    std::cout << "Arena ready." << std::endl;
}

void Arena::runSimulation() {
    int round = 1;
    while (robots_.size() > 1 && round <= max_rounds_) {
        if (game_state_live_) {
            clearScreen();
            std::cout << "=== Round " << round << " ===" << std::endl;
            printGrid();
            printRoundSummary(round);
            usleep(sleep_interval_ * 1000000);
        }
        runTurn(round);
        round++;
    }
    RobotBase* winner = getWinner();
    if (winner) {
        leaderboard_.push_back({ winner->m_name, round - 1, true, getRobotTag(winner) });
        std::cout << "Winner: " << winner->m_name << std::endl;
    } 
    else if (robots_.size() > 1) {
        std::cout << "Max rounds reached - no winner declared." << std::endl;
        for (auto robot : robots_) {
            leaderboard_.push_back({ robot->m_name, round - 1, false, getRobotTag(robot) });
        }
    }
    else {
        std::cout << "No winner - all robots eliminated." << std::endl;
    }
}

void Arena::runTurn(int round) {
    event_log_.clear();

    for (auto robot : robots_) {
        if (robot->get_health() == 0) continue;
        performRadarScan(robot);
        handleShooting(robot);
    }

    for (auto robot : robots_) {
        if (robot->get_health() == 0) {
            leaderboard_.push_back({ robot->m_name, round, false, getRobotTag(robot) });
        }
    }

    robots_.erase(std::remove_if(robots_.begin(), robots_.end(),
        [](RobotBase* r){ return r->get_health() == 0; }), robots_.end());
}

RobotBase* Arena::getWinner() const {
    if (robots_.size() == 1) return robots_[0];
    return nullptr;
}

// --- Radar ---

RadarObj Arena::makeRadarEntry(int row, int col) const {
    RadarObj obj;
    obj.m_type = getValue(row, col);
    obj.m_row   = row;
    obj.m_col   = col;
    return obj;
}

void Arena::performRadarScan(RobotBase* robot) {
    std::vector<RadarObj> results;
    int rr;
    int rc;
    robot->get_current_location(rr,rc);
    int dir;
    robot->get_radar_direction(dir);  // 0–8

    if (dir == 0) {
        // Scan all 8 immediately surrounding cells
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr == 0 && dc == 0) continue; // skip self
                int nr = rr + dr;
                int nc = rc + dc;
                if (indexValid(nr, nc)) {
                    results.push_back(makeRadarEntry(nr, nc));
                }
            }
        }
    } else {
        // Direction vectors: index maps dir → (rowStep, colStep)
        // 1=Up, 2=Up-right, 3=Right, 4=Down-right,
        // 5=Down, 6=Down-left, 7=Left, 8=Up-left
        const int rowStep[] = { 0, -1, -1,  0,  1,  1,  1,  0, -1 };
        const int colStep[] = { 0,  0,  1,  1,  1,  0, -1, -1, -1 };

        int dr = rowStep[dir];
        int dc = colStep[dir];

        // Walk from the robot to the arena edge
        for (int d = 1; ; ++d) {
            int centerR = rr + dr * d;
            int centerC = rc + dc * d;

            // Stop when the center of the ray has left the arena
            if (!indexValid(centerR, centerC)) break;

            // The three cells in this "slice" of the 3-wide beam:
            //   center cell + the two flanking cells perpendicular to travel.
            //
            // For cardinal directions, the flanks shift along the cross-axis.
            // For diagonal directions, one flank shifts along the row axis,
            // the other along the column axis.
            std::array<std::pair<int,int>, 3> slice;

            if (dr == 0) {
                // Pure horizontal travel — flanks go up/down
                slice = {{ {centerR - 1, centerC},
                           {centerR,     centerC},
                           {centerR + 1, centerC} }};
            } else if (dc == 0) {
                // Pure vertical travel — flanks go left/right
                slice = {{ {centerR, centerC - 1},
                           {centerR, centerC    },
                           {centerR, centerC + 1} }};
            } else {
                // Diagonal travel — flanks shift along each axis individually
                // e.g. dir=2 (up-right, dr=-1 dc=+1):
                //   center=(r-d, c+d), flankA=(r-d, c+d-1), flankB=(r-d+1, c+d)
                slice = {{ {centerR,      centerC     },   // center
                           {centerR,      centerC - dc},   // flank: same row, back one col
                           {centerR - dr, centerC     } }}; // flank: back one row, same col
            }

            for (auto& [nr, nc] : slice) {
                if (!indexValid(nr, nc)) continue;
                // Skip the robot's own cell (shouldn't appear but be safe)
                if (nr == rr && nc == rc) continue;
                results.push_back(makeRadarEntry(nr, nc));
            }
        }
    }

    robot->process_radar_results(results);
}

// --- Combat ---

        
void Arena::handleShooting(RobotBase* shooter) {
    int target_row, target_col;
    if (!shooter->get_shot_location(target_row, target_col)) {
        resolveMovement(shooter);
        return;   // no valid target — don't proceed
    }
    WeaponType weapon = shooter->get_weapon();
    if (weapon == WeaponType::railgun) {
        handleRailgunShot(shooter, target_row, target_col);
    }
    if(weapon == WeaponType::hammer){
        handleHammer(shooter, target_row, target_col);
    }
    if(weapon == WeaponType::grenade){
        handleGrenadeShot(shooter, target_row, target_col);
        
    }
    if(weapon == WeaponType::flamethrower){
        handleFlameSpread(shooter, target_row, target_col);
    }
}

void Arena::handleRailgunShot(RobotBase* shooter, int target_row, int target_col) {
    const int RAILGUN_DMG_MIN = 10;
    const int RAILGUN_DMG_MAX = 20;
    const int damage = RAILGUN_DMG_MIN +
                       (std::rand() % (RAILGUN_DMG_MAX - RAILGUN_DMG_MIN + 1));

    int proj_row, proj_col;
    shooter->get_current_location(proj_row, proj_col);

    int row_diff = target_row - proj_row;
    int col_diff = target_col - proj_col;
    int delta_row = (row_diff == 0) ? 0 : (row_diff > 0 ? 1 : -1);
    int delta_col = (col_diff == 0) ? 0 : (col_diff > 0 ? 1 : -1);

    proj_row += delta_row;
    proj_col += delta_col;

    while (indexValid(proj_row, proj_col)) {
        if (getValue(proj_row, proj_col) != '.') {
            for (RobotBase* target : robots_) {
                if (target == shooter) continue;

                int temp_row, temp_col;
                target->get_current_location(temp_row, temp_col);

                if (temp_row == proj_row && temp_col == proj_col) {
                    handleHit(target, damage);
                    break;  // found the robot at this cell, move on to next cell
                }
            }
        }
        proj_row += delta_row;
        proj_col += delta_col;
    }
}

void Arena::handleHammer(RobotBase* shooter, int target_row, int target_col) {
    const int HAMMER_DMG_MIN = 50;
    const int HAMMER_DMG_MAX = 60;
    const int damage = HAMMER_DMG_MIN +
                       (std::rand() % (HAMMER_DMG_MAX - HAMMER_DMG_MIN + 1));

    int proj_row, proj_col;
    shooter->get_current_location(proj_row, proj_col);

    int row_diff = target_row - proj_row;
    int col_diff = target_col - proj_col;
    int delta_row = (row_diff == 0) ? 0 : (row_diff > 0 ? 1 : -1);
    int delta_col = (col_diff == 0) ? 0 : (col_diff > 0 ? 1 : -1);

    // Hammer hits exactly one adjacent cell
    int hit_row = proj_row + delta_row;
    int hit_col = proj_col + delta_col;

    if (!indexValid(hit_row, hit_col)) return;
    if (getValue(hit_row, hit_col) == '.') return;

    for (RobotBase* target : robots_) {
        if (target == shooter) continue;

        int temp_row, temp_col;
        target->get_current_location(temp_row, temp_col);

        if (temp_row == hit_row && temp_col == hit_col) {
            handleHit(target, damage);
            return;
        }
    }
}

void Arena::handleGrenadeShot(RobotBase* shooter, int target_row, int target_col) {
    if (shooter->get_grenades() <= 0) return;
    shooter->decrement_grenades();

    const int GRENADE_DMG_MIN = 10;
    const int GRENADE_DMG_MAX = 40;
    const int damage = GRENADE_DMG_MIN +
                       (std::rand() % (GRENADE_DMG_MAX - GRENADE_DMG_MIN + 1));

    // Scan the 3x3 blast radius centered on target
    for (int proj_row = target_row - 1; proj_row <= target_row + 1; ++proj_row) {
        for (int proj_col = target_col - 1; proj_col <= target_col + 1; ++proj_col) {
            if (!indexValid(proj_row, proj_col)) continue;  // blast clips arena edge

            if (getValue(proj_row, proj_col) != '.') {
                for (RobotBase* target : robots_) {
                    if (target == shooter) continue;

                    int temp_row, temp_col;
                    target->get_current_location(temp_row, temp_col);

                    if (temp_row == proj_row && temp_col == proj_col) {
                        handleHit(target, damage);
                        break;
                    }
                }
            }
        }
    }
}

void Arena::handleFlameSpread(RobotBase* shooter, int target_row, int target_col) {
    const int FLAME_RANGE   = 4;
    const int FLAME_DMG_MIN = 30;
    const int FLAME_DMG_MAX = 50;
    const int damage = FLAME_DMG_MIN +
                       (std::rand() % (FLAME_DMG_MAX - FLAME_DMG_MIN + 1));

    int origin_row, origin_col;
    shooter->get_current_location(origin_row, origin_col);

    int row_diff = target_row - origin_row;
    int col_diff = target_col - origin_col;
    int delta_row = (row_diff == 0) ? 0 : (row_diff > 0 ? 1 : -1);
    int delta_col = (col_diff == 0) ? 0 : (col_diff > 0 ? 1 : -1);

    // Perpendicular flank offsets — whichever axis we're NOT travelling on
    int flank_row, flank_col;
    if (delta_row == 0) {
        // Pure horizontal travel — flanks go up/down
        flank_row = 1;
        flank_col = 0;
    } else {
        // Vertical or diagonal travel — flanks go left/right
        flank_row = 0;
        flank_col = 1;
    }

    int proj_row = origin_row;
    int proj_col = origin_col;

    for (int step = 0; step < FLAME_RANGE; ++step) {
        proj_row += delta_row;
        proj_col += delta_col;

        // The three cells in this slice: center + two flanks
        std::array<std::pair<int,int>, 3> slice = {{
            { proj_row,            proj_col            },  // center
            { proj_row + flank_row, proj_col + flank_col },  // flank +1
            { proj_row - flank_row, proj_col - flank_col }   // flank -1
        }};

        for (auto& [r, c] : slice) {
            if (!indexValid(r, c)) continue;

            if (getValue(r, c) != '.') {
                for (RobotBase* target : robots_) {
                    if (target == shooter) continue;

                    int temp_row, temp_col;
                    target->get_current_location(temp_row, temp_col);

                    if (temp_row == r && temp_col == c) {
                        handleHit(target, damage);
                        break;
                    }
                }
            }
        }
    }
}

void Arena::handleHit(RobotBase* target, const int& damage) {
    int armor_score = target->get_armor();
    double damage_score = damage * (1.0 - armor_score * 0.1);

    target->take_damage(static_cast<int>(damage_score));
    target->reduce_armor(1);
    event_log_.push_back(target->m_name + " hit for " + std::to_string(static_cast<int>(damage_score)) + " damage.");
    if (target->get_health() <= 0) {
        event_log_.push_back(target->m_name + " has been eliminated.");
    }
}

void Arena::resolveMovement(RobotBase* robot) {
    int dir, speed;
    robot->get_move_direction(dir, speed);

    // Cap speed at the robot's maximum
    int max_speed = robot->get_move_speed();
    if (speed > max_speed) speed = max_speed;

    // Direction 0 means no movement
    if (dir == 0) return;

    const int rowStep[] = { 0, -1, -1,  0,  1,  1,  1,  0, -1 };
    const int colStep[] = { 0,  0,  1,  1,  1,  0, -1, -1, -1 };

    int delta_row = rowStep[dir];
    int delta_col = colStep[dir];

    int current_row, current_col;
    robot->get_current_location(current_row, current_col);

    for (int step = 1; step <= speed; ++step) {
        int next_row = current_row + delta_row;
        int next_col = current_col + delta_col;

        if (!indexValid(next_row, next_col)) {
            event_log_.push_back(robot->m_name + " movement stopped at arena boundary.");
            break;
        }

        char cell = getValue(next_row, next_col);
        
        if (cell == 'M' || cell == 'R' || cell == 'X'){
            // Impassable — stop in current cell, do not advance
            event_log_.push_back(robot->m_name + " movement blocked.");
            break;
        }

        if (cell == 'P') {
            // Robot falls in — move onto the pit cell and trap it
            setValue(current_row, current_col, '.');
            robot->move_to(next_row, next_col);
            robot->disable_movement();
            // Leave CELL_PIT value in place so the cell still reads as a pit
            event_log_.push_back(robot->m_name + " has fallen into a pit and is trapped.");
            break;
        }

        if (cell == 'F') {
            // Robot passes through — take flamethrower damage, keep moving
            const int FLAME_DMG_MIN = 30;
            const int FLAME_DMG_MAX = 50;
            int damage = FLAME_DMG_MIN + (std::rand() % (FLAME_DMG_MAX - FLAME_DMG_MIN + 1));

            setValue(current_row, current_col, '.');
            robot->move_to(next_row, next_col);
            // Temporarily mark robot on the flamethrower cell
            setValue(next_row, next_col, getRobotTag(robot));

            event_log_.push_back(robot->m_name + " moves through a flamethrower.");
            handleHit(robot, damage);

            if (robot->get_health() == 0 ) {
                // Dead robot now occupies the cell — flamethrower is destroyed
                setValue(next_row, next_col, 'X');
                event_log_.push_back(robot->m_name + " died on the flamethrower — it is destroyed.");
                break;
            }

            // Robot survived — restore the flamethrower beneath it as it moves on
            // The flamethrower cell will be restored on the next step when we clear current
            current_row = next_row;
            current_col = next_col;
            // Re-stamp flamethrower so it persists for future robots
            setValue(next_row, next_col, 'F');
            continue;
        }

        // Empty cell — normal move
        setValue(current_row, current_col, '.');
        robot->move_to(next_row, next_col);
        setValue(next_row, next_col, getRobotTag(robot));  // was 'R'

        current_row = next_row;
        current_col = next_col;
    }
}
// --- Output ---

void Arena::clearScreen() const {
   system("clear");
}

void Arena::printRoundSummary(const int& round) const {
    std::cout << "--- End of Round " << round << " ---" << std::endl;
    for (auto robot : robots_) {
        std::cout << "[" << getRobotTag(robot) << "] " << robot->print_stats() << std::endl;
    }
    if (!event_log_.empty()) {
        std::cout << "--- Events ---" << std::endl;
        for (const auto& event : event_log_) {
            std::cout << event << std::endl;
        }
    }
}


void Arena::printGrid() const {
    for (int r = 0; r < height_; r++) {
        for (int c = 0; c < width_; c++) {
            std::cout << grid_[index(r, c)] << " ";
        }
        std::cout << "\n";
    }
    std::cout.flush();
}

void Arena::printLeaderboard() const {
    std::cout << "\n=== Final Leaderboard ===" << std::endl;
    for (const auto& result : leaderboard_) {
        if (result.winner) {
            std::cout << result.name << " - WON on round " << result.round << std::endl;
        } else {
            std::cout << result.name << " - Eliminated on round " << result.round << std::endl;
        }
    }
}

// --- Helpers ---

bool Arena::isOccupied(const int& row, const int& col) const {
    return getValue(row, col) != '.';
}

bool Arena::isInBounds(const int& row, const int& col) const {
    return indexValid(row, col);
}

char Arena::getRobotTag(RobotBase* robot) const {
    auto it = robot_tags_.find(robot->m_name);
    if (it != robot_tags_.end()) return it->second;
    return '?';
}
