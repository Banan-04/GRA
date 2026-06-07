#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace Gdiplus;

namespace {
constexpr int WIDTH = 1000;
constexpr int HEIGHT = 700;
constexpr int FPS = 60;
constexpr int FLOOR_Y = 600;
constexpr int FLOOR_H = 100;
constexpr int PLAYER_DRAW_BLOCK = 3;

const COLORREF BG = RGB(145, 105, 15);
const COLORREF PLAT_COL = RGB(255, 180, 80);
const COLORREF SPIKE_COL = RGB(180, 30, 30);
const COLORREF PLAYER_BODY = RGB(18, 18, 18);
const COLORREF RED = RGB(180, 30, 30);
const COLORREF GREEN = RGB(50, 160, 60);
const COLORREF WHITE = RGB(255, 255, 255);
const COLORREF BLACK = RGB(0, 0, 0);
const COLORREF GRAY = RGB(160, 160, 160);
const COLORREF LIGHT_GRAY = RGB(192, 192, 192);
const COLORREF DARK_GRAY = RGB(50, 50, 50);
const COLORREF YELLOW = RGB(240, 200, 40);
const COLORREF ORANGE = RGB(240, 130, 40);
const COLORREF CYAN = RGB(60, 200, 220);

struct IRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    int right() const { return x + w; }
    int bottom() const { return y + h; }
    int centerX() const { return x + w / 2; }
    int centerY() const { return y + h / 2; }

    bool intersects(const IRect& other) const {
        return x < other.right() && right() > other.x &&
               y < other.bottom() && bottom() > other.y;
    }
};

IRect rect(int x, int y, int w, int h) {
    return IRect{x, y, w, h};
}

IRect makeFloor(int x, int w) {
    return IRect{x, FLOOR_Y, w, FLOOR_H};
}

struct LevelText {
    const char* sentence;
    const char* correct;
    const char* wrong;
};

const LevelText LEVEL_DATA[] = {
    {"Machines that can move freely around the real world are the main focus of mobile ___ .", "robotics", "software"},
    {"Boston Dynamics is a famous company currently owned by the ___ Motor Group.", "Hyundai", "Tesla"},
    {"Spot is called a quadruped because it walks on ___ legs.", "four", "two"},
    {"Spot is used in places like deep mines because they are too ___ for humans.", "dangerous", "safe"},
    {"To measure distances and create 3D maps with lasers, the robot uses ___ .", "LiDAR", "Radar"},
    {"In reinforcement learning, the robot's brain learns by ___ and error.", "trial", "code"},
    {"Extra equipment like sensors or cameras carried on the robot's back is called a ___ .", "payload", "backpack"},
    {"Architects use Spot's scans to create a perfect digital ___ of a construction site.", "twin", "photo"},
    {"To instantly re-map its path and avoid obstacles, Spot uses sensor ___ .", "fusion", "bumper"},
    {"On slippery terrain, the balance of each step is calculated individually by the ___ .", "AI", "human"},
    {"The headquarters of Boston Dynamics is located in the city of ___ .", "Waltham", "Tokyo"},
    {"To get human-like 3D vision, Spot uses a ___ camera.", "stereo", "simple"},
    {"Walking from point A to point B completely on its own is called ___ navigation.", "autonomous", "remote"},
    {"Finding factory machines that are getting too hot is called a ___ inspection.", "thermal", "visual"},
    {"Spot can carry sensors to find invisible, dangerous ___ leaks before they explode.", "gas", "water"},
};

const char* DEATH_MESSAGES[] = {
    "GOTCHA!", "NOPE!", "TRY AGAIN!", "HA HA!",
    "TROLLED!", "OOPS!", "NOT TODAY!", "NICE TRY!",
    "LOL!", "DESTROYED!", "RIP!", "SURPRISE!",
};

