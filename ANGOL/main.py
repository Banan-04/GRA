"""
ANGOL RAGE PLATFORMER - Retro Minimalist Style
Mustard-brown & peach aesthetic with maximum trolling.
Every level is designed to betray you.

Controls: Arrow Keys / WASD to move, Up/W/Space to jump
"""

import pygame
import sys
import asyncio
import math
import random
import copy

# 
# INIT
# 
pygame.init()
WIDTH, HEIGHT = 1000, 700
FPS = 60
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("ANGOL - Rage Platformer")
clock = pygame.time.Clock()

# 
# RETRO MINIMALIST COLOR PALETTE
# 
BG          = (145, 105, 15)
PLAT_COL    = (255, 180, 80)
WALL_COL    = (145, 105, 15)
SPIKE_COL   = (180, 30, 30)
PLAYER_BODY = (18, 18, 18)      # black blocky runner (like the reference image)
PLAYER_LINE = (40, 30, 10)      # (kept for compatibility)
PLAYER_EYE  = (25, 20, 10)      # (kept for compatibility)
DOOR_COL    = (192, 192, 192)
DOOR_BORDER = (0, 0, 0)
BUBBLE_BG   = (255, 255, 255)
BUBBLE_BORDER= (0, 0, 0)
TEXT_COL    = (0, 0, 0)
SENT_BG     = (145, 105, 15)
SENT_TEXT   = (0, 0, 0)
RED         = (180, 30, 30)
GREEN       = (50, 160, 60)
WHITE       = (255, 255, 255)
BLACK       = (0, 0, 0)
GRAY        = (160, 160, 160)
LIGHT_GRAY  = (192, 192, 192)
DARK_GRAY   = (50, 50, 50)
YELLOW      = (240, 200, 40)
ORANGE      = (240, 130, 40)
GHOST_COL   = (220, 165, 60)
CYAN        = (60, 200, 220)

# 
# FONTS
# 
font_sm   = pygame.font.SysFont("arial", 16, bold=True)
font_md   = pygame.font.SysFont("arial", 22, bold=True)
font_lg   = pygame.font.SysFont("arial", 26, bold=True)
font_xl   = pygame.font.SysFont("arial", 42, bold=True)
font_xxl  = pygame.font.SysFont("arial", 56, bold=True)
font_bub  = pygame.font.SysFont("arial", 14, bold=True)

# 
# LEVEL SENTENCES
# 
LEVEL_DATA = [
    {"sentence": 'Spot moves on four legs, which means it is a ___ .',
     "correct": "quadruped", "wrong": "humanoidal"},
    {"sentence": 'Deep mines and burning buildings are examples of a ___ environment.',
     "correct": "hazardous", "wrong": "autonomous"},
    {"sentence": 'It uses ___ navigation to move from point A to point B.',
     "correct": "autonomous", "wrong": "obstacle"},
    {"sentence": 'If a robot is about to hit a wall, it uses ___ avoidance.',
     "correct": "obstacle", "wrong": "hazard"},
    {"sentence": "In reinforcement learning, the robot's brain learns by ___ and error.",
     "correct": "trial", "wrong": "twin"},
    {"sentence": "The extra equipment carried on the robot's back is called a ___ .",
     "correct": "payload", "wrong": "scanner"},
    {"sentence": 'Architects use Spot to create a perfect 3D digital ___ of a building.',
     "correct": "twin", "wrong": "trial"},
    {"sentence": 'Atlas has arms, legs, and a body like a person, so it is a ___ robot.',
     "correct": "humanoidal", "wrong": "quadruped"},
    {"sentence": 'Boston Dynamics builds advanced machines for mobile ___ .',
     "correct": "robotics", "wrong": "mechanics"},
    {"sentence": 'Spot is very useful for specific industrial tasks, often called ___ cases.',
     "correct": "use", "wrong": "work"},
    {"sentence": 'To create 3D maps, Spot uses special laser technology called ___ .',
     "correct": "LiDAR", "wrong": "Sonar"},
    {"sentence": 'Thanks to artificial ___ , the robot can learn to walk over difficult terrain.',
     "correct": "intelligence", "wrong": "network"},
    {"sentence": 'It can use cameras for thermal ___ to find machines getting too hot.',
     "correct": "inspection", "wrong": "detection"},
    {"sentence": 'It uses acoustic sensors for gas leak ___ .',
     "correct": "detection", "wrong": "inspection"},
    {"sentence": 'The robots of today are not science ___ -- they are real machines!',
     "correct": "fiction", "wrong": "reality"},
]

