#include "RobotBase.h"
#include <vector>
#include <cmath>

class Robot_Alleyoop : public RobotBase {
public:
    Robot_Alleyoop() : RobotBase(2, 5, grenade) {
        m_name = "Alleyoop";

        m_phase         = 0;
        m_target_corner = 0;
        m_shoot_this_turn = false;
        m_shoot_row     = 0;
        m_shoot_col     = 0;
        m_corners_known = false;
    }

    void get_radar_direction(int& radar_direction) override {
        // If we know the target corner, scan toward it so we can see
        // robots in the way and the corner itself
        if (m_corners_known) {
            int row, col;
            get_current_location(row, col);
            radar_direction = get_direction_to_corner(row, col, m_target_corner);
            if (radar_direction == 0) radar_direction = 1; // fallback
        } else {
            radar_direction = 0; // scan surroundings until we know the board
        }
    }

    void process_radar_results(const std::vector<RadarObj>& radar_results) override {
        int row, col;
        get_current_location(row, col);

        // Build corner list once we have board boundaries
        if (!m_corners_known && m_board_row_max > 0) {
            m_corners = {
                {0,                   0                  },  // top-left
                {0,                   m_board_col_max - 1},  // top-right
                {m_board_row_max - 1, m_board_col_max - 1},  // bottom-right
                {m_board_row_max - 1, 0                  }   // bottom-left
            };
            m_corners_known = true;
        }

        m_shoot_this_turn = false;

        // Phase 2: at the corner — shoot it then move on
        if (m_phase == 2) {
            m_shoot_this_turn = true;
            m_shoot_row = m_corners[m_target_corner].first;
            m_shoot_col = m_corners[m_target_corner].second;
            m_target_corner = (m_target_corner + 1) % 4;
            m_phase = 1;
            return;
        }

        // Phase 0: check if we've reached a wall
        if (m_phase == 0) {
            bool on_wall = (row == 0 || col == 0 ||
                            row == m_board_row_max - 1 ||
                            col == m_board_col_max - 1);
            if (on_wall) m_phase = 1;
        }

        // Phase 1: check if we've reached the target corner
        if (m_phase == 1 && m_corners_known) {
            auto& corner = m_corners[m_target_corner];
            if (row == corner.first && col == corner.second) {
                m_phase = 2;
                return;
            }
        }

        // Priority: if we see a robot in the radar results, shoot it
        // instead of the corner
        if (get_grenades() > 0) {
            for (const auto& obj : radar_results) {
                // Robot cells are non-empty, non-obstacle characters
                if (obj.m_type != ' ' && obj.m_type != 'M' &&
                    obj.m_type != 'P' && obj.m_type != 'F' &&
                    obj.m_type != 'X') {
                    // Found a live robot — shoot it directly
                    m_shoot_this_turn = true;
                    m_shoot_row = obj.m_row;
                    m_shoot_col = obj.m_col;
                    return;
                }
            }

            // No robot spotted — if we can see the corner from here, shoot it
            if (m_corners_known && can_see_corner(row, col, radar_results)) {
                m_shoot_this_turn = true;
                m_shoot_row = m_corners[m_target_corner].first;
                m_shoot_col = m_corners[m_target_corner].second;
                m_target_corner = (m_target_corner + 1) % 4;
            }
        }
    }

    bool get_shot_location(int& shot_row, int& shot_col) override {
        if (m_shoot_this_turn && get_grenades() > 0) {
            shot_row = m_shoot_row;
            shot_col = m_shoot_col;
            return true;
        }
        return false;
    }

    void get_move_direction(int& direction, int& distance) override {
        int row, col;
        get_current_location(row, col);
        distance = get_move_speed();

        if (m_phase == 0) {
            direction = get_direction_to_nearest_wall(row, col);
            return;
        }

        if (m_phase == 1 && m_corners_known) {
            direction = get_direction_to_corner(row, col, m_target_corner);
            return;
        }

        direction = 0;
    }

private:
    int  m_phase;
    int  m_target_corner;
    bool m_shoot_this_turn;
    int  m_shoot_row;
    int  m_shoot_col;
    bool m_corners_known;
    std::vector<std::pair<int,int>> m_corners;

    // Check if the target corner appears in the radar results
    bool can_see_corner(int row, int col,
                        const std::vector<RadarObj>& radar_results) {
        auto& corner = m_corners[m_target_corner];
        for (const auto& obj : radar_results) {
            if (obj.m_row == corner.first && obj.m_col == corner.second) {
                return true;
            }
        }
        return false;
    }

    int get_direction_to_nearest_wall(int row, int col) {
        int dist_up    = row;
        int dist_down  = m_board_row_max - 1 - row;
        int dist_left  = col;
        int dist_right = m_board_col_max - 1 - col;
        int min_dist   = std::min({dist_up, dist_down, dist_left, dist_right});

        if (min_dist == dist_up)   return 1;
        if (min_dist == dist_down) return 5;
        if (min_dist == dist_left) return 7;
        return 3;
    }

    int get_direction_to_corner(int row, int col, int corner_idx) {
        auto& corner   = m_corners[corner_idx];
        int target_row = corner.first;
        int target_col = corner.second;

        int dr = (target_row == row) ? 0 : (target_row > row ? 1 : -1);
        int dc = (target_col == col) ? 0 : (target_col > col ? 1 : -1);

        if (dr == -1 && dc ==  0) return 1;
        if (dr == -1 && dc ==  1) return 2;
        if (dr ==  0 && dc ==  1) return 3;
        if (dr ==  1 && dc ==  1) return 4;
        if (dr ==  1 && dc ==  0) return 5;
        if (dr ==  1 && dc == -1) return 6;
        if (dr ==  0 && dc == -1) return 7;
        if (dr == -1 && dc == -1) return 8;
        return 0;
    }
};

extern "C" RobotBase* create_robot() {
    return new Robot_Alleyoop();
}

extern "C" const char* robot_summary() {
    return "Walks edges, grenades corners, targets enemies first.";
}