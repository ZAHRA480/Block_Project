/*
References for ncurses:
- https://web.mit.edu/barnowl/src/ncurses/ncurses-5.4/doc/html/ncurses-intro.html
- https://tldp.org/HOWTO/html_single/NCURSES-Programming-HOWTO
- ncurses manual documentation (man ncurses)
*/

#include <curses.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <fstream>
#include <string>

using namespace std;

const int BOARD_ROWS = 20;
const int BOARD_COLS = 10;
const int BLOCK_SIZE = 4;
const int TOP_MARGIN = 7;
const int GAME_WIDTH = BOARD_COLS * 3 + 8 + (BLOCK_SIZE * 3 + 2);
const int MAX_HISTORY_LINES = BOARD_ROWS + TOP_MARGIN - 2;
const int UPDATE_INTERVAL = 1000;
const int LEVELUP_SCORES = 500;
const int LEVEL_SPEED = 100;
const int MIN_UPDATE_INTERVAL = 100;

enum class BlockType {
    O,
    I,
    L,
    T,
    J,
    S,
    Z,
};

enum class CellState {
    Empty,
    O,
    I,
    L,
    T,
    J,
    S,
    Z,
};

enum class Direction {
    None,
    Down,
    Left,
    Right,
    Rotate,
};

enum class GameState {
    Menu,
    Playing,
    Paused,
    ConfirmQuit,
    GameOver,
    Exit
};


class Block {
private:
    BlockType type;
    int row;
    int col;
    int size;
    CellState shape[BLOCK_SIZE][BLOCK_SIZE];

    void InitializeShape() {
        for (int i = 0; i < BLOCK_SIZE; i++) {
            for (int j = 0; j < BLOCK_SIZE; j++) {
                shape[i][j] = CellState::Empty;
            }
        }

        switch (type) {
            case BlockType::O:
                size = 2;
                shape[0][0] = CellState::O;
                shape[0][1] = CellState::O;
                shape[1][0] = CellState::O;
                shape[1][1] = CellState::O;
                break;
            case BlockType::I:
                size = 4;
                shape[0][1] = CellState::I;
                shape[1][1] = CellState::I;
                shape[2][1] = CellState::I;
                shape[3][1] = CellState::I;
                break;
            case BlockType::L:
                size = 3;
                shape[0][0] = CellState::L;
                shape[1][0] = CellState::L;
                shape[2][0] = CellState::L;
                shape[2][1] = CellState::L;
                break;
            case BlockType::T:
                size = 3;
                shape[0][0] = CellState::T;
                shape[0][1] = CellState::T;
                shape[0][2] = CellState::T;
                shape[1][1] = CellState::T;
                break;
            case BlockType::J:
                size = 3;
                shape[0][1] = CellState::J;
                shape[1][1] = CellState::J;
                shape[2][1] = CellState::J;
                shape[2][0] = CellState::J;
                break;
            case BlockType::S:
                size = 3;
                shape[0][1] = CellState::S;
                shape[0][2] = CellState::S;
                shape[1][0] = CellState::S;
                shape[1][1] = CellState::S;
                break;
            case BlockType::Z:
                size = 3;
                shape[0][0] = CellState::Z;
                shape[0][1] = CellState::Z;
                shape[1][1] = CellState::Z;
                shape[1][2] = CellState::Z;
                break;
        }
    }
    void RotateShape() {
        CellState rotated[BLOCK_SIZE][BLOCK_SIZE];

        for (int i = 0; i < BLOCK_SIZE; i++) {
            for (int j = 0; j < BLOCK_SIZE; j++) {
                rotated[i][j] = CellState::Empty;
            }
        }

        for (int row = 0; row < size; row++) {
            for (int col = 0; col < size; col++) {
                rotated[col][size - row - 1] = shape[row][col];
            }
        }

        for (int i = 0; i < BLOCK_SIZE; i++) {
            for (int j = 0; j < BLOCK_SIZE; j++) {
                shape[i][j] = rotated[i][j];
            }
        }
    }

public:
    Block(BlockType type) : type(type), row(0), col(3), size(4) {
        InitializeShape();
    }

