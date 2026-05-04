#include "RobotBase.h"
#include <vector>
#include <cmath>
#include <limits>

class Robot_Record_Player : public RobotBase {
public:
    Robot_Record_Player() : RobotBase(2, 5, railgun) {
        m_name         = "Record_Player";
        m_radar_dir    = 1;
        m_shoot_row    = 0;
        m_shoot_col    = 0;
        m_last_hp      = 100;
        m_took_damage  = false;
    }

    void get_radar_direction(int& radar_direction) override {
        // Spin the radar one step clockwise each turn (1-8 then wrap)
        radar_direction = m_radar_dir;
        m_radar_dir = (m_radar_dir % 8) + 1;
    }

    void process_radar_results(const std::vector<RadarObj>& radar_results) override {
        int row, col;
        get_current_location(row, col);

        // Check if we took damage since last turn
        m_took_damage = (get_health() < m_last_hp);
        m_last_hp = get_health();

        // Aim shot in the current radar direction from our position
        // Use direction vectors to pick a target cell far in that direction
        const int rowStep[] = { 0, -1, -1,  0,  1,  1,  1,  0, -1 };
        const int colStep[] = { 0,  0,  1,  1,  1,  0, -1, -1, -1 };

        // Shoot toward the edge in the current radar direction
        int dir = m_radar_dir == 1 ? 8 : m_radar_dir - 1; // direction we just scanned
        m_shoot_row = row + rowStep[dir] * m_board_row_max;
        m_shoot_col = col + colStep[dir] * m_board_col_max;

        // Find the farthest empty cell if we took damage
        if (m_took_damage) {
            m_flee_row = row;
            m_flee_col = col;
            double best_dist = -1.0;

            for (const auto& obj : radar_results) {
                if (obj.m_type == ' ') {
                    double dist = std::sqrt(
                        std::pow(obj.m_row - row, 2) +
                        std::pow(obj.m_col - col, 2)
                    );
                    if (dist > best_dist) {
                        best_dist = dist;
                        m_flee_row = obj.m_row;
                        m_flee_col = obj.m_col;
                    }
                }
            }
        }
    }

    bool get_shot_location(int& shot_row, int& shot_col) override {
        // If we took damage this turn, flee instead of shooting
        if (m_took_damage) return false;

        shot_row = m_shoot_row;
        shot_col = m_shoot_col;
        return true;
    }

    void get_move_direction(int& direction, int& distance) override {
        int row, col;
        get_current_location(row, col);
        distance = get_move_speed();

        // Calculate direction toward flee target
        int dr = (m_flee_row == row) ? 0 : (m_flee_row > row ? 1 : -1);
        int dc = (m_flee_col == col) ? 0 : (m_flee_col > col ? 1 : -1);

        if      (dr == -1 && dc ==  0) direction = 1;
        else if (dr == -1 && dc ==  1) direction = 2;
        else if (dr ==  0 && dc ==  1) direction = 3;
        else if (dr ==  1 && dc ==  1) direction = 4;
        else if (dr ==  1 && dc ==  0) direction = 5;
        else if (dr ==  1 && dc == -1) direction = 6;
        else if (dr ==  0 && dc == -1) direction = 7;
        else if (dr == -1 && dc == -1) direction = 8;
        else direction = 0;
    }

private:
    int  m_radar_dir;
    int  m_shoot_row;
    int  m_shoot_col;
    int  m_last_hp;
    bool m_took_damage;
    int  m_flee_row;
    int  m_flee_col;
};

extern "C" RobotBase* create_robot() {
    return new Robot_Record_Player();
}

extern "C" const char* robot_summary() {
    return "Spins and shoots. Runs when hurt.";
}