# 
# PARTICLES
# 
class Particle:
    def __init__(self, x, y, vx, vy, color, life, size):
        self.x, self.y = x, y
        self.vx, self.vy = vx, vy
        self.color = color
        self.life = life
        self.max_life = life
        self.size = size
    def update(self):
        self.x += self.vx; self.y += self.vy
        self.vy += 0.2
        self.life -= 1
    def draw(self, surf):
        a = max(0, self.life / self.max_life)
        s = max(1, int(self.size * a))
        pygame.draw.rect(surf, self.color,
                         (int(self.x)-s//2, int(self.y)-s//2, s, s))

particles = []

def spawn_particles(x, y, count, colors, vx_range=(-5,5), vy_range=(-8,1)):
    for _ in range(count):
        particles.append(Particle(
            x, y,
            random.uniform(*vx_range), random.uniform(*vy_range),
            random.choice(colors),
            random.randint(15, 40), random.randint(3, 8)
        ))

# 
# PLAYER  (Level Devil style: black rectangle with big white eyes)
# 
class Player:
    def __init__(self, x, y):
        self.w, self.h = 15, 25
        self.x, self.y = float(x), float(y)
        self.vx, self.vy = 0.0, 0.0
        self.speed = 4.5
        self.jump_power = -11.0
        self.gravity = 0.55
        self.on_ground = False
        self.alive = True
        self.facing = 1  # 1=right, -1=left
        self.squish = 0  # squish animation on land
        self.stretch = 0  # vertical stretch while airborne
        self.walk = 0.0  # leg-animation phase
        # Troll modifiers (per-level)
        self.controls_reversed = False
        self.jump_blocked = False
        self.jump_reversed = False
        self.super_gravity = False
        self.gravity_mult = 1.0

    @property
    def rect(self):
        return pygame.Rect(int(self.x), int(self.y), self.w, self.h)

    def update(self, platforms, keys):
        if not self.alive:
            return

        grav = self.gravity * self.gravity_mult

        #  Horizontal 
        move_left = keys[pygame.K_LEFT] or keys[pygame.K_a]
        move_right = keys[pygame.K_RIGHT] or keys[pygame.K_d]

        if self.controls_reversed:
            move_left, move_right = move_right, move_left

        self.vx = 0
        if move_left:
            self.vx = -self.speed
            self.facing = -1
        if move_right:
            self.vx = self.speed
            self.facing = 1

        #  Jump 
        if self.jump_reversed:
            want_jump = keys[pygame.K_DOWN] or keys[pygame.K_s]
        else:
            want_jump = keys[pygame.K_UP] or keys[pygame.K_w] or keys[pygame.K_SPACE]
            
        if want_jump and self.on_ground and not self.jump_blocked:
            if self.super_gravity:
                self.vy = self.jump_power * 2.2  # SUPER LAUNCH
            else:
                self.vy = self.jump_power
            self.on_ground = False

        #  Gravity 
        self.vy += grav
        if self.vy > 18:
            self.vy = 18

        #  Move X 
        self.x += self.vx
        # clamp to screen usunite na rzecz cian
        pass

        r = self.rect
        for p in platforms:
            if r.colliderect(p):
                if self.vx > 0:
                    self.x = p.left - self.w
                elif self.vx < 0:
                    self.x = p.right
                r = self.rect

        #  Move Y 
        was_in_air = not self.on_ground
        self.y += self.vy
        self.on_ground = False
        r = self.rect
        for p in platforms:
            if r.colliderect(p):
                if self.vy > 0:
                    self.y = p.top - self.h
                    if was_in_air and self.vy > 4:
                        self.squish = 8
                        # Landing dust puff
                        spawn_particles(self.x + self.w/2, self.y + self.h,
                                        6, [PLAT_COL, (120, 90, 20), (90, 65, 12)],
                                        vx_range=(-3, 3), vy_range=(-3, -0.5))
                    self.vy = 0
                    self.on_ground = True
                elif self.vy < 0:
                    self.y = p.bottom
                    self.vy = 0
                r = self.rect

        # fall off screen
        if self.y > HEIGHT + 200:
            self.alive = False

        # squish timer
        if self.squish > 0:
            self.squish -= 1

        # airborne vertical stretch (eases back to 0 on ground)
        target_stretch = 3 if (not self.on_ground and abs(self.vy) > 3) else 0
        self.stretch += (target_stretch - self.stretch) * 0.4

        # leg-animation phase (advances only while walking on the ground)
        if self.on_ground and abs(self.vx) > 0.1:
            self.walk += 0.30
        else:
            self.walk = 0.0

    def draw(self, surf):
        if not self.alive:
            return

        col = PLAYER_BODY
        x, y, w, h = int(self.x), int(self.y), self.w, self.h
        walking = self.on_ground and abs(self.vx) > 0.1

        # Soft contact shadow on the ground
        if self.on_ground:
            sh_w = int(w * 1.2)
            shadow = pygame.Surface((sh_w, 6), pygame.SRCALPHA)
            pygame.draw.ellipse(shadow, (0, 0, 0, 70), (0, 0, sh_w, 6))
            surf.blit(shadow, (int(x + w/2 - sh_w/2), y + h - 2))

        #  Body: solid chunky block down to the hips 
        hip_y = y + h - 8
        pygame.draw.rect(surf, col, (x, y, w, hip_y - y))

        #  Legs: two thick segments hip->foot (clean scissor walk, no gaps) 
        hipL = (x + 4, hip_y)
        hipR = (x + w - 4, hip_y)
        s = math.sin(self.walk)
        if walking:
            footL = (x + 4 + int(s * 4), y + h)
            footR = (x + w - 4 - int(s * 4), y + h)
        elif not self.on_ground:
            footL = (x + 3, y + h - 1)          # slight tuck while airborne
            footR = (x + w - 3, y + h - 1)
        else:
            footL = (x + 4, y + h)              # stand straight
            footR = (x + w - 4, y + h)
        pygame.draw.line(surf, col, hipL, footL, 5)
        pygame.draw.line(surf, col, hipR, footR, 5)

    def kill(self):
        if self.alive:
            self.alive = False
            spawn_particles(self.x + self.w//2, self.y + self.h//2,
                            25, [PLAYER_BODY, DARK_GRAY, RED, GRAY])


# 
# SPIKE DRAWING
# 
def draw_spike_up(surf, x, y, w, h):
    """Triangle pointing up (kills from above)."""
    pts = [(x, y+h), (x+w//2, y), (x+w, y+h)]
    pygame.draw.polygon(surf, SPIKE_COL, pts)

def draw_spike_down(surf, x, y, w, h):
    """Triangle pointing down (ceiling spike)."""
    pts = [(x, y), (x+w//2, y+h), (x+w, y)]
    pygame.draw.polygon(surf, SPIKE_COL, pts)

def draw_spike_left(surf, x, y, w, h):
    pts = [(x+w, y), (x, y+h//2), (x+w, y+h)]
    pygame.draw.polygon(surf, SPIKE_COL, pts)

def draw_spike_right(surf, x, y, w, h):
    pts = [(x, y), (x+w, y+h//2), (x, y+h)]
    pygame.draw.polygon(surf, SPIKE_COL, pts)


# 
# LEVEL BUILDER  each level now has MANY traps
# 
FLOOR_Y = 600
FLOOR_H = 100

def make_floor(x, w):
    return pygame.Rect(x, FLOOR_Y, w, FLOOR_H)

def build_level(idx):
    data = LEVEL_DATA[idx]
    # Randomize which door (top/bottom) is correct
    if random.random() < 0.5:
        correct_side = "top"
        top_ans, bottom_ans = data["correct"], data["wrong"]
    else:
        correct_side = "bottom"
        top_ans, bottom_ans = data["wrong"], data["correct"]

    # defaults
    platforms = []     # solid rects
    ghost_plats = []   # fake/ghost platforms (player falls through)
    spikes = []        # list of {rect, dir} permanently visible
    traps = []         # scripted traps
    walls = []         # boundary walls

    player_start = (60, FLOOR_Y - 40)
    # Doors stacked vertically (one above the other)
    door_x = 900
    door_top    = pygame.Rect(door_x, FLOOR_Y - 140, 30, 40)
    door_bottom = pygame.Rect(door_x, FLOOR_Y - 40, 30, 40)
    # Platform under the top door
    door_platform = pygame.Rect(door_x - 30, FLOOR_Y - 80, 90, 18)

    # Player modifiers per level
    controls_reversed = False
    jump_blocked = False
    jump_reversed = False
    super_gravity = False
    gravity_mult = 1.0

    # Special: a spike that finishes the game instead of killing (final level)
    win_spike_rect = None
    # Platforms that LOOK normal but kill on touch (Level-Devil trap)
    death_plats = []
    # Final level: freeze the player, then kill it after 10 s (ends the game)
    final_wait = False

    # Left and right walls (Level Devil has enclosed rooms)
    walls.append(pygame.Rect(-20, 0, 20, HEIGHT))     # left wall
    walls.append(pygame.Rect(WIDTH, 0, 20, HEIGHT))    # right wall
    if idx != 2:
        walls.append(pygame.Rect(0, -20, WIDTH, 20))       # ceiling

    # 
    # LEVEL DESIGNS
    # 

    if idx == 0:
        # LEVEL 1: "Welcome to hell" - looks easy, floor vanishes near doors
        platforms = [make_floor(0, WIDTH)]
        traps = [
            # Floor vanishes when approaching doors
            {"type": "vanish_floor", "trigger_x": 620, "dir": ">",
             "rect": pygame.Rect(620, FLOOR_Y, 380, FLOOR_H),
             "delay": 12, "timer": 0, "active": False, "gone": False, "shake": 0},
            # Ceiling block surprise mid-walk
            {"type": "fall_block", "trigger_x": 350, "dir": ">",
             "rect": pygame.Rect(340, -60, 60, 50), "y": -60.0,
             "speed": 0.0, "active": False, "landed": False},
            # Hidden spikes pop up at 500
            {"type": "popup_spikes", "trigger_x": 450, "dir": ">",
             "positions": [(480, 20), (510, 20), (540, 20)],
             "progress": 0.0, "active": False},
        ]

    elif idx == 1:
        # LEVEL 2: Gap with falling ceiling + edge spikes appear on landing
        platforms = [make_floor(0, 380), make_floor(520, 480)]
        traps = [
            # Ceiling block when jumping the gap
            {"type": "fall_block", "trigger_x": 390, "dir": ">",
             "rect": pygame.Rect(440, -60, 70, 50), "y": -60.0,
             "speed": 0.0, "active": False, "landed": False},
            # Edge spikes appear where you land
            {"type": "edge_spikes", "trigger_x": 520, "dir": ">",
             "positions": [(520, 20), (550, 20), (580, 20)],
             "progress": 0.0, "active": False},
            # Floor vanishes under right door area
            {"type": "vanish_floor", "trigger_x": 800, "dir": ">",
             "rect": pygame.Rect(800, FLOOR_Y, 200, FLOOR_H),
             "delay": 10, "timer": 0, "active": False, "gone": False, "shake": 0},
        ]

    elif idx == 2:
        # LEVEL 3: Ghost platforms trick + real upper path
        platforms = [make_floor(0, 300), make_floor(750, 250)]
        # Ghost platforms (look identical to real, but player falls through!)
        ghost_plats = [
            pygame.Rect(350, FLOOR_Y, 180, FLOOR_H),
            pygame.Rect(570, FLOOR_Y, 140, FLOOR_H),
        ]
        # Real upper platforms (decoy path)
        platforms += [
            pygame.Rect(230, FLOOR_Y - 90, 100, 18),
            pygame.Rect(400, FLOOR_Y - 170, 100, 18),
            pygame.Rect(570, FLOOR_Y - 130, 100, 18),
            pygame.Rect(720, FLOOR_Y - 80, 80, 18),
        ]
        traps = [
            {"type": "fall_block", "trigger_x": 350, "dir": ">",
             "rect": pygame.Rect(450, -500, 200, 500), "y": -500.0,
             "speed": 0.0, "active": False, "landed": False},
            # Secret pit floor that appears when falling into the pit
            {"type": "invisible_wall", "trigger_x": 320, "dir": ">",
             "rect": pygame.Rect(300, FLOOR_Y + 40, 450, 60), "visible": False},
        ]

    elif idx == 3:
        # LEVEL 4: Wrong door escapes on right, Correct door spawns on left
        platforms = [make_floor(0, WIDTH)]
        wrong_side = "bottom" if correct_side == "top" else "top"
        
        wrong_rect = pygame.Rect(900, FLOOR_Y - 40, 30, 40)
        correct_rect = pygame.Rect(50, -500, 30, 40)
        target_y = FLOOR_Y - 100  # where it will spawn
        
        if correct_side == "top":
            door_top = correct_rect
            door_bottom = wrong_rect
        else:
            door_bottom = correct_rect
            door_top = wrong_rect
            
        hidden_platform = pygame.Rect(20, FLOOR_Y - 60, 90, 18)
        door_platform = pygame.Rect(0, -100, 0, 0)
        
        traps = [
            {"type": "escaping_door", "side": wrong_side,
             "trigger_dist": 150, "escaped": False, "escape_y": -8.0},
            {"type": "spawn_correct_door", "trigger_x": 850, "dir": ">",
             "active": False, "platform_rect": hidden_platform, "target_y": target_y},
            # Ruchomy kolec wraca na mape
            {"type": "moving_spikes", "x": 100.0, "y": FLOOR_Y - 25,
             "w": 25, "h": 25, "speed": 3.0,
             "trigger_x": 0, "dir": ">", "active": True, "direction": 1},
            # Przesuwajace sie kolce pod postacia
            {"type": "slide_popup_spikes", "trigger_x": 400, "dir": "<",
             "positions": [[350, 20], [320, 20], [290, 20]],
             "progress": 0.0, "active": False},
        ]

    elif idx == 4:
        # LEVEL 5: Reversed controls + cascade ceiling blocks
        platforms = [make_floor(0, WIDTH)]
        platforms += [
            pygame.Rect(250, FLOOR_Y - 130, 80, 18),
            pygame.Rect(450, FLOOR_Y - 130, 80, 18),
        ]
        controls_reversed = True  # TROLL: reversed controls!
        traps = [
            # Triple ceiling blocks
            {"type": "fall_block", "trigger_x": 250, "dir": ">",
             "rect": pygame.Rect(280, -60, 50, 45), "y": -60.0,
             "speed": 0.0, "active": False, "landed": False},
            {"type": "fall_block", "trigger_x": 420, "dir": ">",
             "rect": pygame.Rect(460, -60, 50, 45), "y": -60.0,
             "speed": 0.0, "active": False, "landed": False},
            {"type": "fall_block", "trigger_x": 600, "dir": ">",
             "rect": pygame.Rect(640, -60, 70, 50), "y": -60.0,
             "speed": 0.0, "active": False, "landed": False},
            # Popup spikes near end
            {"type": "popup_spikes", "trigger_x": 700, "dir": ">",
             "positions": [(720, 20), (750, 20), (780, 20), (810, 20)],
             "progress": 0.0, "active": False},
        ]

    elif idx == 5:
        # LEVEL 6: Jump the ground spike, then dodge a falling block + popup spikes
        platforms = [make_floor(0, WIDTH)]
        # Ground spike you must JUMP over (this is the "kolec" that used to block you)
        spikes.append({"x": 360, "y": FLOOR_Y - 30, "w": 30, "h": 30, "dir": "up"})
        traps = [
            # A second ground spike a bit further (also jumpable)
            {"type": "popup_spikes", "trigger_x": 470, "dir": ">",
             "positions": [(560, 26), (590, 26)],
             "progress": 0.0, "active": False},
            # Falling ceiling block after the spike (added challenge)
            {"type": "fall_block", "trigger_x": 640, "dir": ">",
             "rect": pygame.Rect(690, -60, 60, 50), "y": -60.0,
             "speed": 0.0, "active": False, "landed": False},
            # Floor stays intact under the doors so it is always beatable
        ]

    elif idx == 6:
        # LEVEL 7: Jump reversed! Up does nothing, Down jumps.
        platforms = [make_floor(0, WIDTH)]
        jump_reversed = True  # TROLL: Press DOWN to jump
        
        platforms += [
            pygame.Rect(200, FLOOR_Y - 30, 80, 30),
            pygame.Rect(350, FLOOR_Y - 60, 80, 60),
            pygame.Rect(500, FLOOR_Y - 30, 80, 30),
        ]
        traps = [
            # Moving spikes patrol the floor
            {"type": "moving_spikes", "x": 300.0, "y": FLOOR_Y - 25,
             "w": 25, "h": 25, "speed": 2.5,
             "trigger_x": 0, "dir": ">", "active": True, "direction": 1},
            {"type": "moving_spikes", "x": 600.0, "y": FLOOR_Y - 25,
             "w": 25, "h": 25, "speed": 3.0,
             "trigger_x": 0, "dir": ">", "active": True, "direction": -1},
            # Floor opens to spikes at 650
            {"type": "spike_pit", "trigger_x": 620, "dir": ">",
             "left_rect": pygame.Rect(620, FLOOR_Y, 80, FLOOR_H),
             "right_rect": pygame.Rect(740, FLOOR_Y, 80, FLOOR_H),
             "pit_x": 700, "pit_w": 40,
             "open_progress": 0.0, "active": False},
        ]

    elif idx == 7:
        # LEVEL 8: Super gravity (trampoline of death) + ceiling spikes
        platforms = [make_floor(0, WIDTH)]
        super_gravity = True  # Jumps launch you to the ceiling!
        # Ceiling spikes
        for sx in range(100, 900, 50):
            spikes.append({"x": sx, "y": 60, "w": 25, "h": 25, "dir": "down"})
        traps = [
            # Fall blocks triggered at different points
            {"type": "fall_block", "trigger_x": 500, "dir": ">",
             "rect": pygame.Rect(520, -60, 60, 50), "y": -60.0,
             "speed": 0.0, "active": False, "landed": False},
            # Popup spikes
            {"type": "popup_spikes", "trigger_x": 700, "dir": ">",
             "positions": [(730, 20), (760, 20)],
             "progress": 0.0, "active": False},
        ]

    elif idx == 8:
        # LEVEL 9: Closing walls + escaping door + ceiling blocks
        platforms = [make_floor(0, WIDTH)]
        platforms += [
            pygame.Rect(300, FLOOR_Y - 150, 100, 18),
            pygame.Rect(550, FLOOR_Y - 220, 100, 18),
        ]
        traps = [
            # Closing walls  short enough to leap over the right one
            {"type": "closing_walls", "trigger_x": 400, "dir": ">",
             "left_x": 350.0, "right_x": 760.0,
             "wall_w": 22, "wall_h": 85, "wall_y": FLOOR_Y - 85,
             "speed": 1.6, "active": False, "min_gap": 60},
            # Escaping door  the WRONG door runs away (correct one stays reachable)
            {"type": "escaping_door", "side": "top" if correct_side == "bottom" else "bottom",
             "trigger_dist": 120, "escaped": False, "escape_y": -4.0},
            # Ceiling block
            {"type": "fall_block", "trigger_x": 300, "dir": ">",
             "rect": pygame.Rect(330, -60, 50, 45), "y": -60.0,
             "speed": 0.0, "active": False, "landed": False},
        ]

    elif idx == 9:
        # LEVEL 10: the obvious TOP-right floating platform KILLS on touch,
        # while the BOTTOM-right platform is solid & safe (the real route).
        platforms = [make_floor(0, 200), make_floor(800, 200)]
        # Ghost (fall-through) platforms
        ghost_plats = [
            pygame.Rect(280, FLOOR_Y, 120, FLOOR_H),
            pygame.Rect(450, FLOOR_Y, 120, FLOOR_H),
        ]
        # Bottom-right platform is now REAL & solid -> passable safe route
        platforms.append(pygame.Rect(620, FLOOR_Y, 120, FLOOR_H))
        # Real floating steps (reachable)
        platforms += [
            pygame.Rect(250, FLOOR_Y - 95, 85, 16),
            pygame.Rect(400, FLOOR_Y - 150, 85, 16),
            pygame.Rect(550, FLOOR_Y - 110, 85, 16),
        ]
        # Top-right floating platform LOOKS normal but KILLS on touch
        death_plats = [pygame.Rect(700, FLOOR_Y - 95, 85, 16)]
        # Invisible (un-drawn) but SOLID floor bridging the gap -> you can cross it
        walls.append(pygame.Rect(740, FLOOR_Y, 60, FLOOR_H))
        traps = [
            # Ceiling spikes fall on upper path
            {"type": "ceiling_spikes_fall", "trigger_x": 400, "dir": ">",
             "positions": [(390, 25), (420, 25), (450, 25)],
             "y": -40.0, "speed": 0.0, "active": False},
            # Moving spikes on ground level
            {"type": "moving_spikes", "x": 300.0, "y": FLOOR_Y - 25,
             "w": 25, "h": 25, "speed": 4.0,
             "trigger_x": 0, "dir": ">", "active": True, "direction": 1},
        ]

    elif idx == 10:
        # LEVEL 11: Controls reverse HALFWAY + massive popup spike field
        platforms = [make_floor(0, WIDTH)]
        platforms += [
            pygame.Rect(350, FLOOR_Y - 160, 90, 18),
            pygame.Rect(550, FLOOR_Y - 100, 90, 18),
        ]
        traps = [
            # Controls reverse at x=500
            {"type": "reverse_controls", "trigger_x": 500, "dir": ">",
             "active": False},
            # Popup spikes in two waves
            {"type": "popup_spikes", "trigger_x": 250, "dir": ">",
             "positions": [(270+i*30, 22) for i in range(5)],
             "progress": 0.0, "active": False},
            {"type": "popup_spikes", "trigger_x": 600, "dir": ">",
             "positions": [(630+i*30, 22) for i in range(6)],
             "progress": 0.0, "active": False},
            # Ceiling block
            {"type": "fall_block", "trigger_x": 450, "dir": ">",
             "rect": pygame.Rect(470, -60, 60, 45), "y": -60.0,
             "speed": 0.0, "active": False, "landed": False},
        ]

    elif idx == 11:
        # LEVEL 12: Invisible walls maze + closing walls + edge spikes
        platforms = [make_floor(0, WIDTH)]
        platforms += [
            pygame.Rect(400, FLOOR_Y - 160, 100, 18),
            pygame.Rect(650, FLOOR_Y - 240, 100, 18),
        ]
        traps = [
            # Multiple invisible walls pop up  short enough to jump over
            {"type": "invisible_wall", "trigger_x": 280, "dir": ">",
             "rect": pygame.Rect(380, FLOOR_Y - 80, 18, 80), "visible": False},
            {"type": "invisible_wall", "trigger_x": 540, "dir": ">",
             "rect": pygame.Rect(600, FLOOR_Y - 80, 18, 80), "visible": False},
            {"type": "invisible_wall", "trigger_x": 700, "dir": ">",
             "rect": pygame.Rect(740, FLOOR_Y - 80, 18, 80), "visible": False},
            # Extra simple popup spikes near the start (early trigger = fair warning)
            {"type": "popup_spikes", "trigger_x": 90, "dir": ">",
             "positions": [(180, 20), (210, 20)],
             "progress": 0.0, "active": False},
            # A falling block guarding the door (becomes a low platform)
            {"type": "fall_block", "trigger_x": 800, "dir": ">",
             "rect": pygame.Rect(830, -60, 55, 45), "y": -60.0,
             "speed": 0.0, "active": False, "landed": False},
        ]

    elif idx == 12:
        # LEVEL 13: Moving spikes + ghost platforms + blocked jump zone
        platforms = [make_floor(0, 300), make_floor(700, 300)]
        ghost_plats = [
            pygame.Rect(380, FLOOR_Y, 120, FLOOR_H),
            pygame.Rect(540, FLOOR_Y, 120, FLOOR_H),
        ]
        platforms += [
            pygame.Rect(340, FLOOR_Y - 95, 85, 16),
            pygame.Rect(470, FLOOR_Y - 150, 85, 16),
            pygame.Rect(600, FLOOR_Y - 110, 85, 16),
        ]
        traps = [
            # One patrolling spike on the right floor (climb start stays clear)
            {"type": "moving_spikes", "x": 800.0, "y": FLOOR_Y - 25,
             "w": 25, "h": 25, "speed": 3.0,
             "trigger_x": 0, "dir": ">", "active": True, "direction": -1},
            # Popup spikes on the right floor, PAST the climb-landing zone (jumpable)
            {"type": "popup_spikes", "trigger_x": 770, "dir": ">",
             "positions": [(810, 20), (840, 20)],
             "progress": 0.0, "active": False},
            # Extra simple popup spikes on the start floor (early trigger = fair warning)
            {"type": "popup_spikes", "trigger_x": 60, "dir": ">",
             "positions": [(150, 20), (180, 20)],
             "progress": 0.0, "active": False},
        ]

    elif idx == 13:
        # LEVEL 14: Escaping door + fake door + reversed controls + ceiling blocks
        platforms = [make_floor(0, WIDTH)]
        platforms += [
            pygame.Rect(250, FLOOR_Y - 130, 90, 18),
            pygame.Rect(500, FLOOR_Y - 200, 90, 18),
        ]
        traps = [
            # Controls reverse at 400
            {"type": "reverse_controls", "trigger_x": 400, "dir": ">",
             "active": False},
            # Escaping wrong door (the correct one stays put)
            {"type": "escaping_door", "side": "top" if correct_side == "bottom" else "bottom",
             "trigger_dist": 130, "escaped": False, "escape_y": -4.5},
            # Two ceiling blocks
            {"type": "fall_block", "trigger_x": 300, "dir": ">",
             "rect": pygame.Rect(320, -60, 55, 45), "y": -60.0,
             "speed": 0.0, "active": False, "landed": False},
            {"type": "fall_block", "trigger_x": 600, "dir": ">",
             "rect": pygame.Rect(650, -60, 65, 50), "y": -60.0,
             "speed": 0.0, "active": False, "landed": False},
            # Popup spikes
            {"type": "popup_spikes", "trigger_x": 500, "dir": ">",
             "positions": [(520, 22), (550, 22), (580, 22)],
             "progress": 0.0, "active": False},
            # Extra: a patrolling spike on the right half (jumpable, away from spawn)
            {"type": "moving_spikes", "x": 700.0, "y": FLOOR_Y - 25,
             "w": 25, "h": 25, "speed": 2.5,
             "trigger_x": 0, "dir": ">", "active": True, "direction": -1},
            # Extra: popup spikes near the start (early trigger = fair warning)
            {"type": "popup_spikes", "trigger_x": 90, "dir": ">",
             "positions": [(180, 20), (210, 20)],
             "progress": 0.0, "active": False},
        ]

    elif idx == 14:
        # LEVEL 15 FINAL: you CAN move. The correct-answer door is sealed in a
        # box in the centre (unreachable). The real trick: DON'T MOVE for 10 s,
        # then you die  and that death ends the game (shows the image).
        platforms = [make_floor(0, WIDTH)]
        final_wait = True
        cx = WIDTH // 2
        box = pygame.Rect(cx - 95, FLOOR_Y - 215, 190, 150)   # visible sealed box
        bt = 18
        platforms += [
            pygame.Rect(box.left, box.top, box.width, bt),            # top
            pygame.Rect(box.left, box.bottom - bt, box.width, bt),    # bottom
            pygame.Rect(box.left, box.top, bt, box.height),           # left
            pygame.Rect(box.right - bt, box.top, bt, box.height),     # right
        ]
        # Doors sealed inside the box (one shows the correct answer)  unreachable
        door_top    = pygame.Rect(cx - 15, box.top + 24, 30, 40)
        door_bottom = pygame.Rect(cx - 15, box.top + 78, 30, 40)
        door_platform = pygame.Rect(0, -100, 0, 0)
        player_start = (60, FLOOR_Y - 40)
        traps = []

    # Add door platform to platforms list
    platforms.append(door_platform)

    return {
        "platforms": platforms,
        "ghost_plats": ghost_plats,
        "spikes": spikes,
        "traps": traps,
        "walls": walls,
        "door_top": door_top,
        "door_bottom": door_bottom,
        "top_answer": top_ans,
        "bottom_answer": bottom_ans,
        "correct_side": correct_side,
        "correct_word": data["correct"],
        "player_start": player_start,
        "win_spike_rect": win_spike_rect,
        "death_plats": death_plats,
        "final_wait": final_wait,
        "sentence": data["sentence"],
        "controls_reversed": controls_reversed,
        "jump_blocked": jump_blocked,
        "jump_reversed": jump_reversed,
        "super_gravity": super_gravity,
        "gravity_mult": gravity_mult,
    }


# 
# TRAP UPDATE
# 
def update_traps(level, player, frame):
    """Process all traps. Returns list of kill-rects."""
    kill_rects = []
    px = player.x + player.w / 2
    py = player.y + player.h / 2

    for trap in level["traps"]:
        # Trigger check
        def triggered():
            tx = trap.get("trigger_x", 0)
            d = trap.get("dir", ">")
            if d == ">":
                return px > tx
            else:
                return px < tx

        t = trap["type"]

        #  VANISHING FLOOR 
        if t == "vanish_floor":
            if not trap["active"] and triggered():
                trap["active"] = True
                trap["timer"] = 0
            if trap["active"] and not trap["gone"]:
                trap["timer"] += 1
                if trap["timer"] < trap["delay"]:
                    trap["shake"] = random.randint(-2, 2)
                else:
                    trap["gone"] = True
                    trap["shake"] = 0
                    r = trap["rect"]
                    if r in level["platforms"]:
                        level["platforms"].remove(r)

        #  FALLING BLOCK 
        elif t == "fall_block":
            if not trap["active"] and triggered():
                trap["active"] = True
                trap["speed"] = 0
            if trap["active"] and not trap.get("landed", False):
                trap["speed"] += 0.6
                trap["y"] += trap["speed"]
                br = pygame.Rect(trap["rect"].x, int(trap["y"]),
                                 trap["rect"].w, trap["rect"].h)
                kill_rects.append(br)
                if trap["y"] > FLOOR_Y - trap["rect"].h:
                    trap["y"] = FLOOR_Y - trap["rect"].h
                    trap["landed"] = True
                    # Add as platform
                    landed_rect = pygame.Rect(trap["rect"].x, int(trap["y"]),
                                              trap["rect"].w, trap["rect"].h)
                    level["platforms"].append(landed_rect)

        #  SLIDE POPUP SPIKES 
        elif t == "slide_popup_spikes":
            if not trap.get("active") and triggered():
                trap["active"] = True
                trap["progress"] = 0.0
            if trap.get("active"):
                trap["progress"] = min(1.0, trap["progress"] + 0.08)
                
                first_spike_x = trap["positions"][0][0]
                # jesli postac jest "nad" kolcami (px mniejsze niz kolce bo biegnie w lewo)
                if px < first_spike_x + 30 and not trap.get("sliding"):
                    trap["sliding"] = True
                
                if trap.get("sliding"):
                    for i in range(len(trap["positions"])):
                        sx, sh = trap["positions"][i]
                        trap["positions"][i] = [sx - 4.5, sh]
                        
                if trap["progress"] > 0.4:
                    for (sx, sh) in trap["positions"]:
                        kill_rects.append(pygame.Rect(int(sx), FLOOR_Y - int(sh * trap["progress"]),
                                                      20, int(sh * trap["progress"])))

        #  POPUP SPIKES (from floor) 
        elif t == "popup_spikes":
            if not trap["active"] and triggered():
                trap["active"] = True
                trap["progress"] = 0.0
            if trap["active"]:
                trap["progress"] = min(1.0, trap["progress"] + 0.06)
                if trap["progress"] > 0.4:
                    for (sx, sh) in trap["positions"]:
                        kill_rects.append(pygame.Rect(sx, FLOOR_Y - int(sh * trap["progress"]),
                                                      20, int(sh * trap["progress"])))

        #  EDGE SPIKES (appear at platform edges) 
        elif t == "edge_spikes":
            if not trap["active"] and triggered():
                trap["active"] = True
                trap["progress"] = 0.0
            if trap["active"]:
                trap["progress"] = min(1.0, trap["progress"] + 0.05)
                if trap["progress"] > 0.3:
                    for (sx, sh) in trap["positions"]:
                        kill_rects.append(pygame.Rect(sx, FLOOR_Y - int(sh * trap["progress"]),
                                                      18, int(sh * trap["progress"])))

        #  CEILING SPIKES FALL 
        elif t == "ceiling_spikes_fall":
            if not trap["active"] and triggered():
                trap["active"] = True
                trap["speed"] = 0.0
                trap["y"] = -40.0
            if trap["active"]:
                trap["speed"] += 0.5
                trap["y"] += trap["speed"]
                for (sx, sh) in trap["positions"]:
                    kill_rects.append(pygame.Rect(sx, int(trap["y"]), 20, sh))
                if trap["y"] > HEIGHT:
                    trap["y"] = HEIGHT  # stop updating

        #  INVISIBLE WALL 
        elif t == "invisible_wall":
            if not trap.get("visible") and triggered():
                trap["visible"] = True
                level["platforms"].append(trap["rect"])

        #  ESCAPING DOOR 
        elif t == "escaping_door":
            side = trap["side"]
            door_rect = level["door_top"] if side == "top" else level["door_bottom"]
            dx = abs(px - door_rect.centerx)
            dy = abs(py - door_rect.centery)
            dist = math.sqrt(dx*dx + dy*dy)
            if dist < trap["trigger_dist"] and not trap["escaped"]:
                trap["escaped"] = True
            if trap["escaped"]:
                door_rect.y += int(trap["escape_y"])
                trap["escape_y"] -= 0.3  # accelerate upward
                if door_rect.bottom < -100:
                    # Door is gone  put it way off screen
                    door_rect.y = -500

        #  SPAWN CORRECT DOOR 
        elif t == "spawn_correct_door":
            if not trap.get("active") and triggered():
                trap["active"] = True
                door = level["door_top"] if level["correct_side"] == "top" else level["door_bottom"]
                door.y = trap["target_y"]
                level["platforms"].append(trap["platform_rect"])

        #  SCREEN WRAP LEFT 
        elif t == "screen_wrap_left":
            if px < trap["trigger_x"]:
                player.x = trap["target_x"]
                player.y = trap["target_y"]
                player.vy = 0

        #  FAKE DOOR 
        elif t == "fake_door":
            side = trap["side"]
            door_rect = level["door_top"] if side == "top" else level["door_bottom"]
            pr = player.rect
            if pr.colliderect(door_rect):
                player.kill()

        #  MOVING SPIKES 
        elif t == "moving_spikes":
            if not trap["active"] and triggered():
                trap["active"] = True
            if trap["active"]:
                trap["x"] += trap["speed"] * trap["direction"]
                if trap["x"] < 10 or trap["x"] + trap["w"] > WIDTH - 10:
                    trap["direction"] *= -1
                kill_rects.append(pygame.Rect(int(trap["x"]), int(trap["y"]),
                                              trap["w"], trap["h"]))

        #  CLOSING WALLS 
        elif t == "closing_walls":
            if not trap["active"] and triggered():
                trap["active"] = True
            if trap["active"]:
                gap = trap["right_x"] - trap["left_x"]
                if gap > trap.get("min_gap", 50):
                    trap["left_x"] += trap["speed"]
                    trap["right_x"] -= trap["speed"]
                lw = pygame.Rect(int(trap["left_x"]), trap["wall_y"],
                                 trap["wall_w"], trap["wall_h"])
                rw = pygame.Rect(int(trap["right_x"]), trap["wall_y"],
                                 trap["wall_w"], trap["wall_h"])
                kill_rects.append(lw)
                kill_rects.append(rw)

        #  REVERSE CONTROLS (mid-level) 
        elif t == "reverse_controls":
            if not trap["active"] and triggered():
                trap["active"] = True
                player.controls_reversed = True

        #  BLOCK JUMP (mid-level) 
        elif t == "block_jump":
            if not trap["active"] and triggered():
                trap["active"] = True
                player.jump_blocked = True

        #  SPIKE PIT 
        elif t == "spike_pit":
            if not trap["active"] and triggered():
                trap["active"] = True
            if trap["active"]:
                trap["open_progress"] = min(1.0, trap.get("open_progress", 0) + 0.03)
                prog = trap["open_progress"]
                # Move floor segments apart
                lr = trap["left_rect"]
                rr = trap["right_rect"]
                shift = int(40 * prog)
                lr.x = trap.get("orig_lx", lr.x) - shift
                rr.x = trap.get("orig_rx", rr.x) + shift
                if "orig_lx" not in trap:
                    trap["orig_lx"] = lr.x
                    trap["orig_rx"] = rr.x
                # Spikes at the bottom of the pit
                if prog > 0.5:
                    pit_x = trap["pit_x"]
                    pit_w = trap["pit_w"] + shift * 2
                    for sx in range(pit_x - shift, pit_x + trap["pit_w"] + shift, 20):
                        kill_rects.append(pygame.Rect(sx, FLOOR_Y + 20, 18, 18))

    return kill_rects


# 
# TRAP DRAWING
# 
def draw_traps(surf, level, frame):
    for trap in level["traps"]:
        t = trap["type"]

        if t == "vanish_floor":
            if trap["active"] and not trap["gone"]:
                r = trap["rect"].copy()
                r.x += trap.get("shake", 0)
                pygame.draw.rect(surf, PLAT_COL, r)
                # Crack effect
                cx = r.centerx + random.randint(-20, 20)
                pygame.draw.line(surf, RED, (cx, r.y), (cx + random.randint(-15, 15), r.y + 15), 2)

        elif t == "fall_block":
            if trap["active"]:
                br = pygame.Rect(trap["rect"].x, int(trap["y"]),
                                 trap["rect"].w, trap["rect"].h)
                pygame.draw.rect(surf, BLACK, br)
                pygame.draw.rect(surf, RED, br, 2)
                # Warning line
                if not trap.get("landed") and trap["y"] < FLOOR_Y // 2:
                    cx = trap["rect"].x + trap["rect"].w // 2
                    for dy in range(0, int(trap["y"]), 12):
                        if (dy // 6) % 2 == 0:
                            pygame.draw.line(surf, RED, (cx, dy), (cx, min(dy+6, int(trap["y"]))), 1)

        elif t == "popup_spikes":
            if trap["active"]:
                prog = trap["progress"]
                for (sx, sh) in trap["positions"]:
                    vis_h = int(sh * prog)
                    if vis_h > 2:
                        draw_spike_up(surf, sx, FLOOR_Y - vis_h, 20, vis_h)

        elif t == "edge_spikes":
            if trap["active"]:
                prog = trap["progress"]
                for (sx, sh) in trap["positions"]:
                    vis_h = int(sh * prog)
                    if vis_h > 2:
                        draw_spike_up(surf, sx, FLOOR_Y - vis_h, 18, vis_h)

        elif t == "ceiling_spikes_fall":
            if trap["active"] and trap["y"] < HEIGHT:
                for (sx, sh) in trap["positions"]:
                    draw_spike_down(surf, sx, int(trap["y"]), 20, sh)

        elif t == "invisible_wall":
            if trap.get("visible"):
                r = trap["rect"]
                pygame.draw.rect(surf, BLACK, r)
                # Glitch effect
                for gy in range(r.y, r.bottom, 6):
                    if random.random() < 0.3:
                        ox = random.randint(-3, 3)
                        pygame.draw.line(surf, RED, (r.x+ox, gy), (r.right+ox, gy), 1)
                # "!" icon
                warn = font_lg.render("!", True, RED)
                surf.blit(warn, (r.centerx - 7, r.y - 30))

        elif t == "moving_spikes":
            if trap["active"]:
                sx, sy = int(trap["x"]), int(trap["y"])
                draw_spike_up(surf, sx, sy, trap["w"], trap["h"])

        elif t == "closing_walls":
            if trap["active"]:
                lx = int(trap["left_x"])
                rx = int(trap["right_x"])
                wy = trap["wall_y"]
                ww = trap["wall_w"]
                wh = trap["wall_h"]
                pygame.draw.rect(surf, BLACK, (lx, wy, ww, wh))
                pygame.draw.rect(surf, BLACK, (rx, wy, ww, wh))
                # Spikes on inner faces
                for sy in range(wy+15, wy+wh-15, 30):
                    draw_spike_right(surf, lx+ww-5, sy, 15, 20)
                    draw_spike_left(surf, rx-10, sy, 15, 20)

        elif t == "spike_pit":
            if trap["active"] and trap.get("open_progress", 0) > 0.5:
                prog = trap["open_progress"]
                shift = int(40 * prog)
                pit_x = trap["pit_x"]
                pit_w = trap["pit_w"]
                for sx in range(pit_x - shift, pit_x + pit_w + shift, 20):
                    draw_spike_up(surf, sx, FLOOR_Y + 20, 18, 18)


# 
# DRAWING FUNCTIONS
# 
def draw_platform(surf, rect):
    """Flat peach/orange rectangle, no borders."""
    pygame.draw.rect(surf, PLAT_COL, rect)

def draw_ghost_platform(surf, rect, frame):
    """Ghost platform - looks IDENTICAL to real platforms to trick the player."""
    pygame.draw.rect(surf, PLAT_COL, rect)

def draw_door(surf, rect, answer, frame):
    # Door body (main rectangle)
    body = pygame.Rect(rect.x, rect.y + 10, rect.w, rect.h - 10)
    pygame.draw.rect(surf, LIGHT_GRAY, body)
    # Rounded arch at top
    arch_rect = pygame.Rect(rect.x, rect.y, rect.w, 20)
    pygame.draw.ellipse(surf, LIGHT_GRAY, arch_rect)
    # Dark border around the whole door
    pygame.draw.rect(surf, BLACK, body, 1)
    pygame.draw.ellipse(surf, BLACK, arch_rect, 1)
    # Cover the border between arch and body
    pygame.draw.rect(surf, LIGHT_GRAY, (rect.x + 1, rect.y + 10, rect.w - 2, 6))

    # Answer bubble
    txt = font_bub.render(answer, True, BLACK)
    tw, th = txt.get_size()
    bw = tw + 10
    bh = th + 6
    bx = rect.centerx - bw // 2
    by = rect.y - bh - 20

    # White rectangle bubble
    br = pygame.Rect(bx, by, bw, bh)
    pygame.draw.rect(surf, WHITE, br)
    pygame.draw.rect(surf, BLACK, br, 1)
    # Text centered in bubble
    surf.blit(txt, (bx + 5, by + 3))

def draw_static_spike(surf, sp):
    d = sp["dir"]
    x, y, w, h = sp["x"], sp["y"], sp["w"], sp["h"]
    if d == "up":    draw_spike_up(surf, x, y, w, h)
    elif d == "down":draw_spike_down(surf, x, y, w, h)
    elif d == "left":draw_spike_left(surf, x, y, w, h)
    elif d == "right":draw_spike_right(surf, x, y, w, h)

def fmt_time(frames):
    """Format a frame count (at FPS) as M:SS.t"""
    total = max(0, frames) / FPS
    m = int(total // 60)
    s = int(total % 60)
    t = int((total * 10) % 10)
    return f"{m}:{s:02d}.{t}"

def draw_sentence_bar(surf, sentence, level_idx, death_count, play_frames=0):
    lv = font_sm.render(f"LEVEL {level_idx+1}/15", True, BLACK)
    surf.blit(lv, (12, 12))

    # Running timer (top-left, under the level counter)
    tm = font_sm.render(f"TIME  {fmt_time(play_frames)}", True, BLACK)
    surf.blit(tm, (12, 30))

    if death_count > 0:
        dc = font_sm.render(f"Deaths: {death_count}", True, RED)
        surf.blit(dc, (WIDTH - dc.get_width() - 12, 12))

    st = font_lg.render(sentence, True, BLACK)
    sw = st.get_width()
    sx = max(10, (WIDTH - sw) // 2)
    surf.blit(st, (sx, 50))

def draw_troll_warning(surf, text, frame):
    """Flash a troll warning on screen."""
    if (frame // 8) % 2 == 0:
        t = font_lg.render(text, True, WHITE)
        surf.blit(t, ((WIDTH - t.get_width())//2, HEIGHT//2 - 80))


# 
# MENU
# 
def draw_menu(surf, frame):
    surf.fill(BG)

    # Title (white on brown)
    ty = 130 + math.sin(frame * 0.025) * 6
    title = font_xxl.render("ANGOL", True, WHITE)
    surf.blit(title, ((WIDTH - title.get_width())//2, int(ty)))

    sub = font_lg.render("RAGE   PLATFORMER", True, PLAT_COL)
    surf.blit(sub, ((WIDTH - sub.get_width())//2, int(ty) + 65))

    # Divider (peach line)
    pygame.draw.line(surf, PLAT_COL, (WIDTH//2-150, int(ty)+110), (WIDTH//2+150, int(ty)+110), 2)

    # Description
    lines = [
        "A Level-Devil inspired educational game.",
        "Reach the correct door... if you can.",
        "The game WILL trick you. Trust nothing.",
    ]
    for i, line in enumerate(lines):
        t = font_sm.render(line, True, (220, 200, 160))
        surf.blit(t, ((WIDTH - t.get_width())//2, int(ty) + 130 + i * 26))

    # Controls box (peach bg)
    by = int(ty) + 230
    box = pygame.Rect(WIDTH//2-180, by, 360, 130)
    pygame.draw.rect(surf, PLAT_COL, box)

    ct = font_md.render("CONTROLS", True, BLACK)
    surf.blit(ct, ((WIDTH - ct.get_width())//2, by + 10))

    ctrls = [
        "Arrows / WASD  --  Move & Jump",
        "Space  --  Jump",
        "R  --  Restart Level",
        "ESC  --  Quit",
    ]
    for i, c in enumerate(ctrls):
        t = font_sm.render(c, True, (60, 40, 5))
        surf.blit(t, ((WIDTH - t.get_width())//2, by + 40 + i * 22))

    # Start prompt (blinking white)
    if frame > 30 and abs(math.sin(frame * 0.06)) > 0.25:
        st = font_lg.render("Press ENTER or SPACE to start", True, WHITE)
        surf.blit(st, ((WIDTH - st.get_width())//2, HEIGHT - 85))

    # Decorative player icon (black blocky runner with animated legs)
    px, py_icon = WIDTH//2 - 16, HEIGHT - 150
    s = math.sin(frame * 0.18)
    # Head
    pygame.draw.rect(surf, PLAYER_BODY, (px + 6, py_icon, 20, 14))
    # Torso
    pygame.draw.rect(surf, PLAYER_BODY, (px, py_icon + 14, 32, 22))
    # Animated legs
    l_off = int(s * 5)
    r_off = int(-s * 5)
    pygame.draw.rect(surf, PLAYER_BODY, (px + 3 + l_off, py_icon + 36, 9, 14 - abs(l_off)//2))
    pygame.draw.rect(surf, PLAYER_BODY, (px + 20 + r_off, py_icon + 36, 9, 14 - abs(r_off)//2))
    # Swinging arm
    pygame.draw.rect(surf, PLAYER_BODY, (px + 30, py_icon + 16 + int(s * 3), 6, 12))


# 
# DEATH / WIN / END SCREENS
# 
DEATH_MESSAGES = [
    "GOTCHA!", "NOPE!", "TRY AGAIN!", "HA HA!",
    "TROLLED!", "OOPS!", "NOT TODAY!", "NICE TRY!",
    "LOL!", "DESTROYED!", "RIP!", "SURPRISE!",
]

def draw_death_overlay(surf, timer):
    if timer < 5:
        return
    ov = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
    a = min(140, timer * 6)
    ov.fill((210, 40, 40, a))
    surf.blit(ov, (0, 0))

    if timer > 10:
        random.seed(timer // 25)
        msg = random.choice(DEATH_MESSAGES)
        random.seed()
        t = font_xl.render(msg, True, WHITE)
        surf.blit(t, ((WIDTH - t.get_width())//2, HEIGHT//2 - 30))

        h = font_sm.render("Restarting...", True, (240, 200, 200))
        surf.blit(h, ((WIDTH - h.get_width())//2, HEIGHT//2 + 30))

def draw_win_overlay(surf, timer):
    if timer < 5:
        return
    ov = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
    a = min(100, timer * 5)
    ov.fill((50, 180, 70, a))
    surf.blit(ov, (0, 0))

    t = font_xl.render("CORRECT!", True, WHITE)
    surf.blit(t, ((WIDTH - t.get_width())//2, HEIGHT//2 - 25))

def draw_end_screen(surf, frame):
    surf.fill(BG)

    # Confetti particles
    if frame % 4 == 0:
        for _ in range(2):
            spawn_particles(random.randint(100, WIDTH-100), -10, 1,
                            [PLAT_COL, YELLOW, WHITE, ORANGE],
                            (-2, 2), (2, 5))

    ty = 100 + math.sin(frame * 0.03) * 8

    t1 = font_xxl.render("CONGRATULATIONS!", True, WHITE)
    surf.blit(t1, ((WIDTH - t1.get_width())//2, int(ty)))

    t2 = font_lg.render("You completed all 15 levels!", True, PLAT_COL)
    surf.blit(t2, ((WIDTH - t2.get_width())//2, int(ty) + 75))

    t3 = font_md.render("You are a true ANGOL champion!", True, (220, 200, 160))
    surf.blit(t3, ((WIDTH - t3.get_width())//2, int(ty) + 120))

    # Trophy (flat rectangles)
    cx, cy = WIDTH//2, int(ty) + 230
    pygame.draw.rect(surf, YELLOW, (cx-35, cy-25, 70, 50))
    pygame.draw.rect(surf, (200, 170, 30), (cx-30, cy-20, 60, 40))
    pygame.draw.rect(surf, YELLOW, (cx-12, cy+25, 24, 8))
    pygame.draw.rect(surf, YELLOW, (cx-20, cy+33, 40, 7))

    if frame > 40 and abs(math.sin(frame * 0.06)) > 0.3:
        rt = font_md.render("Press R to restart  |  Press ESC to quit", True, (220, 200, 160))
        surf.blit(rt, ((WIDTH - rt.get_width())//2, HEIGHT - 80))


# 
# GAME STATES
# 
ST_MENU    = 0
ST_PLAY    = 1
ST_DYING   = 2
ST_WIN_LV  = 3
ST_END     = 4
ST_IMAGE   = 5   # final reveal: show the secret image

# 
# SECRET END IMAGE (shown after the final spike)
# 
import os
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
WIN_IMAGE_FILE = "1520056982297.jfif"
_win_image_cache = None

def get_win_image():
    """Load the secret image once (lazily); returns a Surface or None."""
    global _win_image_cache
    if _win_image_cache is None:
        try:
            img = pygame.image.load(os.path.join(SCRIPT_DIR, WIN_IMAGE_FILE))
            _win_image_cache = img.convert()
        except Exception as e:
            print("Could not load win image:", e)
            _win_image_cache = False
    return _win_image_cache or None

def draw_image_screen(surf, frame, final_time=None):
    surf.fill(BLACK)
    img = get_win_image()
    if img:
        iw, ih = img.get_size()
        scale = min(WIDTH / iw, (HEIGHT - 90) / ih)
        nw, nh = max(1, int(iw * scale)), max(1, int(ih * scale))
        simg = pygame.transform.smoothscale(img, (nw, nh))
        surf.blit(simg, ((WIDTH - nw) // 2, (HEIGHT - nh) // 2 - 20))
    else:
        t = font_xxl.render("THE END", True, WHITE)
        surf.blit(t, ((WIDTH - t.get_width()) // 2, HEIGHT // 2 - 30))

    # Final time, shown together with the image
    if final_time is not None:
        label = font_xl.render(f"YOUR TIME:  {fmt_time(final_time)}", True, WHITE)
        bw, bh = label.get_width() + 28, label.get_height() + 12
        bx, by = (WIDTH - bw) // 2, 14
        strip = pygame.Surface((bw, bh), pygame.SRCALPHA)
        strip.fill((0, 0, 0, 160))
        surf.blit(strip, (bx, by))
        surf.blit(label, (bx + 14, by + 6))

    if frame % 60 < 40:
        p = font_sm.render("Press ENTER or ESC to return to menu", True, (220, 200, 160))
        surf.blit(p, ((WIDTH - p.get_width()) // 2, HEIGHT - 30))

# 
# MAIN
# 
async def main():
    global particles

    state = ST_MENU
    cur_level = 0
    level = None
    player = None
    death_timer = 0
    win_timer = 0
    frame = 0
    shake = (0, 0)
    deaths = 0
    troll_msg = ""
    troll_timer = 0
    play_frames = 0      # gameplay time counter (in frames)
    final_time = None    # frozen time shown with the end image
    wait_frames = 0      # final-level countdown (frozen-then-die)

    def load_lv(idx):
        global particles
        nonlocal level, player, troll_msg, troll_timer, wait_frames
        particles = []
        troll_msg = ""
        troll_timer = 0
        wait_frames = 0
        level = build_level(idx)
        px, py = level["player_start"]
        player = Player(px, py)
        player.controls_reversed = level["controls_reversed"]
        player.jump_blocked = level["jump_blocked"]
        player.jump_reversed = level["jump_reversed"]
        player.super_gravity = level["super_gravity"]
        player.gravity_mult = level["gravity_mult"]
        if level["super_gravity"]:
            player.gravity_mult = 1.8

    running = True
    while running:
        clock.tick(FPS)
        frame += 1

        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                running = False
            if ev.type == pygame.KEYDOWN:
                if ev.key == pygame.K_ESCAPE:
                    if state in (ST_MENU, ST_END):
                        running = False
                    else:
                        state = ST_MENU
                if state == ST_MENU:
                    if ev.key in (pygame.K_RETURN, pygame.K_SPACE):
                        state = ST_PLAY
                        cur_level = 0
                        deaths = 0
                        play_frames = 0
                        final_time = None
                        load_lv(0)
                elif state == ST_PLAY:
                    if ev.key == pygame.K_r:
                        deaths += 1
                        load_lv(cur_level)
                elif state == ST_END:
                    if ev.key == pygame.K_r:
                        state = ST_MENU
                elif state == ST_IMAGE:
                    if ev.key in (pygame.K_RETURN, pygame.K_SPACE):
                        state = ST_MENU

        keys = pygame.key.get_pressed()

        # Advance the gameplay timer while actively playing
        if state in (ST_PLAY, ST_DYING, ST_WIN_LV):
            play_frames += 1

        #  UPDATE 
        if state == ST_PLAY:
            # Build collision list (solid platforms + walls, NOT ghost)
            solids = list(level["platforms"]) + level["walls"]

            player.update(solids, keys)

            # FINAL LEVEL: stand STILL for 10 s -> you die -> ends the game.
            # Any movement (keys / walking / being airborne) resets the timer.
            if level.get("final_wait"):
                pressing = (keys[pygame.K_LEFT] or keys[pygame.K_RIGHT] or
                            keys[pygame.K_a] or keys[pygame.K_d] or
                            keys[pygame.K_UP] or keys[pygame.K_w] or keys[pygame.K_SPACE] or
                            keys[pygame.K_DOWN] or keys[pygame.K_s])
                # (resting on the floor jitters vy by ~0.55, so allow a small band)
                still = (not pressing) and abs(player.vx) < 0.1 and abs(player.vy) < 1.0
                if still:
                    wait_frames += 1
                else:
                    wait_frames = 0
                if wait_frames >= 10 * FPS:
                    spawn_particles(player.x + player.w/2, player.y + player.h/2,
                                    30, [PLAYER_BODY, DARK_GRAY, RED, GRAY])
                    final_time = play_frames
                    state = ST_IMAGE
                    frame = 0
                    shake = (0, 0)

            # Trap updates
            kill_rects = update_traps(level, player, frame)

            # Check kills
            if player.alive:
                pr = player.rect

                # FINAL LEVEL: touching the special spike finishes the game
                wsr = level.get("win_spike_rect")
                if wsr is not None and pr.colliderect(wsr):
                    state = ST_IMAGE
                    frame = 0
                    shake = (0, 0)
                    final_time = play_frames
                    spawn_particles(wsr.centerx, wsr.centery, 30,
                                    [RED, WHITE, PLAYER_BODY, GRAY])

                if state == ST_PLAY:
                    for kr in kill_rects:
                        if pr.colliderect(kr):
                            player.kill()
                            break

                # Deadly platforms (look normal, kill on touch)
                if state == ST_PLAY and player.alive:
                    for dp in level.get("death_plats", []):
                        if pr.colliderect(dp):
                            player.kill()
                            break

                # Static spikes
                if state == ST_PLAY and player.alive:
                    for sp in level["spikes"]:
                        if level.get("win_spike_rect") is not None and \
                           sp["x"] == level["win_spike_rect"].x and \
                           sp["y"] == level["win_spike_rect"].y:
                            continue  # the win spike never kills
                        sr = pygame.Rect(sp["x"], sp["y"], sp["w"], sp["h"])
                        if pr.colliderect(sr):
                            player.kill()
                            break

                # Door collision (disabled on the final stand-still level)
                if player.alive and level.get("win_spike_rect") is None \
                        and not level.get("final_wait"):
                    if pr.colliderect(level["door_top"]):
                        if level["correct_side"] == "top":
                            state = ST_WIN_LV
                            win_timer = 0
                            spawn_particles(level["door_top"].centerx,
                                            level["door_top"].centery,
                                            30, [GREEN, CYAN, WHITE, YELLOW])
                        else:
                            player.kill()
                            troll_msg = "WRONG DOOR!"
                            troll_timer = 40

                    elif pr.colliderect(level["door_bottom"]):
                        if level["correct_side"] == "bottom":
                            state = ST_WIN_LV
                            win_timer = 0
                            spawn_particles(level["door_bottom"].centerx,
                                            level["door_bottom"].centery,
                                            30, [GREEN, CYAN, WHITE, YELLOW])
                        else:
                            player.kill()
                            troll_msg = "WRONG DOOR!"
                            troll_timer = 40

            if not player.alive and state == ST_PLAY:
                state = ST_DYING
                death_timer = 0
                deaths += 1
                shake = (random.randint(-5, 5), random.randint(-3, 3))

            # Troll message timer
            if troll_timer > 0:
                troll_timer -= 1

        elif state == ST_DYING:
            death_timer += 1
            if death_timer < 12:
                shake = (random.randint(-4, 4), random.randint(-3, 3))
            else:
                shake = (0, 0)
            if death_timer > 55:
                state = ST_PLAY
                load_lv(cur_level)

        elif state == ST_WIN_LV:
            win_timer += 1
            if win_timer > 45:
                cur_level += 1
                if cur_level >= len(LEVEL_DATA):
                    state = ST_END
                    frame = 0
                else:
                    state = ST_PLAY
                    load_lv(cur_level)

        # Particles
        for p in particles[:]:
            p.update()
            if p.life <= 0:
                particles.remove(p)

        #  DRAW 
        render = pygame.Surface((WIDTH, HEIGHT))

        if state == ST_MENU:
            draw_menu(render, frame)

        elif state in (ST_PLAY, ST_DYING, ST_WIN_LV):
            render.fill(BG)

            # Ghost platforms (drawn like normal but player falls through)
            for gp in level["ghost_plats"]:
                draw_ghost_platform(render, gp, frame)

            # Solid platforms
            for p in level["platforms"]:
                draw_platform(render, p)

            # Deadly platforms (drawn identically  a hidden trap)
            for dp in level.get("death_plats", []):
                draw_platform(render, dp)

            # Static spikes
            for sp in level["spikes"]:
                draw_static_spike(render, sp)

            # Traps
            draw_traps(render, level, frame)

            # Doors
            draw_door(render, level["door_top"], level["top_answer"], frame)
            draw_door(render, level["door_bottom"], level["bottom_answer"], frame)

            # Player
            if state != ST_DYING or death_timer < 4:
                player.draw(render)

            # Particles
            for p in particles:
                p.draw(render)

            # Sentence bar (on top)
            draw_sentence_bar(render, level["sentence"], cur_level, deaths, play_frames)

            # Final-level countdown: only runs while you stand perfectly still
            if level.get("final_wait") and wait_frames > 0:
                remaining = max(0, 10 - wait_frames // FPS)
                cd = font_xxl.render(str(remaining), True, RED)
                render.blit(cd, ((WIDTH - cd.get_width()) // 2, HEIGHT // 2 - 90))
                msg = font_md.render("DON'T MOVE!", True, WHITE)
                render.blit(msg, ((WIDTH - msg.get_width()) // 2, HEIGHT // 2 - 20))

            # Troll modifier warnings
            if level["controls_reversed"] and frame % 120 < 60:
                warn = font_sm.render("! CONTROLS REVERSED !", True, RED)
                render.blit(warn, ((WIDTH - warn.get_width())//2, 56))
            if level["jump_blocked"]:
                warn = font_sm.render("! JUMP DISABLED !", True, RED)
                render.blit(warn, ((WIDTH - warn.get_width())//2, 56))
            if level["jump_reversed"]:
                warn = font_sm.render("! PRESS DOWN TO JUMP !", True, RED)
                render.blit(warn, ((WIDTH - warn.get_width())//2, 56))
            if level["super_gravity"]:
                warn = font_sm.render("! SUPER GRAVITY !", True, RED)
                render.blit(warn, ((WIDTH - warn.get_width())//2, 56))

            # Mid-level troll warnings (from traps that activate)
            for trap in level["traps"]:
                if trap["type"] == "reverse_controls" and trap.get("active") and frame % 100 < 50:
                    warn = font_sm.render("! CONTROLS REVERSED !", True, RED)
                    render.blit(warn, ((WIDTH - warn.get_width())//2, 56))
                if trap["type"] == "block_jump" and trap.get("active"):
                    warn = font_sm.render("! JUMP BLOCKED !", True, RED)
                    render.blit(warn, ((WIDTH - warn.get_width())//2, 56))

            # Troll message
            if troll_timer > 0:
                draw_troll_warning(render, troll_msg, frame)

            # Overlays
            if state == ST_DYING:
                draw_death_overlay(render, death_timer)
            elif state == ST_WIN_LV:
                draw_win_overlay(render, win_timer)

        elif state == ST_END:
            draw_end_screen(render, frame)
            for p in particles:
                p.draw(render)

        elif state == ST_IMAGE:
            draw_image_screen(render, frame, final_time)

        # Apply shake & display
        screen.fill(BG)
        screen.blit(render, shake)
        pygame.display.flip()
        await asyncio.sleep(0)

    pygame.quit()
    sys.exit()

if __name__ == "__main__":
    asyncio.run(main())