    CellState GetState() const {
        switch (type) {
            case BlockType::O: return CellState::O;
            case BlockType::I: return CellState::I;
            case BlockType::L: return CellState::L;
            case BlockType::T: return CellState::T;
            case BlockType::J: return CellState::J;
            case BlockType::S: return CellState::S;
            case BlockType::Z: return CellState::Z;
        }

        return CellState::Empty;
    }

    int GetRow() const {
        return row;
    }

    int GetCol() const {
        return col;
    }

    int GetSize() const {
        return size;
    }

    CellState GetCell(int r, int c) const {
        return shape[r][c];
    }

    void SetCell(int row, int col, CellState state) {
        shape[row][col] = state;
    }

    BlockType GetType() const {
        return type;
    }

    void SetRow(int r) {
        row = r;
    }

    void SetCol(int c) {
        col = c;
    }

    void MoveDown() {
        row++;
    }

    void MoveLeft() {
        col--;
    }

    void MoveRight() {
        col++;
    }

    void Rotate() {
        RotateShape();
    }
};


class Board {
private:
    CellState grid[BOARD_ROWS][BOARD_COLS];

    int GetColorPair(CellState state) const {
        switch (state) {
            case CellState::O: return 1;
            case CellState::I: return 2;
            case CellState::L: return 3;
            case CellState::T: return 4;
            case CellState::J: return 5;
            case CellState::S: return 6;
            case CellState::Z: return 7;
            default: return 0;
        }
    }

public:
    Board() {
        for (int i = 0; i < BOARD_ROWS; i++) {
            for (int j = 0; j < BOARD_COLS; j++) {
                grid[i][j] = CellState::Empty;
            }
        }
    }

    void Draw(const Block& block) const {
        for (int i = 0; i < BOARD_ROWS; i++) {
            for (int j = 0; j < BOARD_COLS; j++) {
                bool filled = (grid[i][j] != CellState::Empty);
                CellState state = grid[i][j];
                int localRow = i - block.GetRow();
                int localCol = j - block.GetCol();

                if (!filled && localRow >= 0 && localCol >= 0 && localRow < block.GetSize() && localCol < block.GetSize()) {
                    filled = (block.GetCell(localRow, localCol) != CellState::Empty);
                    state = block.GetCell(localRow, localCol);
                }

                if (filled) {
                    int pair = GetColorPair(state);
                    mvprintw(i + TOP_MARGIN, j * 3 + 5, "[");

                    if (pair != 0) {
                        attron(COLOR_PAIR(pair));
                        if (state == CellState::L) {
                            attron(A_DIM);
                        }
                    }
                    mvprintw(i + TOP_MARGIN, j * 3 + 6, "#");
                    if (pair != 0) {
                        if (state == CellState::L) {
                            attroff(A_DIM);
                        }
                        attroff(COLOR_PAIR(pair));
                    }

                    mvprintw(i + TOP_MARGIN, j * 3 + 7, "]");
                } else {
                    mvprintw(i + TOP_MARGIN, j * 3 + 5, "[ ]");
                }
            }
        }
    }

    void DrawNextBlock(BlockType type) const {
        Block previewBlock(type);

        int width = BLOCK_SIZE * 3 + 2;
        int height = BLOCK_SIZE + 2;

        int startY = TOP_MARGIN + 1;
        int startX = BOARD_COLS * 3 + 8;

        WINDOW* popup = newwin(height, width, startY, startX);

        if (popup == nullptr) {
            return;
        }

        box(popup, 0, 0);
        mvwprintw(popup, 0, 3, "NEXT");

        for (int row = 0; row < BLOCK_SIZE; row++) {
            for (int col = 0; col < BLOCK_SIZE; col++) {
                CellState state = previewBlock.GetCell(row, col);
                int pair = GetColorPair(state);

                int y = row + 1;
                int x = col * 3 + 1;

                if (state != CellState::Empty) {
                    mvwprintw(popup, y, x, "[");

                    if (pair != 0) {
                        wattron(popup, COLOR_PAIR(pair));

                        if (state == CellState::L) {
                            wattron(popup, A_DIM);
                        }
                    }

                    mvwprintw(popup, y, x + 1, "#");

                    if (pair != 0) {
                        if (state == CellState::L) {
                            wattroff(popup, A_DIM);
                        }
                        wattroff(popup, COLOR_PAIR(pair));
                    }
                    mvwprintw(popup, y, x + 2, "]");
                }
                else {
                    mvwprintw(popup, y, x, "[ ]");
                }
            }
        }

        wrefresh(popup);
        delwin(popup);
    }

