#ifndef ARENA_H
#define ARENA_H

#include "RobotBase.h"
#include "RadarObj.h"
#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <dlfcn.h>
#include <filesystem>
#include <random>
#include <fstream>
#include <sstream>  
#include <unistd.h>
#include <map>

struct RobotResult {
    std::string name;
    int round;
    bool winner;
    char special_char;
};

class Arena {
public:
    Arena();
    Arena(const int& height, const int& width);
    virtual ~Arena();

    // Grid management
    int getHeight() const;
    int getWidth() const;
    virtual void setGridSize(const int& height, const int& width);
    int index(const int& row, const int& column) const;
    bool indexValid(const int& row, const int& column) const;
    char getValue(const int& row, const int& column) const;
    void setValue(const int& row, const int& column, const char& value);

    // Robot management
    int getRobotCount() const;
    void loadRobots();
    void addRobot(RobotBase* robot);
    void removeRobot(RobotBase* robot);

    // Simulation
    void loadConfig(const std::string& filename);
    void placeObstacles();
    void placeRobots();
    void generateArena(const std::string& config_file);
    void runSimulation();
    void runTurn(int round);
    RobotBase* getWinner() const;

    // Radar
    RadarObj makeRadarEntry(int row, int col) const;
    void performRadarScan(RobotBase* robot);

    // Combat
    void handleShooting(RobotBase* shooter);
    void handleRailgunShot(RobotBase* shooter, int target_row, int target_col);
    void handleHammer(RobotBase* shooter, int target_row, int target_col);
    void handleGrenadeShot(RobotBase* shooter, int target_row, int target_col);
    void handleFlameSpread(RobotBase* shooter, int target_row, int target_col);
    void handleHit(RobotBase* target, const int& damage);
    void resolveMovement(RobotBase* robot);

    // Output
    void clearScreen() const;
    void printRoundSummary(const int& round) const;
    void printGrid() const;
    void printLeaderboard() const;

private:
    int height_;
    int width_;
    int max_rounds_;
    double sleep_interval_;
    bool game_state_live_;
    int num_flamethrowers_, num_pits_, num_mounds_;
    std::vector<char> grid_;
    std::vector<RobotBase*> robots_;
    std::vector<void*> lib_handles_;
    std::vector<RobotResult> leaderboard_;  // moved inside the class
    std::map<std::string, char> robot_tags_;
    std::vector<std::string> event_log_;

    // Helpers
    bool isOccupied(const int& row, const int& col) const;
    bool isInBounds(const int& row, const int& col) const;
    char getRobotTag(RobotBase* robot) const;
};

#endif
// Key: 'R'==robot, 'P'==pit, 'M'==mound, 'F'==flame turret, 'X'==dead robot, ' '==empty cell