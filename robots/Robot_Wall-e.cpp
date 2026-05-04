#include "RobotBase.h"
#include <vector>

class Robot_WallCrawler : public RobotBase {
public:
    Robot_WallCrawler() : RobotBase(2, 5, flamethrower) {
        m_name = "Wall-E";

        // State machine phases:
        // 0 = moving to nearest wall
        // 1 = walking edges to next corner
        // 2 = attacking corner with flamethrower
        m_phase = 0;
        m_target_corner = 0;  // 0=top-left, 1=top-right, 2=bottom-right, 3=bottom-left
        m_shoot_this_turn = false;
        m_shoot_row = 0;
        m_shoot_col = 0;

        // Corner definitions (filled in once we know board size)
        m_corners_known = false;
    }

    void get_radar_direction(int& radar_direction) override {
        // Always scan immediately around us so we know what's nearby
        radar_direction = 0;
    }

    void process_radar_results(const std::vector<RadarObj>& radar_results) override {
        int row, col;
        get_current_location(row, col);

        // Build corner list once we have board boundaries
        if (!m_corners_known && m_board_row_max > 0) {
            m_corners = {
                {0,                  0                 },  // top-left
                {0,                  m_board_col_max - 1},  // top-right
                {m_board_row_max - 1, m_board_col_max - 1},  // bottom-right
                {m_board_row_max - 1, 0                 }   // bottom-left
            };
            m_corners_known = true;
        }

        m_shoot_this_turn = false;

        // Phase 2: shoot the current target corner
        if (m_phase == 2) {
            m_shoot_this_turn = true;
            m_shoot_row = m_corners[m_target_corner].first;
            m_shoot_col = m_corners[m_target_corner].second;

            // After shooting, advance to next corner and go back to walking edges
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
                m_phase = 2;  // at the corner — shoot next turn
            }
        }
    }

    bool get_shot_location(int& shot_row, int& shot_col) override {
        if (m_shoot_this_turn) {
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

        // Phase 0: move to nearest wall
        if (m_phase == 0) {
            direction = get_direction_to_nearest_wall(row, col);
            return;
        }

        // Phase 1: walk edges toward target corner
        if (m_phase == 1 && m_corners_known) {
            direction = get_direction_to_corner(row, col, m_target_corner);
            return;
        }

        direction = 0;  // no move
    }

private:
    int m_phase;
    int m_target_corner;
    bool m_shoot_this_turn;
    int m_shoot_row;
    int m_shoot_col;
    bool m_corners_known;
    std::vector<std::pair<int,int>> m_corners;

    int get_direction_to_nearest_wall(int row, int col) {
        // Find which wall is closest and head there
        int dist_up    = row;
        int dist_down  = m_board_row_max - 1 - row;
        int dist_left  = col;
        int dist_right = m_board_col_max - 1 - col;

        int min_dist = std::min({dist_up, dist_down, dist_left, dist_right});

        if (min_dist == dist_up)    return 1;  // Up
        if (min_dist == dist_down)  return 5;  // Down
        if (min_dist == dist_left)  return 7;  // Left
        return 3;                               // Right
    }

    int get_direction_to_corner(int row, int col, int corner_idx) {
        auto& corner = m_corners[corner_idx];
        int target_row = corner.first;
        int target_col = corner.second;

        int dr = (target_row == row) ? 0 : (target_row > row ? 1 : -1);
        int dc = (target_col == col) ? 0 : (target_col > col ? 1 : -1);

        // Map (dr, dc) back to direction number
        if (dr == -1 && dc ==  0) return 1;  // Up
        if (dr == -1 && dc ==  1) return 2;  // Up-right
        if (dr ==  0 && dc ==  1) return 3;  // Right
        if (dr ==  1 && dc ==  1) return 4;  // Down-right
        if (dr ==  1 && dc ==  0) return 5;  // Down
        if (dr ==  1 && dc == -1) return 6;  // Down-left
        if (dr ==  0 && dc == -1) return 7;  // Left
        if (dr == -1 && dc == -1) return 8;  // Up-left
        return 0;  // already there
    }
};

extern "C" RobotBase* create_robot() {
    return new Robot_WallCrawler();
}

// Required by grading arena/test harness: keep <= 50 chars.
extern "C" const char* robot_summary()
{
    return "Runs along the arena, hunting those that hide.";
}