    bool CanPlace(const Block& block, Direction direction) const {
        if (direction == Direction::Rotate) {
            Block rotatedBlock = block;
            rotatedBlock.Rotate();
            return CanPlace(rotatedBlock, Direction::None);
        }

        int offsetRow = 0;
        int offsetCol = 0;

        switch (direction) {
            case Direction::Down:
                offsetRow = 1;
                break;
            case Direction::Left:
                offsetCol = -1;
                break;
            case Direction::Right:
                offsetCol = 1;
                break;
            case Direction::None:
                break;
        }

        for (int row = 0; row < block.GetSize(); row++) {
            for (int col = 0; col < block.GetSize(); col++) {
                if (block.GetCell(row, col) != CellState::Empty) {
                    int bRow = block.GetRow() + row + offsetRow;
                    int bCol = block.GetCol() + col + offsetCol;

                    if (bRow < 0 || bRow >= BOARD_ROWS || bCol < 0 || bCol >= BOARD_COLS) {
                        return false;
                    }

                    if (grid[bRow][bCol] != CellState::Empty) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    void LockBlock(const Block& block) {
        for (int row = 0; row < block.GetSize(); row++) {
            for (int col = 0; col < block.GetSize(); col++) {
                if (block.GetCell(row, col) != CellState::Empty) {
                    int bRow = block.GetRow() + row;
                    int bCol = block.GetCol() + col;
                    grid[bRow][bCol] = block.GetState();
                }
            }
        }
    }

    int ClearLines() {
        int linesCleared = 0;

        for (int row = BOARD_ROWS - 1; row >= 0; row--) {
            bool full = true;
            for (int col = 0; col < BOARD_COLS; col++) {
                if (grid[row][col] == CellState::Empty) {
                    full = false;
                    break;
                }
            }

            if (full) {
                linesCleared++;
                for (int r = row; r > 0; r--) {
                    for (int c = 0; c < BOARD_COLS; c++) {
                        grid[r][c] = grid[r - 1][c];
                    }
                }

                for (int c = 0; c < BOARD_COLS; c++) {
                    grid[0][c] = CellState::Empty;
                }

                row++;
            }
        }
        return linesCleared;
    }

    CellState GetCell(int row, int col) const {
        return grid[row][col];
    }

    void SetCell(int row, int col, CellState state) {
        grid[row][col] = state;
    }
};


class Game {
private:
    Board board;
    Block currentBlock;
    BlockType nextBlock;
    string playerName;
    string message;
    int score;
    GameState state;

    int GetLevel() const {
        return (score / LEVELUP_SCORES) + 1;
    }

    int GetUpdateInterval() const {
        int level = GetLevel();
        int interval = UPDATE_INTERVAL - ((level - 1) * LEVEL_SPEED);
        if (interval < MIN_UPDATE_INTERVAL) {
            interval = MIN_UPDATE_INTERVAL;
        }
        return interval;
    }

    BlockType GetRandomBlockType() {
        int r = rand() % 7;
        switch (r) {
            case 0:
                return BlockType::O;
            case 1:
                return BlockType::I;
            case 2:
                return BlockType::L;
            case 3:
                return BlockType::T;
            case 4:
                return BlockType::J;
            case 5:
                return BlockType::S;
            case 6:
                return BlockType::Z;
            default:
                return BlockType::O;
        }
    }

    void EndGame() {
        board = Board();
        currentBlock = Block(GetRandomBlockType());
        nextBlock = GetRandomBlockType();
        playerName.clear();
        score = 0;
        clear();
        refresh();
        state = GameState::Menu;
    }

    void NewGame() {
        message.clear();
        EndGame();
        PromptPlayerName();
        state = GameState::Playing;
    }

    void HandleGameInput(int input) {
        if (input == 'a') {
            if (board.CanPlace(currentBlock, Direction::Left)) {
                currentBlock.MoveLeft();
            }
        } else if (input == 'd') {
            if (board.CanPlace(currentBlock, Direction::Right)) {
                currentBlock.MoveRight();
            }
        } else if (input == 's') {
            StepDown();
        } else if (input == 'w') {
            if (board.CanPlace(currentBlock, Direction::Rotate)) {
                currentBlock.Rotate();
            }
        } else if (input == 'p' || input == 27) {
            state = GameState::Paused;
        }
    }

    void HandleMainMenuInput(int input) {
        if (input == '1') {
            NewGame();
        } else if (input == '2') {
            LoadGame();
        } else if (input == '3') {
            ShowHistory();
        } else if (input == 27) {
            state = GameState::Exit;
        }
    }

    void HandlePauseInput(int input) {
        if (input == 'p') {
            state = GameState::Playing;
        } else if (input == 's') {
            if (SaveGame()) {
                AddToHistory("Unfinished");
                message = "Game saved!";
            } else {
                message = "Save failed.";
            }
            EndGame();
        } else if (input == 27) {
            state = GameState::ConfirmQuit;
        }
    }

    void HandleConfirmQuitInput(int input) {
        if (input == 'y') {
            AddToHistory("Unfinished");
            EndGame();
        } else if (input == 'n' || input == 27) {
            state = GameState::Paused;
        }
    }

    void SpawnBlock() {
        currentBlock = Block(nextBlock);
        nextBlock = GetRandomBlockType();
        if (!board.CanPlace(currentBlock, Direction::None)) {
            state = GameState::GameOver;
        }
    }

    bool SaveGame() {
        ofstream file("savegame.txt");
        if (!file.is_open())
            return false;

        file << playerName << endl;
        file << score << endl;

        // Board
        for (int i = 0; i < BOARD_ROWS; i++) {
            for (int j = 0; j < BOARD_COLS; j++) {
                file << (int)board.GetCell(i, j) << " ";
            }
            file << endl;
        }

        // Current block
        file << (int)currentBlock.GetType() << endl;
        file << currentBlock.GetRow() << endl;
        file << currentBlock.GetCol() << endl;

        for (int i = 0; i < BLOCK_SIZE; i++) {
            for (int j = 0; j < BLOCK_SIZE; j++) {
                file << (int)currentBlock.GetCell(i, j) << " ";
            }
            file << endl;
        }

        // Next block
        file << (int)nextBlock << endl;

        file.close();
        return file.good(); // https://cplusplus.com/reference/ios/ios/good/
    }

    void LoadGame() {
        message.clear();
        ifstream file("savegame.txt");

        if (!file.is_open()) {
            message = "No saved game found.";
            return;
        }

        getline(file, playerName);
        file >> score;

        for (int i = 0; i < BOARD_ROWS; i++) {
            for (int j = 0; j < BOARD_COLS; j++) {
                int value;
                file >> value;
                board.SetCell(i, j, (CellState)value);
            }
        }

        int type;
        int row;
        int col;

        file >> type;
        file >> row;
        file >> col;

        currentBlock = Block((BlockType)type);
        currentBlock.SetRow(row);
        currentBlock.SetCol(col);

        for (int i = 0; i < BLOCK_SIZE; i++) {
            for (int j = 0; j < BLOCK_SIZE; j++) {
                int value;
                file >> value;
                currentBlock.SetCell(i, j, (CellState)value);
            }
        }

        int nextType;
        file >> nextType;
        nextBlock = (BlockType)nextType;

        state = GameState::Playing;

        file.close();
    }

    void AddToHistory(string status) {
        ofstream file("history.txt", ios::app);

        if (!file.is_open())
            return;

        file << playerName << " | Score: " << score << " | Status: " << status << endl;

        file.close();
    }

    void ShowHistory() {
        ifstream file("history.txt");

        if (!file.is_open()) {
            message = "No history found.";
            return;
        }

        clear();
        mvprintw(0, 0, "====================================================");
        mvprintw(1, BOARD_COLS * 3 - 15, "GAME HISTORY");
        mvprintw(2, 0, "====================================================");

        vector<string> history;
        string line;

        while (getline(file, line)) {
            history.push_back(line);
        }

        int start = 0;
        if ((int)history.size() > MAX_HISTORY_LINES) {
            start = history.size() - MAX_HISTORY_LINES;
        }

        int row = 4;

        for (int i = start; i < (int)history.size(); i++) {
            mvprintw(row++, 2, "%s", history[i].c_str());
        }

        mvprintw(BOARD_ROWS + TOP_MARGIN + 4, 2, "Press any key...");
        mvprintw(BOARD_ROWS + TOP_MARGIN + 6, 0, "====================================================");
        refresh();
        timeout(-1);
        getch();
        timeout(0);
    }

public:
    Game() : currentBlock(GetRandomBlockType()), nextBlock(GetRandomBlockType()), score(0), state(GameState::Menu) {
    }

    void AddScore(int points) {
        switch (points) {
            case 1:
                score += 100;
                break;
            case 2:
                score += 300;
                break;
            case 3:
                score += 500;
                break;
            case 4:
                score += 800;
                break;
        }
    }

    void StepDown() {
        if (state == GameState::GameOver) return;
        if (board.CanPlace(currentBlock, Direction::Down)) {
            currentBlock.MoveDown();
        }
        else {
            board.LockBlock(currentBlock);
            int cleared = board.ClearLines();
            AddScore(cleared);
            SpawnBlock();
        }
    }

    void Run() {
        timeout(0);
        bool running = true;
        auto lastUpdate = chrono::steady_clock::now(); // https://en.cppreference.com/cpp/chrono/steady_clock

        while (running) {
            int input = getch();

            switch (state) {
                case GameState::Menu:
                    clear();
                    HandleMainMenuInput(input);
                    DrawMainMenu();
                    break;
                case GameState::Playing:
                    HandleGameInput(input);
                    {
                        auto now = chrono::steady_clock::now();
                        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastUpdate).count(); // https://en.cppreference.com/cpp/chrono/duration/duration_cast

                        if (elapsed >= GetUpdateInterval()) {
                            Update();
                            lastUpdate = now;
                        }
                    }
                    Draw();
                    break;
                case GameState::Paused:
                    HandlePauseInput(input);
                    Draw();
                    DrawPauseMenu();
                    break;
                case GameState::ConfirmQuit:
                    HandleConfirmQuitInput(input);
                    Draw();
                    DrawConfirmQuitMenu();
                    break;
                case GameState::GameOver:
                    Draw();
                    DrawGameOverMenu();
                    if (input == 27) {
                        AddToHistory("Completed");
                        EndGame();
                    }
                    break;
                case GameState::Exit:
                    running = false;
                    break;
            }
            napms(16);
        }
    }

    void PromptPlayerName() {
        echo();
        curs_set(TRUE);
        timeout(-1);

        clear();

        mvprintw(8, 10, "Enter your name:");
        move(10, 10);

        char buffer[32];
        getnstr(buffer, sizeof(buffer) - 1);

        playerName = buffer;

        if (playerName.empty())
            playerName = "Player";

        noecho();
        curs_set(FALSE);
        timeout(0);
    }

    void DrawUI() {
        // HEADER
        mvprintw(0, 0, "====================================================");
        mvprintw(1, BOARD_COLS * 3 - 15, "BLOCK GAME");
        mvprintw(2, 0, "====================================================");
        mvprintw(3, 1, "Player: %s", playerName.c_str());
        mvprintw(3, BOARD_COLS * 3 - 10, "| Score: %d", score);
        mvprintw(4, 0, "====================================================");
        mvprintw(6, 3, ".--------------------------------.");

        // FOOTER
        mvprintw(BOARD_ROWS + TOP_MARGIN, 3, ".________________________________.");
        for (int i = 0; i < BOARD_ROWS; i++) {
            mvprintw(i + TOP_MARGIN, 3, "|");
            mvprintw(i + TOP_MARGIN, BOARD_COLS * 3 + 6, "|");
        }
        mvprintw(BOARD_ROWS + TOP_MARGIN + 2, 0, "====================================================");
        mvprintw(BOARD_ROWS + TOP_MARGIN + 3, 1, "Controls:");
        mvprintw(BOARD_ROWS + TOP_MARGIN + 4, 3, "[A] Left   [S] Down   [D] Right");
        mvprintw(BOARD_ROWS + TOP_MARGIN + 5, 3, "[W] Rotate   [P/ESC] Pause Menu");
        mvprintw(BOARD_ROWS + TOP_MARGIN + 6, 0, "====================================================");
    }

    void DrawMainMenu() {
        mvprintw(0, 0, "====================================================");
        mvprintw(1, BOARD_COLS * 3 - 15, "BLOCK GAME");
        mvprintw(2, 0, "====================================================");
        mvprintw(BOARD_ROWS + TOP_MARGIN + 6, 0, "====================================================");

        int width = 28;
        int height = 9;
        int startY = TOP_MARGIN + (BOARD_ROWS - height) / 2;
        int startX = 5 + (BOARD_COLS * 3 - width) / 2;
        WINDOW* popup = newwin(height, width, startY, startX);

        box(popup, 0, 0);

        mvwprintw(popup, 1, 10, "Main Menu");
        mvwprintw(popup, 3, 4, "[1] New Game");
        mvwprintw(popup, 4, 4, "[2] Continue Game");
        mvwprintw(popup, 5, 4, "[3] History");
        mvwprintw(popup, 7, 4, "[ESC] Quit");

        wrefresh(popup);
        delwin(popup);

        if (!message.empty()) {
            mvprintw(BOARD_ROWS + TOP_MARGIN + 4, 2, "%s", message.c_str());
        }
    }

    void DrawPauseMenu() {
        int width = 28;
        int height = 7;
        int startY = TOP_MARGIN + (BOARD_ROWS - height) / 2;
        int startX = 5 + (BOARD_COLS * 3 - width) / 2;
        WINDOW* popup = newwin(height, width, startY, startX);

        box(popup, 0, 0);

        mvwprintw(popup, 1, 10, "PAUSED");
        mvwprintw(popup, 3, 4, "[P] Resume");
        mvwprintw(popup, 4, 4, "[S] Save Game");
        mvwprintw(popup, 5, 4, "[ESC] Back to Menu");

        wrefresh(popup);
        delwin(popup);
    }

    void DrawConfirmQuitMenu() {
        int width = 28;
        int height = 7;
        int startY = TOP_MARGIN + (BOARD_ROWS - height) / 2;
        int startX = 5 + (BOARD_COLS * 3 - width) / 2;
        WINDOW* popup = newwin(height, width, startY, startX);

        box(popup, 0, 0);

        mvwprintw(popup, 1, 10, "Are you sure?");
        mvwprintw(popup, 3, 4, "[Y] Yes");
        mvwprintw(popup, 4, 4, "[N] No");

        wrefresh(popup);
        delwin(popup);
    }

    void DrawGameOverMenu() {
        int width = 28;
        int height = 7;
        int startY = TOP_MARGIN + (BOARD_ROWS - height) / 2;
        int startX = 5 + (BOARD_COLS * 3 - width) / 2;
        WINDOW* popup = newwin(height, width, startY, startX);

        box(popup, 0, 0);

        mvwprintw(popup, 1, 8, "GAME OVER");
        mvwprintw(popup, 3, 4, "Final Score: %d", score);
        mvwprintw(popup, 5, 4, "Press ESC to Quit");

        wrefresh(popup);
        delwin(popup);
    }

    void Draw() {
        clear();
        DrawUI();
        board.Draw(currentBlock);
        refresh();
        board.DrawNextBlock(nextBlock);
    }

    void Update() {
        StepDown();
    }
};

int main() {
    srand(time(nullptr));
    initscr();
    // Colors
    start_color();
    use_default_colors();
    init_pair(1, COLOR_YELLOW, -1); // O
    init_pair(2, COLOR_CYAN, -1); // I
    init_pair(3, COLOR_RED, -1); // L
    init_pair(4, COLOR_MAGENTA, -1); // T
    init_pair(5, COLOR_BLUE, -1); // J
    init_pair(6, COLOR_GREEN, -1); // S
    init_pair(7, COLOR_RED, -1); // Z

    noecho();
    curs_set(FALSE);

    Game game;
    game.Run();

    endwin();

    return 0;
}