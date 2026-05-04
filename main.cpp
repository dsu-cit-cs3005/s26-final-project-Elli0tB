#include "Arena.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    Arena arena;
    arena.generateArena(argv[1]);
    arena.runSimulation();
    return 0;
}