HFONT makeFont(int px) {
    return CreateFontA(
        -px, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
}

RECT toWinRect(const IRect& r) {
    return RECT{r.x, r.y, r.x + r.w, r.y + r.h};
}

void fillRect(HDC hdc, const IRect& r, COLORREF color) {
    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    HBRUSH brush = CreateSolidBrush(color);
    RECT wr = toWinRect(r);
    FillRect(hdc, &wr, brush);
    DeleteObject(brush);
}

void outlineRect(HDC hdc, const IRect& r, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    Rectangle(hdc, r.x, r.y, r.x + r.w, r.y + r.h);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
}

void fillEllipse(HDC hdc, const IRect& r, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    Ellipse(hdc, r.x, r.y, r.x + r.w, r.y + r.h);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void outlineEllipse(HDC hdc, const IRect& r, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    Ellipse(hdc, r.x, r.y, r.x + r.w, r.y + r.h);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
}

void drawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    MoveToEx(hdc, x1, y1, nullptr);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void fillPolygon(HDC hdc, const POINT* points, int count, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    Polygon(hdc, points, count);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void alphaFillRect(HDC hdc, int x, int y, int w, int h, int r, int g, int b, int a) {
    Graphics graphics(hdc);
    SolidBrush brush(Color(static_cast<BYTE>(a), static_cast<BYTE>(r), static_cast<BYTE>(g), static_cast<BYTE>(b)));
    graphics.FillRectangle(&brush, x, y, w, h);
}

void alphaFillEllipse(HDC hdc, int x, int y, int w, int h, int r, int g, int b, int a) {
    Graphics graphics(hdc);
    SolidBrush brush(Color(static_cast<BYTE>(a), static_cast<BYTE>(r), static_cast<BYTE>(g), static_cast<BYTE>(b)));
    graphics.FillEllipse(&brush, x, y, w, h);
}

void drawText(HDC hdc, const std::string& text, HFONT font, COLORREF color, int x, int topY) {
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    SetBkMode(hdc, TRANSPARENT);
    SetTextAlign(hdc, TA_LEFT | TA_TOP);
    SetTextColor(hdc, color);
    TextOutA(hdc, x, topY, text.c_str(), static_cast<int>(text.size()));
    SelectObject(hdc, oldFont);
}

SIZE textSize(HDC hdc, const std::string& text, HFONT font) {
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    SIZE size{};
    GetTextExtentPoint32A(hdc, text.c_str(), static_cast<int>(text.size()), &size);
    SelectObject(hdc, oldFont);
    return size;
}

void drawTopCentered(HDC hdc, const std::string& text, HFONT font, COLORREF color, int centerX, int topY) {
    SIZE size = textSize(hdc, text, font);
    drawText(hdc, text, font, color, centerX - size.cx / 2, topY);
}

void drawFitTopCentered(HDC hdc, const std::string& text, int basePx, COLORREF color, int centerX, int topY, int maxWidth) {
    int px = basePx;
    HFONT font = makeFont(px);
    while (textSize(hdc, text, font).cx > maxWidth && px > 13) {
        DeleteObject(font);
        px--;
        font = makeFont(px);
    }
    drawTopCentered(hdc, text, font, color, centerX, topY);
    DeleteObject(font);
}

std::wstring executableDir() {
    wchar_t buffer[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::wstring path(buffer);
    std::size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return L".";
    }
    return path.substr(0, slash);
}

void drawBlockyRunner(HDC hdc, int centerX, int baseY, int block, double phase, bool walking, bool airborne, int facing) {
    // Blocky runner on a 10x12 cell grid: anchored by its feet at baseY and
    // centred on centerX. Three parts only -- head, torso, legs -- shaped after
    // the reference icon. Legs have just two arrangements: standing (two
    // straight vertical bars) and a two-frame walk cycle that toggles between
    // an open and a closed stride to fake the running motion.
    const int cols = 10;
    const int rows = 12;
    int sx = centerX - (cols * block) / 2;
    int sy = baseY - rows * block;

    auto drawCell = [&](int gx, int gy, int gw, int gh) {
        if (facing < 0) {
            gx = cols - gx - gw;   // mirror horizontally when facing left
        }
        fillRect(hdc, rect(sx + gx * block, sy + gy * block, gw * block, gh * block), PLAYER_BODY);
    };

    // --- Head + torso (identical in every frame) ---
    drawCell(3, 0, 4, 4);   // head  : cols 3-6, rows 0-3
    drawCell(2, 4, 6, 4);   // torso : cols 2-7, rows 4-7

    // --- Legs ---
    // Standing: two straight vertical bars hanging from the hips.
    auto drawStandingLegs = [&]() {
        drawCell(2, 8, 2, 4);   // left  leg
        drawCell(6, 8, 2, 4);   // right leg
    };

    // Walk frame A: stride open -- both feet splayed outward.
    auto drawStepA = [&]() {
        drawCell(2, 8, 2, 2);   // left  thigh
        drawCell(1, 10, 2, 2);  // left  foot, kicked back
        drawCell(6, 8, 2, 2);   // right thigh
        drawCell(7, 10, 2, 2);  // right foot, reaching forward
    };

    // Walk frame B: stride closed -- feet drawn together in the middle.
    auto drawStepB = [&]() {
        drawCell(2, 8, 2, 2);   // left  thigh
        drawCell(3, 10, 2, 2);  // left  foot, swung forward
        drawCell(6, 8, 2, 2);   // right thigh
        drawCell(5, 10, 2, 2);  // right foot, swung back
    };

    if (walking && !airborne) {
        // Only two leg poses exist; toggle between them to fake walking.
        int legFrame = static_cast<int>(phase / 7.2) % 2;
        if (legFrame == 0) {
            drawStepA();
        } else {
            drawStepB();
        }
    } else {
        drawStandingLegs();
    }
}
}

class AngolGame {
public:
    int run(HINSTANCE instance, int showCommand) {
        GdiplusStartupInput gdiplusInput;
        if (GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr) != Ok) {
            MessageBoxA(nullptr, "Could not start GDI+.", "ANGOL", MB_ICONERROR);
            return 1;
        }

        fontSm = makeFont(16);
        fontMd = makeFont(22);
        fontLg = makeFont(26);
        fontXl = makeFont(42);
        fontXxl = makeFont(56);
        fontBubble = makeFont(14);

        WNDCLASSA wc{};
        wc.lpfnWndProc = &AngolGame::windowProc;
        wc.hInstance = instance;
        wc.lpszClassName = "AngolGameWindow";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(BG);

        RegisterClassA(&wc);

        RECT windowRect{0, 0, WIDTH, HEIGHT};
        AdjustWindowRect(&windowRect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
        hwnd = CreateWindowExA(
            0,
            wc.lpszClassName,
            "ANGOL - Rage Platformer C++",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top,
            nullptr,
            nullptr,
            instance,
            this);

        if (!hwnd) {
            cleanup();
            return 1;
        }

        ShowWindow(hwnd, showCommand);
        UpdateWindow(hwnd);

        MSG msg{};
        while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        cleanup();
        return static_cast<int>(msg.wParam);
    }

private:
    enum class State {
        Menu,
        Play,
        Dying,
        WinLevel,
        End,
        Image
    };

    struct Spike {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
        std::string dir = "up";
    };

    struct SpikePos {
        double x = 0;
        int h = 0;
    };

    struct Trap {
        std::string type;
        std::string dir = ">";
        std::string side;
        double triggerX = 0;
        IRect rect{};
        IRect leftRect{};
        IRect rightRect{};
        IRect platformRect{};
        std::vector<SpikePos> positions;
        bool active = false;
        bool gone = false;
        bool landed = false;
        bool visible = false;
        bool escaped = false;
        bool sliding = false;
        bool origSet = false;
        int delay = 0;
        int timer = 0;
        int shake = 0;
        double x = 0;
        double y = 0;
        double speed = 0;
        int w = 0;
        int h = 0;
        int direction = 1;
        int triggerDist = 0;
        double escapeY = 0;
        int targetY = 0;
        int wallY = 0;
        double leftX = 0;
        double rightX = 0;
        int wallW = 0;
        int wallH = 0;
        int minGap = 50;
        double progress = 0;
        double openProgress = 0;
        int pitX = 0;
        int pitW = 0;
        int origLx = 0;
        int origRx = 0;

        explicit Trap(std::string trapType) : type(std::move(trapType)) {}

        Trap& trigger(double value, std::string directionName) {
            triggerX = value;
            dir = std::move(directionName);
            return *this;
        }

        Trap& withRect(IRect value) {
            rect = value;
            return *this;
        }

        Trap& withDelay(int value) {
            delay = value;
            return *this;
        }

        Trap& withY(double value) {
            y = value;
            return *this;
        }

        Trap& withXY(double px, double py) {
            x = px;
            y = py;
            return *this;
        }

        Trap& withSize(int width, int height) {
            w = width;
            h = height;
            return *this;
        }

        Trap& withSpeed(double value) {
            speed = value;
            return *this;
        }

        Trap& withActive(bool value) {
            active = value;
            return *this;
        }

        Trap& withDirection(int value) {
            direction = value;
            return *this;
        }

        Trap& withPositions(std::initializer_list<SpikePos> values) {
            positions.assign(values.begin(), values.end());
            return *this;
        }

        Trap& withPositions(const std::vector<SpikePos>& values) {
            positions = values;
            return *this;
        }

        Trap& withSide(std::string value) {
            side = std::move(value);
            return *this;
        }

        Trap& withTriggerDist(int value) {
            triggerDist = value;
            return *this;
        }

        Trap& withEscapeY(double value) {
            escapeY = value;
            return *this;
        }

        Trap& withPlatform(IRect value) {
            platformRect = value;
            return *this;
        }

        Trap& withTargetY(int value) {
            targetY = value;
            return *this;
        }

        Trap& withClosingWalls(double lx, double rx, int ww, int wh, int wy, double wallSpeed, int gap) {
            leftX = lx;
            rightX = rx;
            wallW = ww;
            wallH = wh;
            wallY = wy;
            speed = wallSpeed;
            minGap = gap;
            return *this;
        }

        Trap& withPit(IRect left, IRect right, int px, int width) {
            leftRect = left;
            rightRect = right;
            pitX = px;
            pitW = width;
            return *this;
        }
    };

    struct Level {
        std::vector<IRect> platforms;
        std::vector<IRect> ghostPlats;
        std::vector<Spike> spikes;
        std::vector<Trap> traps;
        std::vector<IRect> walls;
        std::vector<IRect> deathPlats;
        IRect doorTop{};
        IRect doorBottom{};
        IRect winSpikeRect{};
        bool hasWinSpike = false;
        std::string topAnswer;
        std::string bottomAnswer;
        std::string correctSide;
        std::string correctWord;
        std::string sentence;
        int playerStartX = 60;
        int playerStartY = FLOOR_Y - 40;
        bool finalWait = false;
        bool controlsReversed = false;
        bool jumpBlocked = false;
        bool jumpReversed = false;
        bool superGravity = false;
        double gravityMult = 1.0;
    };

    struct Particle {
        double x = 0;
        double y = 0;
        double vx = 0;
        double vy = 0;
        COLORREF color = WHITE;
        int life = 0;
        int maxLife = 0;
        int size = 1;

        void update() {
            x += vx;
            y += vy;
            vy += 0.2;
            life--;
        }

        void draw(HDC hdc) const {
            double alpha = std::max(0.0, life / static_cast<double>(maxLife));
            int s = std::max(1, static_cast<int>(size * alpha));
            fillRect(hdc, rect(static_cast<int>(x) - s / 2, static_cast<int>(y) - s / 2, s, s), color);
        }
    };

    struct Player {
        int w = 15;
        int h = 25;
        double x = 0;
        double y = 0;
        double vx = 0;
        double vy = 0;
        double speed = 4.5;
        double jumpPower = -11.0;
        double gravity = 0.55;
        bool onGround = false;
        bool alive = true;
        int facing = 1;
        int squish = 0;
        double stretch = 0;
        double walk = 0;
        bool controlsReversed = false;
        bool jumpBlocked = false;
        bool jumpReversed = false;
        bool superGravity = false;
        double gravityMult = 1.0;

        IRect bodyRect() const {
            return rect(static_cast<int>(x), static_cast<int>(y), w, h);
        }

        void update(AngolGame& game, const std::vector<IRect>& platforms) {
            if (!alive) {
                return;
            }

            double grav = gravity * gravityMult;
            bool moveLeft = game.isDown(VK_LEFT) || game.isDown('A');
            bool moveRight = game.isDown(VK_RIGHT) || game.isDown('D');
            if (controlsReversed) {
                std::swap(moveLeft, moveRight);
            }

            vx = 0;
            if (moveLeft) {
                vx = -speed;
                facing = -1;
            }
            if (moveRight) {
                vx = speed;
                facing = 1;
            }

            bool wantJump = jumpReversed
                ? (game.isDown(VK_DOWN) || game.isDown('S'))
                : (game.isDown(VK_UP) || game.isDown('W') || game.isDown(VK_SPACE));

            if (wantJump && onGround && !jumpBlocked) {
                vy = superGravity ? jumpPower * 1.75 : jumpPower;
                onGround = false;
            }

            vy += grav;
            double maxFall = superGravity ? 14.0 : 18.0;
            if (vy > maxFall) {
                vy = maxFall;
            }

            x += vx;
            IRect current = bodyRect();
            for (const IRect& p : platforms) {
                if (current.intersects(p)) {
                    if (vx > 0) {
                        x = p.x - w;
                    } else if (vx < 0) {
                        x = p.right();
                    }
                    current = bodyRect();
                }
            }

            bool wasInAir = !onGround;
            y += vy;
            onGround = false;
            current = bodyRect();
            for (const IRect& p : platforms) {
                if (current.intersects(p)) {
                    if (vy > 0) {
                        y = p.y - h;
                        if (wasInAir && vy > 4) {
                            squish = 8;
                            game.spawnParticles(x + w / 2.0, y + h, 6,
                                {PLAT_COL, RGB(120, 90, 20), RGB(90, 65, 12)},
                                -3, 3, -3, -0.5);
                        }
                        vy = 0;
                        onGround = true;
                    } else if (vy < 0) {
                        y = p.bottom();
                        vy = 0;
                    }
                    current = bodyRect();
                }
            }

            if (y > HEIGHT + 200) {
                alive = false;
            }
            if (squish > 0) {
                squish--;
            }

            double targetStretch = (!onGround && std::abs(vy) > 3) ? 3 : 0;
            stretch += (targetStretch - stretch) * 0.4;
            if (onGround && std::abs(vx) > 0.1) {
                walk += 0.30;
            } else {
                walk = 0;
            }
        }

        void draw(HDC hdc) const {
            if (!alive) {
                return;
            }
            int ix = static_cast<int>(x);
            int iy = static_cast<int>(y);
            bool walking = onGround && std::abs(vx) > 0.1;

            drawBlockyRunner(hdc, ix + w / 2, iy + h, PLAYER_DRAW_BLOCK, walk, walking, !onGround, facing);
        }

        void kill(AngolGame& game) {
            if (alive) {
                alive = false;
                game.spawnParticles(x + w / 2.0, y + h / 2.0, 25, {PLAYER_BODY, DARK_GRAY, RED, GRAY});
            }
        }
    };

    HWND hwnd = nullptr;
    ULONG_PTR gdiplusToken = 0;
    std::array<bool, 256> keys{};
    std::mt19937 rng{std::random_device{}()};
    std::vector<Particle> particles;
    std::unique_ptr<Image> winImage;
    bool triedImageLoad = false;
    bool highResolutionTimer = false;
    LARGE_INTEGER perfFrequency{};
    LARGE_INTEGER lastFrameCounter{};
    double frameAccumulator = 0.0;

    HFONT fontSm = nullptr;
    HFONT fontMd = nullptr;
    HFONT fontLg = nullptr;
    HFONT fontXl = nullptr;
    HFONT fontXxl = nullptr;
    HFONT fontBubble = nullptr;

    State state = State::Menu;
    int curLevel = 0;
    Level level;
    Player player;
    int deathTimer = 0;
    int winTimer = 0;
    int frame = 0;
    int shakeX = 0;
    int shakeY = 0;
    int deaths = 0;
    std::string trollMsg;
    int trollTimer = 0;
    int playFrames = 0;
    int finalTime = -1;
    int waitFrames = 0;

    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        AngolGame* game = nullptr;
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
            game = static_cast<AngolGame*>(create->lpCreateParams);
            SetWindowLongPtrA(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(game));
            game->hwnd = window;
        } else {
            game = reinterpret_cast<AngolGame*>(GetWindowLongPtrA(window, GWLP_USERDATA));
        }

        if (game) {
            return game->handleMessage(message, wParam, lParam);
        }
        return DefWindowProcA(window, message, wParam, lParam);
    }

    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
            case WM_CREATE:
                timeBeginPeriod(1);
                highResolutionTimer = true;
                QueryPerformanceFrequency(&perfFrequency);
                QueryPerformanceCounter(&lastFrameCounter);
                SetTimer(hwnd, 1, 1, nullptr);
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_TIMER:
                runTimedUpdates();
                return 0;
            case WM_KEYDOWN:
                processKeyDown(static_cast<int>(wParam));
                return 0;
            case WM_KEYUP:
                if (wParam < keys.size()) {
                    keys[wParam] = false;
                }
                return 0;
            case WM_PAINT:
                paint();
                return 0;
            case WM_DESTROY:
                KillTimer(hwnd, 1);
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProcA(hwnd, message, wParam, lParam);
        }
    }

    void cleanup() {
        if (fontSm) DeleteObject(fontSm);
        if (fontMd) DeleteObject(fontMd);
        if (fontLg) DeleteObject(fontLg);
        if (fontXl) DeleteObject(fontXl);
        if (fontXxl) DeleteObject(fontXxl);
        if (fontBubble) DeleteObject(fontBubble);
        if (highResolutionTimer) {
            timeEndPeriod(1);
            highResolutionTimer = false;
        }
        winImage.reset();
        if (gdiplusToken) {
            GdiplusShutdown(gdiplusToken);
            gdiplusToken = 0;
        }
    }

    bool isDown(int key) const {
        return key >= 0 && key < static_cast<int>(keys.size()) && keys[key];
    }

    void runTimedUpdates() {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        double elapsed = static_cast<double>(now.QuadPart - lastFrameCounter.QuadPart) /
            static_cast<double>(perfFrequency.QuadPart);
        lastFrameCounter = now;

        elapsed = std::min(elapsed, 0.10);
        frameAccumulator += elapsed;

        constexpr double fixedStep = 1.0 / FPS;
        int steps = 0;
        while (frameAccumulator >= fixedStep && steps < 5) {
            tick();
            frameAccumulator -= fixedStep;
            steps++;
        }
        if (steps == 5) {
            frameAccumulator = 0.0;
        }

        if (steps > 0) {
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_NOERASE | RDW_UPDATENOW);
        }
    }

    void processKeyDown(int key) {
        bool wasDown = key >= 0 && key < static_cast<int>(keys.size()) && keys[key];
        if (key >= 0 && key < static_cast<int>(keys.size())) {
            keys[key] = true;
        }
        if (wasDown) {
            return;
        }

        if (key == VK_ESCAPE) {
            if (state == State::Menu || state == State::End) {
                DestroyWindow(hwnd);
            } else {
                state = State::Menu;
            }
        }

        if (state == State::Menu) {
            if (key == VK_RETURN || key == VK_SPACE) {
                state = State::Play;
                curLevel = 0;
                deaths = 0;
                playFrames = 0;
                finalTime = -1;
                loadLevel(0);
            }
        } else if (state == State::Play) {
            if (key == 'R') {
                deaths++;
                loadLevel(curLevel);
            }
        } else if (state == State::End) {
            if (key == 'R') {
                state = State::Menu;
            }
        } else if (state == State::Image) {
            if (key == VK_RETURN || key == VK_SPACE) {
                state = State::Menu;
            }
        }
    }

    void tick() {
        frame++;

        if (state == State::Play || state == State::Dying || state == State::WinLevel) {
            playFrames++;
        }

        if (state == State::Play) {
            updatePlay();
        } else if (state == State::Dying) {
            updateDying();
        } else if (state == State::WinLevel) {
            updateWinLevel();
        } else if (state == State::End && frame % 4 == 0) {
            for (int i = 0; i < 2; i++) {
                spawnParticles(randInt(100, WIDTH - 100), -10, 1,
                    {PLAT_COL, YELLOW, WHITE, ORANGE}, -2, 2, 2, 5);
            }
        }

        for (auto it = particles.begin(); it != particles.end();) {
            it->update();
            if (it->life <= 0) {
                it = particles.erase(it);
            } else {
                ++it;
            }
        }
    }

    void updatePlay() {
        std::vector<IRect> solids = level.platforms;
        solids.insert(solids.end(), level.walls.begin(), level.walls.end());
        player.update(*this, solids);

        if (level.finalWait) {
            bool pressing = isDown(VK_LEFT) || isDown(VK_RIGHT) ||
                isDown('A') || isDown('D') || isDown(VK_UP) || isDown('W') ||
                isDown(VK_SPACE) || isDown(VK_DOWN) || isDown('S');
            bool still = !pressing && std::abs(player.vx) < 0.1 && std::abs(player.vy) < 1.0;
            waitFrames = still ? waitFrames + 1 : 0;
            if (waitFrames >= 10 * FPS) {
                spawnParticles(player.x + player.w / 2.0, player.y + player.h / 2.0, 30,
                    {PLAYER_BODY, DARK_GRAY, RED, GRAY});
                finalTime = playFrames;
                state = State::Image;
                frame = 0;
                shakeX = 0;
                shakeY = 0;
            }
        }

        std::vector<IRect> killRects = updateTraps();

        if (player.alive) {
            IRect pr = player.bodyRect();

            if (level.hasWinSpike && pr.intersects(level.winSpikeRect)) {
                state = State::Image;
                frame = 0;
                shakeX = 0;
                shakeY = 0;
                finalTime = playFrames;
                spawnParticles(level.winSpikeRect.centerX(), level.winSpikeRect.centerY(), 30,
                    {RED, WHITE, PLAYER_BODY, GRAY});
            }

            if (state == State::Play) {
                for (const IRect& kr : killRects) {
                    if (pr.intersects(kr)) {
                        player.kill(*this);
                        break;
                    }
                }
            }

            if (state == State::Play && player.alive) {
                for (const IRect& dp : level.deathPlats) {
                    if (pr.intersects(dp)) {
                        player.kill(*this);
                        break;
                    }
                }
            }

            if (state == State::Play && player.alive) {
                for (const Spike& sp : level.spikes) {
                    if (pr.intersects(rect(sp.x, sp.y, sp.w, sp.h))) {
                        player.kill(*this);
                        break;
                    }
                }
            }

            if (player.alive && !level.hasWinSpike && !level.finalWait) {
                if (pr.intersects(level.doorTop)) {
                    enterDoor("top");
                } else if (pr.intersects(level.doorBottom)) {
                    enterDoor("bottom");
                }
            }
        }

        if (!player.alive && state == State::Play) {
            state = State::Dying;
            deathTimer = 0;
            deaths++;
            shakeX = randInt(-5, 5);
            shakeY = randInt(-3, 3);
        }

        if (trollTimer > 0) {
            trollTimer--;
        }
    }

    void enterDoor(const std::string& side) {
        IRect door = side == "top" ? level.doorTop : level.doorBottom;
        if (level.correctSide == side) {
            state = State::WinLevel;
            winTimer = 0;
            spawnParticles(door.centerX(), door.centerY(), 30, {GREEN, CYAN, WHITE, YELLOW});
        } else {
            player.kill(*this);
            trollMsg = "WRONG DOOR!";
            trollTimer = 40;
        }
    }

    void updateDying() {
        deathTimer++;
        if (deathTimer < 12) {
            shakeX = randInt(-4, 4);
            shakeY = randInt(-3, 3);
        } else {
            shakeX = 0;
            shakeY = 0;
        }
        if (deathTimer > 55) {
            state = State::Play;
            loadLevel(curLevel);
        }
    }

    void updateWinLevel() {
        winTimer++;
        if (winTimer > 45) {
            curLevel++;
            if (curLevel >= static_cast<int>(std::size(LEVEL_DATA))) {
                state = State::End;
                frame = 0;
            } else {
                state = State::Play;
                loadLevel(curLevel);
            }
        }
    }

    std::vector<IRect> updateTraps() {
        std::vector<IRect> killRects;
        double px = player.x + player.w / 2.0;
        double py = player.y + player.h / 2.0;

        for (Trap& trap : level.traps) {
            if (trap.type == "vanish_floor") {
                if (!trap.active && triggered(trap, px)) {
                    trap.active = true;
                    trap.timer = 0;
                }
                if (trap.active && !trap.gone) {
                    trap.timer++;
                    if (trap.timer < trap.delay) {
                        trap.shake = randInt(-2, 2);
                    } else {
                        trap.gone = true;
                        trap.shake = 0;
                        removePlatformSection(trap.rect);
                    }
                }
            } else if (trap.type == "fall_block") {
                if (!trap.active && triggered(trap, px)) {
                    trap.active = true;
                    trap.speed = 0;
                }
                if (trap.active && !trap.landed) {
                    trap.speed += 0.6;
                    trap.y += trap.speed;
                    IRect br = rect(trap.rect.x, static_cast<int>(trap.y), trap.rect.w, trap.rect.h);
                    killRects.push_back(br);
                    if (trap.y > FLOOR_Y - trap.rect.h) {
                        trap.y = FLOOR_Y - trap.rect.h;
                        trap.landed = true;
                        level.platforms.push_back(rect(trap.rect.x, static_cast<int>(trap.y), trap.rect.w, trap.rect.h));
                    }
                }
            } else if (trap.type == "slide_popup_spikes") {
                if (!trap.active && triggered(trap, px)) {
                    trap.active = true;
                    trap.progress = 0;
                }
                if (trap.active) {
                    trap.progress = std::min(1.0, trap.progress + 0.08);
                    double firstSpikeX = trap.positions.empty() ? 0 : trap.positions.front().x;
                    if (px < firstSpikeX + 30 && !trap.sliding) {
                        trap.sliding = true;
                    }
                    if (trap.sliding) {
                        for (SpikePos& pos : trap.positions) {
                            pos.x -= 4.5;
                        }
                    }
                    if (trap.progress > 0.4) {
                        for (const SpikePos& pos : trap.positions) {
                            int h = static_cast<int>(pos.h * trap.progress);
                            killRects.push_back(rect(static_cast<int>(pos.x), FLOOR_Y - h, 20, h));
                        }
                    }
                }
            } else if (trap.type == "popup_spikes") {
                if (!trap.active && triggered(trap, px)) {
                    trap.active = true;
                    trap.progress = 0;
                }
                if (trap.active) {
                    trap.progress = std::min(1.0, trap.progress + 0.06);
                    if (trap.progress > 0.4) {
                        for (const SpikePos& pos : trap.positions) {
                            int h = static_cast<int>(pos.h * trap.progress);
                            killRects.push_back(rect(static_cast<int>(pos.x), FLOOR_Y - h, 20, h));
                        }
                    }
                }
            } else if (trap.type == "edge_spikes") {
                if (!trap.active && triggered(trap, px)) {
                    trap.active = true;
                    trap.progress = 0;
                }
                if (trap.active) {
                    trap.progress = std::min(1.0, trap.progress + 0.05);
                    if (trap.progress > 0.3) {
                        for (const SpikePos& pos : trap.positions) {
                            int h = static_cast<int>(pos.h * trap.progress);
                            killRects.push_back(rect(static_cast<int>(pos.x), FLOOR_Y - h, 18, h));
                        }
                    }
                }
            } else if (trap.type == "ceiling_spikes_fall") {
                if (!trap.active && triggered(trap, px)) {
                    trap.active = true;
                    trap.speed = 0;
                    trap.y = -40;
                }
                if (trap.active) {
                    trap.speed += 0.5;
                    trap.y += trap.speed;
                    for (const SpikePos& pos : trap.positions) {
                        killRects.push_back(rect(static_cast<int>(pos.x), static_cast<int>(trap.y), 20, pos.h));
                    }
                    if (trap.y > HEIGHT) {
                        trap.y = HEIGHT;
                    }
                }
            } else if (trap.type == "invisible_wall") {
                if (!trap.visible && triggered(trap, px)) {
                    trap.visible = true;
                    level.platforms.push_back(trap.rect);
                }
            } else if (trap.type == "escaping_door") {
                IRect& door = trap.side == "top" ? level.doorTop : level.doorBottom;
                double dx = std::abs(px - door.centerX());
                double dy = std::abs(py - door.centerY());
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist < trap.triggerDist && !trap.escaped) {
                    trap.escaped = true;
                }
                if (trap.escaped) {
                    door.y += static_cast<int>(trap.escapeY);
                    trap.escapeY -= 0.3;
                    if (door.bottom() < -100) {
                        door.y = -500;
                    }
                }
            } else if (trap.type == "spawn_correct_door") {
                if (!trap.active && triggered(trap, px)) {
                    trap.active = true;
                    IRect& door = level.correctSide == "top" ? level.doorTop : level.doorBottom;
                    door.y = trap.targetY;
                    level.platforms.push_back(trap.platformRect);
                }
            } else if (trap.type == "screen_wrap_left") {
                if (px < trap.triggerX) {
                    player.x = trap.x;
                    player.y = trap.y;
                    player.vy = 0;
                }
            } else if (trap.type == "fake_door") {
                const IRect& door = trap.side == "top" ? level.doorTop : level.doorBottom;
                if (player.bodyRect().intersects(door)) {
                    player.kill(*this);
                }
            } else if (trap.type == "moving_spikes") {
                if (!trap.active && triggered(trap, px)) {
                    trap.active = true;
                }
                if (trap.active) {
                    trap.x += trap.speed * trap.direction;
                    if (trap.x < 10 || trap.x + trap.w > WIDTH - 10) {
                        trap.direction *= -1;
                    }
                    killRects.push_back(rect(static_cast<int>(trap.x), static_cast<int>(trap.y), trap.w, trap.h));
                }
            } else if (trap.type == "closing_walls") {
                if (!trap.active && triggered(trap, px)) {
                    trap.active = true;
                }
                if (trap.active) {
                    double gap = trap.rightX - trap.leftX;
                    if (gap > trap.minGap) {
                        trap.leftX += trap.speed;
                        trap.rightX -= trap.speed;
                    }
                    killRects.push_back(rect(static_cast<int>(trap.leftX), trap.wallY, trap.wallW, trap.wallH));
                    killRects.push_back(rect(static_cast<int>(trap.rightX), trap.wallY, trap.wallW, trap.wallH));
                }
            } else if (trap.type == "reverse_controls") {
                if (!trap.active && triggered(trap, px)) {
                    trap.active = true;
                    player.controlsReversed = true;
                }
            } else if (trap.type == "block_jump") {
                if (!trap.active && triggered(trap, px)) {
                    trap.active = true;
                    player.jumpBlocked = true;
                }
            } else if (trap.type == "spike_pit") {
                if (!trap.active && triggered(trap, px)) {
                    trap.active = true;
                }
                if (trap.active) {
                    trap.openProgress = std::min(1.0, trap.openProgress + 0.03);
                    if (!trap.origSet) {
                        trap.origLx = trap.leftRect.x;
                        trap.origRx = trap.rightRect.x;
                        trap.origSet = true;
                    }
                    int shift = static_cast<int>(40 * trap.openProgress);
                    trap.leftRect.x = trap.origLx - shift;
                    trap.rightRect.x = trap.origRx + shift;
                    if (trap.openProgress > 0.5) {
                        for (int sx = trap.pitX - shift; sx < trap.pitX + trap.pitW + shift; sx += 20) {
                            killRects.push_back(rect(sx, FLOOR_Y + 20, 18, 18));
                        }
                    }
                }
            }
        }

        return killRects;
    }

    bool triggered(const Trap& trap, double px) const {
        return trap.dir == ">" ? px > trap.triggerX : px < trap.triggerX;
    }

    void removePlatformSection(const IRect& cut) {
        std::vector<IRect> next;
        for (const IRect& platform : level.platforms) {
            if (platform.intersects(cut) && platform.y == cut.y && platform.h == cut.h) {
                if (cut.x > platform.x) {
                    next.push_back(rect(platform.x, platform.y, cut.x - platform.x, platform.h));
                }
                if (cut.right() < platform.right()) {
                    next.push_back(rect(cut.right(), platform.y, platform.right() - cut.right(), platform.h));
                }
            } else {
                next.push_back(platform);
            }
        }
        level.platforms = std::move(next);
    }

    void loadLevel(int idx) {
        particles.clear();
        trollMsg.clear();
        trollTimer = 0;
        waitFrames = 0;
        level = buildLevel(idx);
        player = Player{};
        player.x = level.playerStartX;
        player.y = level.playerStartY;
        player.controlsReversed = level.controlsReversed;
        player.jumpBlocked = level.jumpBlocked;
        player.jumpReversed = level.jumpReversed;
        player.superGravity = level.superGravity;
        player.gravityMult = level.superGravity ? 1.35 : level.gravityMult;
    }

    Level buildLevel(int idx) {
        const LevelText& data = LEVEL_DATA[idx];
        bool correctTop = randInt(0, 1) == 1;
        Level lv;
        lv.correctSide = correctTop ? "top" : "bottom";
        lv.topAnswer = correctTop ? data.correct : data.wrong;
        lv.bottomAnswer = correctTop ? data.wrong : data.correct;
        lv.correctWord = data.correct;
        lv.sentence = data.sentence;
        lv.playerStartX = 60;
        lv.playerStartY = FLOOR_Y - 40;
        lv.doorTop = rect(900, FLOOR_Y - 140, 30, 40);
        lv.doorBottom = rect(900, FLOOR_Y - 40, 30, 40);
        IRect doorPlatform = rect(870, FLOOR_Y - 80, 90, 18);

        lv.walls.push_back(rect(-20, 0, 20, HEIGHT));
        lv.walls.push_back(rect(WIDTH, 0, 20, HEIGHT));
        if (idx != 2) {
            lv.walls.push_back(rect(0, -20, WIDTH, 20));
        }

        switch (idx) {
            case 0:
                lv.platforms.push_back(makeFloor(0, WIDTH));
                lv.traps.push_back(Trap("fall_block").trigger(350, ">").withRect(rect(340, -60, 60, 50)).withY(-60));
                lv.traps.push_back(Trap("popup_spikes").trigger(450, ">").withPositions({{480, 20}, {510, 20}, {540, 20}}));
                break;
            case 1:
                lv.platforms.push_back(makeFloor(0, 380));
                lv.platforms.push_back(makeFloor(520, 480));
                lv.traps.push_back(Trap("fall_block").trigger(390, ">").withRect(rect(440, -60, 70, 50)).withY(-60));
                lv.traps.push_back(Trap("edge_spikes").trigger(520, ">").withPositions({{520, 20}, {550, 20}, {580, 20}}));
                break;
            case 2:
                lv.platforms.push_back(makeFloor(0, 300));
                lv.platforms.push_back(makeFloor(750, 250));
                lv.ghostPlats.push_back(rect(350, FLOOR_Y, 180, FLOOR_H));
                lv.ghostPlats.push_back(rect(570, FLOOR_Y, 140, FLOOR_H));
                lv.platforms.push_back(rect(230, FLOOR_Y - 90, 100, 18));
                lv.platforms.push_back(rect(400, FLOOR_Y - 170, 100, 18));
                lv.platforms.push_back(rect(570, FLOOR_Y - 130, 100, 18));
                lv.platforms.push_back(rect(720, FLOOR_Y - 80, 80, 18));
                lv.traps.push_back(Trap("fall_block").trigger(350, ">").withRect(rect(450, -500, 200, 500)).withY(-500));
                lv.traps.push_back(Trap("invisible_wall").trigger(320, ">").withRect(rect(300, FLOOR_Y + 40, 450, 60)));
                break;
            case 3: {
                lv.platforms.push_back(makeFloor(0, WIDTH));
                std::string wrongSide = correctTop ? "bottom" : "top";
                IRect wrongRect = rect(900, FLOOR_Y - 40, 30, 40);
                IRect correctRect = rect(50, -500, 30, 40);
                if (correctTop) {
                    lv.doorTop = correctRect;
                    lv.doorBottom = wrongRect;
                } else {
                    lv.doorBottom = correctRect;
                    lv.doorTop = wrongRect;
                }
                IRect hiddenPlatform = rect(20, FLOOR_Y - 60, 90, 18);
                doorPlatform = rect(0, -100, 0, 0);
                lv.traps.push_back(Trap("escaping_door").withSide(wrongSide).withTriggerDist(150).withEscapeY(-8));
                lv.traps.push_back(Trap("spawn_correct_door").trigger(850, ">").withPlatform(hiddenPlatform).withTargetY(FLOOR_Y - 100));
                lv.traps.push_back(Trap("moving_spikes").trigger(0, ">").withXY(100, FLOOR_Y - 25).withSize(25, 25).withSpeed(3).withActive(true).withDirection(1));
                lv.traps.push_back(Trap("slide_popup_spikes").trigger(400, "<").withPositions({{350, 20}, {320, 20}, {290, 20}}));
                break;
            }
            case 4:
                lv.platforms.push_back(makeFloor(0, WIDTH));
                lv.platforms.push_back(rect(250, FLOOR_Y - 130, 80, 18));
                lv.platforms.push_back(rect(450, FLOOR_Y - 130, 80, 18));
                lv.controlsReversed = true;
                lv.traps.push_back(Trap("fall_block").trigger(250, ">").withRect(rect(280, -60, 50, 45)).withY(-60));
                lv.traps.push_back(Trap("fall_block").trigger(420, ">").withRect(rect(460, -60, 50, 45)).withY(-60));
                lv.traps.push_back(Trap("fall_block").trigger(600, ">").withRect(rect(640, -60, 70, 50)).withY(-60));
                lv.traps.push_back(Trap("popup_spikes").trigger(700, ">").withPositions({{720, 20}, {750, 20}, {780, 20}, {810, 20}}));
                break;
            case 5:
                lv.platforms.push_back(makeFloor(0, WIDTH));
                lv.spikes.push_back({360, FLOOR_Y - 30, 30, 30, "up"});
                lv.traps.push_back(Trap("popup_spikes").trigger(470, ">").withPositions({{560, 26}, {590, 26}}));
                lv.traps.push_back(Trap("fall_block").trigger(640, ">").withRect(rect(690, -60, 60, 50)).withY(-60));
                break;
            case 6:
                lv.platforms.push_back(makeFloor(0, WIDTH));
                lv.jumpReversed = true;
                lv.platforms.push_back(rect(200, FLOOR_Y - 30, 80, 30));
                lv.platforms.push_back(rect(350, FLOOR_Y - 60, 80, 60));
                lv.platforms.push_back(rect(500, FLOOR_Y - 30, 80, 30));
                lv.traps.push_back(Trap("moving_spikes").trigger(0, ">").withXY(300, FLOOR_Y - 25).withSize(25, 25).withSpeed(2.5).withActive(true).withDirection(1));
                lv.traps.push_back(Trap("moving_spikes").trigger(0, ">").withXY(600, FLOOR_Y - 25).withSize(25, 25).withSpeed(3).withActive(true).withDirection(-1));
                lv.traps.push_back(Trap("spike_pit").trigger(620, ">").withPit(rect(620, FLOOR_Y, 80, FLOOR_H), rect(740, FLOOR_Y, 80, FLOOR_H), 700, 40));
                break;
            case 7:
                // LEVEL 8: "Super gravity". Every jump rockets you ~250px upward,
                // straight into the row of hanging spikes -- so the way through is
                // to WALK underneath them and never jump beneath the spikes. The
                // floor is solid up to a single gap that sits in a spike-free zone,
                // cleared with one safe super-jump. The whole right side is left
                // clear so the (widened) platform for the top door is reachable.
                lv.platforms.push_back(makeFloor(0, 600));
                lv.platforms.push_back(makeFloor(700, 300));
                lv.superGravity = true;
                for (int sx = 150; sx < 460; sx += 50) {
                    lv.spikes.push_back({sx, 285, 25, 50, "down"});
                }
                doorPlatform = rect(800, FLOOR_Y - 80, 170, 18);
                break;
            case 8:
                lv.platforms.push_back(makeFloor(0, WIDTH));
                lv.platforms.push_back(rect(300, FLOOR_Y - 150, 100, 18));
                lv.platforms.push_back(rect(550, FLOOR_Y - 220, 100, 18));
                lv.traps.push_back(Trap("closing_walls").trigger(400, ">").withClosingWalls(350, 760, 22, 85, FLOOR_Y - 85, 1.6, 60));
                lv.traps.push_back(Trap("escaping_door").withSide(correctTop ? "bottom" : "top").withTriggerDist(120).withEscapeY(-4));
                lv.traps.push_back(Trap("fall_block").trigger(300, ">").withRect(rect(330, -60, 50, 45)).withY(-60));
                break;
            case 9:
                lv.platforms.push_back(makeFloor(0, 200));
                lv.platforms.push_back(makeFloor(800, 200));
                lv.ghostPlats.push_back(rect(280, FLOOR_Y, 120, FLOOR_H));
                lv.ghostPlats.push_back(rect(450, FLOOR_Y, 120, FLOOR_H));
                lv.platforms.push_back(rect(620, FLOOR_Y, 120, FLOOR_H));
                lv.platforms.push_back(rect(250, FLOOR_Y - 95, 85, 16));
                lv.platforms.push_back(rect(400, FLOOR_Y - 150, 85, 16));
                lv.platforms.push_back(rect(550, FLOOR_Y - 110, 85, 16));
                lv.deathPlats.push_back(rect(700, FLOOR_Y - 95, 85, 16));
                lv.walls.push_back(rect(740, FLOOR_Y, 60, FLOOR_H));
                lv.traps.push_back(Trap("ceiling_spikes_fall").trigger(400, ">").withPositions({{390, 25}, {420, 25}, {450, 25}}).withY(-40));
                lv.traps.push_back(Trap("moving_spikes").trigger(0, ">").withXY(300, FLOOR_Y - 25).withSize(25, 25).withSpeed(4).withActive(true).withDirection(1));
                break;
            case 10:
                lv.platforms.push_back(makeFloor(0, WIDTH));
                lv.platforms.push_back(rect(350, FLOOR_Y - 160, 90, 18));
                lv.platforms.push_back(rect(550, FLOOR_Y - 100, 90, 18));
                lv.traps.push_back(Trap("reverse_controls").trigger(500, ">"));
                lv.traps.push_back(Trap("popup_spikes").trigger(250, ">").withPositions(series(270, 5, 30, 22)));
                lv.traps.push_back(Trap("popup_spikes").trigger(600, ">").withPositions(series(630, 6, 30, 22)));
                lv.traps.push_back(Trap("fall_block").trigger(450, ">").withRect(rect(470, -60, 60, 45)).withY(-60));
                break;
            case 11:
                lv.platforms.push_back(makeFloor(0, WIDTH));
                lv.platforms.push_back(rect(400, FLOOR_Y - 160, 100, 18));
                lv.platforms.push_back(rect(650, FLOOR_Y - 240, 100, 18));
                lv.traps.push_back(Trap("invisible_wall").trigger(280, ">").withRect(rect(380, FLOOR_Y - 80, 18, 80)));
                lv.traps.push_back(Trap("invisible_wall").trigger(540, ">").withRect(rect(600, FLOOR_Y - 80, 18, 80)));
                lv.traps.push_back(Trap("invisible_wall").trigger(700, ">").withRect(rect(740, FLOOR_Y - 80, 18, 80)));
                lv.traps.push_back(Trap("popup_spikes").trigger(90, ">").withPositions({{180, 20}, {210, 20}}));
                lv.traps.push_back(Trap("fall_block").trigger(800, ">").withRect(rect(830, -60, 55, 45)).withY(-60));
                break;
            case 12:
                lv.platforms.push_back(makeFloor(0, 300));
                lv.platforms.push_back(makeFloor(700, 300));
                lv.ghostPlats.push_back(rect(380, FLOOR_Y, 120, FLOOR_H));
                lv.ghostPlats.push_back(rect(540, FLOOR_Y, 120, FLOOR_H));
                lv.platforms.push_back(rect(340, FLOOR_Y - 95, 85, 16));
                lv.platforms.push_back(rect(470, FLOOR_Y - 150, 85, 16));
                lv.platforms.push_back(rect(600, FLOOR_Y - 110, 85, 16));
                lv.traps.push_back(Trap("moving_spikes").trigger(0, ">").withXY(800, FLOOR_Y - 25).withSize(25, 25).withSpeed(3).withActive(true).withDirection(-1));
                lv.traps.push_back(Trap("popup_spikes").trigger(770, ">").withPositions({{810, 20}, {840, 20}}));
                lv.traps.push_back(Trap("popup_spikes").trigger(60, ">").withPositions({{150, 20}, {180, 20}}));
                break;
            case 13:
                lv.platforms.push_back(makeFloor(0, WIDTH));
                lv.platforms.push_back(rect(250, FLOOR_Y - 130, 90, 18));
                lv.platforms.push_back(rect(500, FLOOR_Y - 200, 90, 18));
                lv.traps.push_back(Trap("reverse_controls").trigger(400, ">"));
                lv.traps.push_back(Trap("escaping_door").withSide(correctTop ? "bottom" : "top").withTriggerDist(130).withEscapeY(-4.5));
                lv.traps.push_back(Trap("fall_block").trigger(300, ">").withRect(rect(320, -60, 55, 45)).withY(-60));
                lv.traps.push_back(Trap("fall_block").trigger(600, ">").withRect(rect(650, -60, 65, 50)).withY(-60));
                lv.traps.push_back(Trap("popup_spikes").trigger(500, ">").withPositions({{520, 22}, {550, 22}, {580, 22}}));
                lv.traps.push_back(Trap("moving_spikes").trigger(0, ">").withXY(700, FLOOR_Y - 25).withSize(25, 25).withSpeed(2.5).withActive(true).withDirection(-1));
                lv.traps.push_back(Trap("popup_spikes").trigger(90, ">").withPositions({{180, 20}, {210, 20}}));
                break;
            case 14: {
                lv.platforms.push_back(makeFloor(0, WIDTH));
                lv.finalWait = true;
                int cx = WIDTH / 2;
                IRect box = rect(cx - 95, FLOOR_Y - 305, 190, 150);
                int bt = 18;
                lv.platforms.push_back(rect(box.x, box.y, box.w, bt));
                lv.platforms.push_back(rect(box.x, box.bottom() - bt, box.w, bt));
                lv.platforms.push_back(rect(box.x, box.y, bt, box.h));
                lv.platforms.push_back(rect(box.right() - bt, box.y, bt, box.h));
                lv.doorTop = rect(cx - 15, box.y + 24, 30, 40);
                lv.doorBottom = rect(cx - 15, box.y + 78, 30, 40);
                doorPlatform = rect(0, -100, 0, 0);
                break;
            }
            default:
                break;
        }

        lv.platforms.push_back(doorPlatform);
        return lv;
    }

    static std::vector<SpikePos> series(int startX, int count, int step, int h) {
        std::vector<SpikePos> values;
        for (int i = 0; i < count; i++) {
            values.push_back({static_cast<double>(startX + i * step), h});
        }
        return values;
    }

    void paint() {
        PAINTSTRUCT ps{};
        HDC windowDc = BeginPaint(hwnd, &ps);

        HDC renderDc = CreateCompatibleDC(windowDc);
        HBITMAP renderBitmap = CreateCompatibleBitmap(windowDc, WIDTH, HEIGHT);
        HBITMAP oldRenderBitmap = static_cast<HBITMAP>(SelectObject(renderDc, renderBitmap));

        fillRect(renderDc, rect(0, 0, WIDTH, HEIGHT), BG);

        if (state == State::Menu) {
            drawMenu(renderDc);
        } else if (state == State::Play || state == State::Dying || state == State::WinLevel) {
            drawGameplay(renderDc);
        } else if (state == State::End) {
            drawEndScreen(renderDc);
            for (const Particle& p : particles) {
                p.draw(renderDc);
            }
        } else if (state == State::Image) {
            drawImageScreen(renderDc);
        }

        HDC finalDc = CreateCompatibleDC(windowDc);
        HBITMAP finalBitmap = CreateCompatibleBitmap(windowDc, WIDTH, HEIGHT);
        HBITMAP oldFinalBitmap = static_cast<HBITMAP>(SelectObject(finalDc, finalBitmap));
        fillRect(finalDc, rect(0, 0, WIDTH, HEIGHT), BG);
        BitBlt(finalDc, shakeX, shakeY, WIDTH, HEIGHT, renderDc, 0, 0, SRCCOPY);
        BitBlt(windowDc, 0, 0, WIDTH, HEIGHT, finalDc, 0, 0, SRCCOPY);

        SelectObject(finalDc, oldFinalBitmap);
        DeleteObject(finalBitmap);
        DeleteDC(finalDc);

        SelectObject(renderDc, oldRenderBitmap);
        DeleteObject(renderBitmap);
        DeleteDC(renderDc);
        EndPaint(hwnd, &ps);
    }

    void drawGameplay(HDC hdc) {
        fillRect(hdc, rect(0, 0, WIDTH, HEIGHT), BG);

        for (const IRect& gp : level.ghostPlats) {
            drawPlatform(hdc, gp);
        }
        for (const IRect& p : level.platforms) {
            drawPlatform(hdc, p);
        }
        for (const IRect& dp : level.deathPlats) {
            drawPlatform(hdc, dp);
        }
        for (const Spike& sp : level.spikes) {
            drawStaticSpike(hdc, sp);
        }

        drawTraps(hdc);
        drawDoor(hdc, level.doorTop, level.topAnswer);
        drawDoor(hdc, level.doorBottom, level.bottomAnswer);

        if (state != State::Dying || deathTimer < 4) {
            player.draw(hdc);
        }
        for (const Particle& p : particles) {
            p.draw(hdc);
        }

        if (!level.finalWait) {
            drawSentenceBar(hdc);
        }

        if (level.finalWait && waitFrames > 0) {
            fillEllipse(hdc, rect(12, 12, 14, 14), GREEN);
            outlineEllipse(hdc, rect(12, 12, 14, 14), BLACK);
        }

        if (level.controlsReversed && frame % 120 < 60) {
            drawTopCentered(hdc, "! CONTROLS REVERSED !", fontSm, RED, WIDTH / 2, 56);
        }
        if (level.jumpBlocked) {
            drawTopCentered(hdc, "! JUMP DISABLED !", fontSm, RED, WIDTH / 2, 56);
        }
        if (level.jumpReversed) {
            drawTopCentered(hdc, "! PRESS DOWN TO JUMP !", fontSm, RED, WIDTH / 2, 56);
        }
        if (level.superGravity) {
            drawTopCentered(hdc, "! SUPER GRAVITY !", fontSm, RED, WIDTH / 2, 56);
        }

        for (const Trap& trap : level.traps) {
            if (trap.type == "reverse_controls" && trap.active && frame % 100 < 50) {
                drawTopCentered(hdc, "! CONTROLS REVERSED !", fontSm, RED, WIDTH / 2, 56);
            }
            if (trap.type == "block_jump" && trap.active) {
                drawTopCentered(hdc, "! JUMP BLOCKED !", fontSm, RED, WIDTH / 2, 56);
            }
        }

        if (trollTimer > 0) {
            drawTrollWarning(hdc, trollMsg);
        }
        if (state == State::Dying) {
            drawDeathOverlay(hdc);
        } else if (state == State::WinLevel) {
            drawWinOverlay(hdc);
        }
    }

    void drawTraps(HDC hdc) {
        for (const Trap& trap : level.traps) {
            if (trap.type == "vanish_floor") {
                if (trap.active && !trap.gone) {
                    IRect rr = trap.rect;
                    rr.x += trap.shake;
                    drawPlatform(hdc, rr);
                    int cx = rr.centerX() + randInt(-20, 20);
                    drawLine(hdc, cx, rr.y, cx + randInt(-15, 15), rr.y + 15, RED, 2);
                }
            } else if (trap.type == "fall_block") {
                if (trap.active) {
                    IRect br = rect(trap.rect.x, static_cast<int>(trap.y), trap.rect.w, trap.rect.h);
                    fillRect(hdc, br, BLACK);
                    outlineRect(hdc, br, RED);
                    if (!trap.landed && trap.y < FLOOR_Y / 2.0) {
                        int cx = trap.rect.x + trap.rect.w / 2;
                        for (int dy = 0; dy < static_cast<int>(trap.y); dy += 12) {
                            if ((dy / 6) % 2 == 0) {
                                drawLine(hdc, cx, dy, cx, std::min(dy + 6, static_cast<int>(trap.y)), RED);
                            }
                        }
                    }
                }
            } else if (trap.type == "popup_spikes" || trap.type == "edge_spikes" || trap.type == "slide_popup_spikes") {
                if (trap.active) {
                    int spikeW = trap.type == "edge_spikes" ? 18 : 20;
                    for (const SpikePos& pos : trap.positions) {
                        int visH = static_cast<int>(pos.h * trap.progress);
                        if (visH > 2) {
                            drawSpikeUp(hdc, static_cast<int>(pos.x), FLOOR_Y - visH, spikeW, visH);
                        }
                    }
                }
            } else if (trap.type == "ceiling_spikes_fall") {
                if (trap.active && trap.y < HEIGHT) {
                    for (const SpikePos& pos : trap.positions) {
                        drawSpikeDown(hdc, static_cast<int>(pos.x), static_cast<int>(trap.y), 20, pos.h);
                    }
                }
            } else if (trap.type == "invisible_wall") {
                if (trap.visible) {
                    fillRect(hdc, trap.rect, BLACK);
                    for (int gy = trap.rect.y; gy < trap.rect.bottom(); gy += 6) {
                        if (randInt(0, 99) < 30) {
                            int ox = randInt(-3, 3);
                            drawLine(hdc, trap.rect.x + ox, gy, trap.rect.right() + ox, gy, RED);
                        }
                    }
                    drawTopCentered(hdc, "!", fontLg, RED, trap.rect.centerX(), trap.rect.y - 30);
                }
            } else if (trap.type == "moving_spikes") {
                if (trap.active) {
                    drawSpikeUp(hdc, static_cast<int>(trap.x), static_cast<int>(trap.y), trap.w, trap.h);
                }
            } else if (trap.type == "closing_walls") {
                if (trap.active) {
                    int lx = static_cast<int>(trap.leftX);
                    int rx = static_cast<int>(trap.rightX);
                    fillRect(hdc, rect(lx, trap.wallY, trap.wallW, trap.wallH), BLACK);
                    fillRect(hdc, rect(rx, trap.wallY, trap.wallW, trap.wallH), BLACK);
                    for (int sy = trap.wallY + 15; sy < trap.wallY + trap.wallH - 15; sy += 30) {
                        drawSpikeRight(hdc, lx + trap.wallW - 5, sy, 15, 20);
                        drawSpikeLeft(hdc, rx - 10, sy, 15, 20);
                    }
                }
            } else if (trap.type == "spike_pit") {
                if (trap.active && trap.openProgress > 0.5) {
                    int shift = static_cast<int>(40 * trap.openProgress);
                    for (int sx = trap.pitX - shift; sx < trap.pitX + trap.pitW + shift; sx += 20) {
                        drawSpikeUp(hdc, sx, FLOOR_Y + 20, 18, 18);
                    }
                }
            }
        }
    }

    void drawMenu(HDC hdc) {
        fillRect(hdc, rect(0, 0, WIDTH, HEIGHT), BG);
        int ty = static_cast<int>(130 + std::sin(frame * 0.025) * 6);
        drawTopCentered(hdc, "ANGOL", fontXxl, WHITE, WIDTH / 2, ty);
        drawTopCentered(hdc, "RAGE   PLATFORMER", fontLg, PLAT_COL, WIDTH / 2, ty + 65);
        drawLine(hdc, WIDTH / 2 - 150, ty + 110, WIDTH / 2 + 150, ty + 110, PLAT_COL, 2);

        drawTopCentered(hdc, "A Level-Devil inspired educational game.", fontSm, RGB(220, 200, 160), WIDTH / 2, ty + 130);
        drawTopCentered(hdc, "Reach the correct door... if you can.", fontSm, RGB(220, 200, 160), WIDTH / 2, ty + 156);
        drawTopCentered(hdc, "The game WILL trick you. Trust nothing.", fontSm, RGB(220, 200, 160), WIDTH / 2, ty + 182);

        int by = ty + 230;
        fillRect(hdc, rect(WIDTH / 2 - 180, by, 360, 130), PLAT_COL);
        drawTopCentered(hdc, "CONTROLS", fontMd, BLACK, WIDTH / 2, by + 10);
        drawTopCentered(hdc, "Arrows / WASD  --  Move & Jump", fontSm, RGB(60, 40, 5), WIDTH / 2, by + 40);
        drawTopCentered(hdc, "Space  --  Jump", fontSm, RGB(60, 40, 5), WIDTH / 2, by + 62);
        drawTopCentered(hdc, "R  --  Restart Level", fontSm, RGB(60, 40, 5), WIDTH / 2, by + 84);
        drawTopCentered(hdc, "ESC  --  Quit", fontSm, RGB(60, 40, 5), WIDTH / 2, by + 106);

        if (frame > 30 && std::abs(std::sin(frame * 0.06)) > 0.25) {
            drawTopCentered(hdc, "Press ENTER or SPACE to start", fontLg, WHITE, WIDTH / 2, HEIGHT - 85);
        }

        drawBlockyRunner(hdc, WIDTH / 2, HEIGHT - 100, PLAYER_DRAW_BLOCK, frame * 0.18, true, false, 1);
    }

    void drawEndScreen(HDC hdc) {
        fillRect(hdc, rect(0, 0, WIDTH, HEIGHT), BG);
        int ty = static_cast<int>(100 + std::sin(frame * 0.03) * 8);
        drawTopCentered(hdc, "CONGRATULATIONS!", fontXxl, WHITE, WIDTH / 2, ty);
        drawTopCentered(hdc, "You completed all 15 levels!", fontLg, PLAT_COL, WIDTH / 2, ty + 75);
        drawTopCentered(hdc, "You are a true ANGOL champion!", fontMd, RGB(220, 200, 160), WIDTH / 2, ty + 120);

        int cx = WIDTH / 2;
        int cy = ty + 230;
        fillRect(hdc, rect(cx - 35, cy - 25, 70, 50), YELLOW);
        fillRect(hdc, rect(cx - 30, cy - 20, 60, 40), RGB(200, 170, 30));
        fillRect(hdc, rect(cx - 12, cy + 25, 24, 8), YELLOW);
        fillRect(hdc, rect(cx - 20, cy + 33, 40, 7), YELLOW);

        if (frame > 40 && std::abs(std::sin(frame * 0.06)) > 0.3) {
            drawTopCentered(hdc, "Press R to restart  |  Press ESC to quit", fontMd, RGB(220, 200, 160), WIDTH / 2, HEIGHT - 80);
        }
    }

    void drawImageScreen(HDC hdc) {
        fillRect(hdc, rect(0, 0, WIDTH, HEIGHT), BLACK);
        Image* image = getWinImage();
        if (image) {
            double scale = std::min(WIDTH / static_cast<double>(image->GetWidth()), (HEIGHT - 90) / static_cast<double>(image->GetHeight()));
            int nw = std::max(1, static_cast<int>(image->GetWidth() * scale));
            int nh = std::max(1, static_cast<int>(image->GetHeight() * scale));
            Graphics graphics(hdc);
            graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            graphics.DrawImage(image, (WIDTH - nw) / 2, (HEIGHT - nh) / 2 - 20, nw, nh);
        } else {
            drawTopCentered(hdc, "THE END", fontXxl, WHITE, WIDTH / 2, HEIGHT / 2 - 30);
        }

        if (finalTime >= 0) {
            std::string label = "YOUR TIME:  " + fmtTime(finalTime);
            SIZE size = textSize(hdc, label, fontXl);
            int bw = size.cx + 28;
            int bh = size.cy + 12;
            int bx = (WIDTH - bw) / 2;
            int by = 14;
            alphaFillRect(hdc, bx, by, bw, bh, 0, 0, 0, 160);
            drawText(hdc, label, fontXl, WHITE, bx + 14, by + 6);
        }

        if (frame % 60 < 40) {
            drawTopCentered(hdc, "Press ENTER or ESC to return to menu", fontSm, RGB(220, 200, 160), WIDTH / 2, HEIGHT - 30);
        }
    }

    void drawPlatform(HDC hdc, const IRect& r) {
        fillRect(hdc, r, PLAT_COL);
    }

    void drawDoor(HDC hdc, const IRect& door, const std::string& answer) {
        fillRect(hdc, rect(door.x, door.y + 10, door.w, door.h - 10), LIGHT_GRAY);
        fillEllipse(hdc, rect(door.x, door.y, door.w, 20), LIGHT_GRAY);
        outlineRect(hdc, rect(door.x, door.y + 10, door.w, door.h - 10), BLACK);
        outlineEllipse(hdc, rect(door.x, door.y, door.w, 20), BLACK);
        fillRect(hdc, rect(door.x + 1, door.y + 10, door.w - 2, 6), LIGHT_GRAY);

        SIZE size = textSize(hdc, answer, fontBubble);
        int bw = size.cx + 10;
        int bh = size.cy + 6;
        int bx = door.centerX() - bw / 2;
        int by = door.y - bh - 20;
        fillRect(hdc, rect(bx, by, bw, bh), WHITE);
        outlineRect(hdc, rect(bx, by, bw, bh), BLACK);
        drawText(hdc, answer, fontBubble, BLACK, bx + 5, by + 3);
    }

    void drawSentenceBar(HDC hdc) {
        drawText(hdc, "LEVEL " + std::to_string(curLevel + 1) + "/15", fontSm, BLACK, 12, 12);
        drawText(hdc, "TIME  " + fmtTime(playFrames), fontSm, BLACK, 12, 30);
        if (deaths > 0) {
            std::string text = "Deaths: " + std::to_string(deaths);
            SIZE size = textSize(hdc, text, fontSm);
            drawText(hdc, text, fontSm, RED, WIDTH - size.cx - 12, 12);
        }
        drawFitTopCentered(hdc, level.sentence, 26, BLACK, WIDTH / 2, 50, WIDTH - 20);
    }

    void drawDeathOverlay(HDC hdc) {
        if (deathTimer < 5) {
            return;
        }
        int alpha = std::min(140, deathTimer * 6);
        alphaFillRect(hdc, 0, 0, WIDTH, HEIGHT, 210, 40, 40, alpha);
        if (deathTimer > 10) {
            std::string msg = DEATH_MESSAGES[(deathTimer / 25) % std::size(DEATH_MESSAGES)];
            drawTopCentered(hdc, msg, fontXl, WHITE, WIDTH / 2, HEIGHT / 2 - 30);
            drawTopCentered(hdc, "Restarting...", fontSm, RGB(240, 200, 200), WIDTH / 2, HEIGHT / 2 + 30);
        }
    }

    void drawWinOverlay(HDC hdc) {
        if (winTimer < 5) {
            return;
        }
        int alpha = std::min(100, winTimer * 5);
        alphaFillRect(hdc, 0, 0, WIDTH, HEIGHT, 50, 180, 70, alpha);
        drawTopCentered(hdc, "CORRECT!", fontXl, WHITE, WIDTH / 2, HEIGHT / 2 - 25);
    }

    void drawTrollWarning(HDC hdc, const std::string& text) {
        if ((frame / 8) % 2 == 0) {
            drawTopCentered(hdc, text, fontLg, WHITE, WIDTH / 2, HEIGHT / 2 - 80);
        }
    }

    void drawStaticSpike(HDC hdc, const Spike& sp) {
        if (sp.dir == "up") {
            drawSpikeUp(hdc, sp.x, sp.y, sp.w, sp.h);
        } else if (sp.dir == "down") {
            drawSpikeDown(hdc, sp.x, sp.y, sp.w, sp.h);
        } else if (sp.dir == "left") {
            drawSpikeLeft(hdc, sp.x, sp.y, sp.w, sp.h);
        } else if (sp.dir == "right") {
            drawSpikeRight(hdc, sp.x, sp.y, sp.w, sp.h);
        }
    }

    void drawSpikeUp(HDC hdc, int x, int y, int w, int h) {
        POINT points[3] = {{x, y + h}, {x + w / 2, y}, {x + w, y + h}};
        fillPolygon(hdc, points, 3, SPIKE_COL);
    }

    void drawSpikeDown(HDC hdc, int x, int y, int w, int h) {
        POINT points[3] = {{x, y}, {x + w / 2, y + h}, {x + w, y}};
        fillPolygon(hdc, points, 3, SPIKE_COL);
    }

    void drawSpikeLeft(HDC hdc, int x, int y, int w, int h) {
        POINT points[3] = {{x + w, y}, {x, y + h / 2}, {x + w, y + h}};
        fillPolygon(hdc, points, 3, SPIKE_COL);
    }

    void drawSpikeRight(HDC hdc, int x, int y, int w, int h) {
        POINT points[3] = {{x, y}, {x + w, y + h / 2}, {x, y + h}};
        fillPolygon(hdc, points, 3, SPIKE_COL);
    }

    Image* getWinImage() {
        if (triedImageLoad) {
            return winImage.get();
        }
        triedImageLoad = true;

        std::vector<std::wstring> candidates = {
            executableDir() + L"\\1520056982297.jfif",
            L"1520056982297.jfif",
            L"..\\1520056982297.jfif",
            L"..\\..\\1520056982297.jfif"
        };

        for (const std::wstring& path : candidates) {
            auto image = std::make_unique<Image>(path.c_str());
            if (image && image->GetLastStatus() == Ok) {
                winImage = std::move(image);
                break;
            }
        }
        return winImage.get();
    }

    std::string fmtTime(int frames) const {
        double total = std::max(0, frames) / static_cast<double>(FPS);
        int minutes = static_cast<int>(total / 60);
        int seconds = static_cast<int>(total) % 60;
        int tenths = static_cast<int>(total * 10) % 10;
        char buffer[32]{};
        wsprintfA(buffer, "%d:%02d.%d", minutes, seconds, tenths);
        return buffer;
    }

    void spawnParticles(double x, double y, int count, std::initializer_list<COLORREF> colors,
                        double minVx = -5, double maxVx = 5, double minVy = -8, double maxVy = 1) {
        std::vector<COLORREF> palette(colors);
        for (int i = 0; i < count; i++) {
            Particle p;
            p.x = x;
            p.y = y;
            p.vx = randDouble(minVx, maxVx);
            p.vy = randDouble(minVy, maxVy);
            p.color = palette[static_cast<std::size_t>(randInt(0, static_cast<int>(palette.size()) - 1))];
            p.life = randInt(15, 40);
            p.maxLife = p.life;
            p.size = randInt(3, 8);
            particles.push_back(p);
        }
    }

    int randInt(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }

    double randDouble(double min, double max) {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(rng);
    }
};

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand) {
    AngolGame game;
    return game.run(instance, showCommand);
}
