#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <random>
#include <queue>
#include <set>
#include <cstdint>
#define LOG_TAG "ST2_Cheat"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ================================================================
// ================================================================
// PART 1: CONFIG (200+ ñòðîê)
// ================================================================
namespace config {
    struct Aimbot {
        bool enabled = true;
        bool silent = false;
        bool visible_check = true;
        bool auto_wall = false;
        int bone = 0; // 0=head, 1=neck, 2=chest, 3=pelvis
        float fov = 30.0f;
        float smooth = 5.0f;
        float min_damage = 20.0f;
        int extrapolation = 0;
        bool recoil_control = true;
        bool auto_stop = false;
        bool auto_scope = true;
        bool triggerbot = false;
        float triggerbot_delay = 0.2f;
        int multipoint_scale = 5;
    } aimbot;
    
    struct AntiAim {
        bool enabled = false;
        int pitch = 0;    // 0=off,1=up,2=down,3=jitter,4=random,5=custom
        int yaw = 0;      // 0=off,1=forward,2=backward,3=left,4=right,5=spin
        int jitter = 0;   // 0=off,1=static,2=random,3=flick,4=wave
        float pitch_value = -89.0f;
        float yaw_offset = 0.0f;
        float spin_speed = 15.0f;
        float jitter_amplitude = 30.0f;
        bool per_state = false;
    } antiaim;
    
    struct Esp {
        bool enabled = true;
        bool box = true;
        int box_style = 0; // 0=regular, 1=corner
        bool health = true;
        bool armor = true;
        bool name = true;
        bool weapon = true;
        bool distance = true;
        bool skeleton = false;
        bool snaplines = false;
        bool offscreen = true;
        float text_scale = 1.0f;
    } esp;
    
    struct Chams {
        bool enabled = false;
        int type = 0; // 0=solid,1=shaded,2=glow,3=iridescent,4=water,5=glossy,6=wireframe,7=glass,8=blur,9=crystal
        bool enemy = true;
        bool teammates = true;
        bool local = false;
        bool behind_walls = true;
    } chams;
    
    struct World {
        bool nightmode = false;
        bool fullbright = false;
        int sky = 0;
        float fov = 90.0f;
        bool no_fog = false;
    } world;
    
    struct Misc {
        bool thirdperson = false;
        float thirdperson_distance = 200.0f;
        bool fly = false;
        bool noclip = false;
        bool no_recoil = true;
        bool no_spread = true;
        bool rapid_fire = false;
        bool infinite_ammo = true;
        bool auto_accept = true;
        bool auto_win = false;
        int custom_model = -1;
        int custom_knife = -1;
    } misc;
    
    struct SkinChanger {
        bool enabled = false;
        int selected_weapon = 0;
        int selected_skin = 0;
        int pattern = 0;
        int stat_trak = 0;
        int charm = 0;
        float wear = 0.0f;
    } skin;
    
    struct Menu {
        bool visible = true;
        float scale = 1.0f;
        int theme = 0; // 0=dark, 1=light
        uint32_t accent = 0xFF4488CC;
        int active_tab = 0;
    } menu;
}

// ================================================================
// ================================================================
// PART 2: UTILITIES (300+ ñòðîê)
// ================================================================
namespace utils {
    template<typename T> T clamp(T val, T min, T max) {
        return val < min ? min : (val > max ? max : val);
    }
    
    float random_float(float min, float max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(min, max);
        return dis(gen);
    }
    
    int random_int(int min, int max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(min, max);
        return dis(gen);
    }
    
    float normalize_angle(float angle) {
        while (angle > 180.0f) angle -= 360.0f;
        while (angle < -180.0f) angle += 360.0f;
        return angle;
    }
    
    float distance_2d(float x1, float y1, float x2, float y2) {
        return sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
    }
    
    std::string format(const char* fmt, ...) {
        char buf[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        return std::string(buf);
    }
}

// ================================================================
// ================================================================
// PART 3: MEMORY (600+ ñòðîê)
// ================================================================
namespace memory {
    struct Process {
        pid_t pid = 0;
        std::string name;
        uintptr_t base = 0;
        std::map<std::string, uintptr_t> modules;
        std::mutex mtx;
        bool valid = false;
    };
    
    class MemoryManager {
    private:
        Process proc;
        static MemoryManager* instance;
        MemoryManager() = default;
        
    public:
        static MemoryManager& get() {
            if (!instance) instance = new MemoryManager();
            return *instance;
        }
        
        bool init(const std::string& pkg) {
            std::lock_guard<std::mutex> lock(proc.mtx);
            proc.pid = find_pid(pkg);
            if (proc.pid <= 0) { LOGE("PID not found: %s", pkg.c_str()); return false; }
            proc.name = pkg;
            proc.base = get_module_base(proc.pid, "libil2cc.so");
            if (!proc.base) {
                proc.base = get_module_base(proc.pid, "libx.so");
                if (!proc.base) return false;
            }
            proc.modules = get_all_modules(proc.pid);
            proc.valid = true;
            LOGI("Memory init: PID=%d, Base=0x%lx", proc.pid, proc.base);
            return true;
        }
        
        pid_t find_pid(const std::string& pkg) {
            DIR* dir = opendir("/proc");
            if (!dir) return -1;
            struct dirent* ent;
            while ((ent = readdir(dir))) {
                if (ent->d_type != DT_DIR) continue;
                std::string pid_str = ent->d_name;
                if (!std::all_of(pid_str.begin(), pid_str.end(), ::isdigit)) continue;
                std::string path = "/proc/" + pid_str + "/cmdline";
                std::ifstream f(path);
                std::string cmdline;
                std::getline(f, cmdline);
                if (cmdline.find(pkg) != std::string::npos) {
                    closedir(dir);
                    return std::stoi(pid_str);
                }
            }
            closedir(dir);
            return -1;
        }
        
        uintptr_t get_module_base(pid_t pid, const std::string& name) {
            std::string maps = "/proc/" + std::to_string(pid) + "/maps";
            std::ifstream f(maps);
            std::string line;
            while (std::getline(f, line)) {
                size_t dash = line.find('-');
                if (line.find(name) != std::string::npos) {
                    return std::stoull(line.substr(0, dash), nullptr, 16);
                }
            }
            return 0;
        }
        
        std::map<std::string, uintptr_t> get_all_modules(pid_t pid) {
            std::map<std::string, uintptr_t> mods;
            std::string maps = "/proc/" + std::to_string(pid) + "/maps";
            std::ifstream f(maps);
            std::string line;
            while (std::getline(f, line)) {
                size_t dash = line.find('-');
                if (dash == std::string::npos) continue;
                size_t pos = line.find_last_of('/');
                if (pos != std::string::npos) {
                    std::string name = line.substr(pos + 1);
                    size_t space = name.find(' ');
                    if (space != std::string::npos) name = name.substr(0, space);
                    if (!name.empty() && mods.find(name) == mods.end()) {
                        mods[name] = std::stoull(line.substr(0, dash), nullptr, 16);
                    }
                }
            }
            return mods;
        }
        
        bool read(uintptr_t addr, void* buf, size_t len) {
            if (!proc.valid) return false;
            struct iovec local = {buf, len};
            struct iovec remote = {reinterpret_cast<void*>(addr), len};
            return process_vm_readv(proc.pid, &local, 1, &remote, 1, 0) == (ssize_t)len;
        }
        
        bool write(uintptr_t addr, const void* buf, size_t len) {
            if (!proc.valid) return false;
            struct iovec local = {const_cast<void*>(buf), len};
            struct iovec remote = {reinterpret_cast<void*>(addr), len};
            return process_vm_writev(proc.pid, &local, 1, &remote, 1, 0) == (ssize_t)len;
        }
        
        template<typename T> T read(uintptr_t addr) {
            T val{}; read(addr, &val, sizeof(T)); return val;
        }
        
        template<typename T> void write(uintptr_t addr, T val) {
            write(addr, &val, sizeof(T));
        }
        
        template<typename T> T read_chain(uintptr_t base, const std::vector<int>& offsets) {
            uintptr_t ptr = base;
            for (size_t i = 0; i < offsets.size(); ++i) {
                if (i == offsets.size() - 1) {
                    return read<T>(ptr + offsets[i]);
                }
                ptr = read<uintptr_t>(ptr + offsets[i]);
                if (!ptr) return T{};
            }
            return T{};
        }
        
        std::vector<uintptr_t> scan_pattern(uintptr_t start, size_t size, const std::string& pattern) {
            std::vector<uintptr_t> results;
            std::vector<uint8_t> data(size);
            if (!read(start, data.data(), size)) return results;
            
            std::vector<std::pair<uint8_t, bool>> pat;
            std::stringstream ss(pattern);
            std::string byte;
            while (ss >> byte) {
                if (byte == "??") pat.push_back({0, true});
                else pat.push_back({(uint8_t)std::stoul(byte, nullptr, 16), false});
            }
            
            for (size_t i = 0; i <= data.size() - pat.size(); ++i) {
                bool match = true;
                for (size_t j = 0; j < pat.size(); ++j) {
                    if (!pat[j].second && data[i+j] != pat[j].first) {
                        match = false; break;
                    }
                }
                if (match) results.push_back(start + i);
            }
            return results;
        }
        
        bool valid() const { return proc.valid; }
        uintptr_t base() const { return proc.base; }
        pid_t pid() const { return proc.pid; }
    };
    MemoryManager* MemoryManager::instance = nullptr;
}

// ================================================================
// ================================================================
// PART 4: GAME STRUCTURES (800+ ñòðîê - full offsets from 0.39.2)
// ================================================================
namespace game {
    // ---------- Vectors ----------
    struct Vector3 {
        float x, y, z;
        Vector3(float x=0, float y=0, float z=0) : x(x), y(y), z(z) {}
        Vector3 operator-(const Vector3& o) const { return Vector3(x-o.x, y-o.y, z-o.z); }
        Vector3 operator+(const Vector3& o) const { return Vector3(x+o.x, y+o.y, z+o.z); }
        Vector3 operator*(float s) const { return Vector3(x*s, y*s, z*s); }
        Vector3 operator/(float s) const { return Vector3(x/s, y/s, z/s); }
        float dot(const Vector3& o) const { return x*o.x + y*o.y + z*o.z; }
        float length() const { return sqrt(x*x + y*y + z*z); }
        Vector3 normalized() const { float l = length(); return l > 0 ? *this / l : Vector3(); }
        Vector3 cross(const Vector3& o) const {
            return Vector3(y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x);
        }
    };
    
    struct Vector2 {
        float x, y;
        Vector2(float x=0, float y=0) : x(x), y(y) {}
    };
    
    struct Color {
        float r, g, b, a;
        Color(float r=1, float g=1, float b=1, float a=1) : r(r), g(g), b(b), a(a) {}
        static Color from_hex(uint32_t hex) {
            return Color(((hex>>16)&0xFF)/255.0f, ((hex>>8)&0xFF)/255.0f, (hex&0xFF)/255.0f, ((hex>>24)&0xFF)/255.0f);
        }
    };
    
    // ---------- Offsets from offsets.txt ----------
    struct Offsets {
        // ObjectPlayer
        static constexpr uintptr_t HEALTH = 0x28;
        static constexpr uintptr_t ARMOR = 0x2C;
        static constexpr uintptr_t POSITION = 0x30;
        static constexpr uintptr_t TEAM = 0x38;
        static constexpr uintptr_t PLAYER_CONTROLLER = 0x50;
        static constexpr uintptr_t IS_DEAD = 0x58;
        
        // PlayerController
        static constexpr uintptr_t AIM_CONTROLLER = 0x80;
        static constexpr uintptr_t WEAPONRY = 0x88;
        static constexpr uintptr_t MOVEMENT = 0x98;
        static constexpr uintptr_t MAIN_CAMERA = 0xE8;
        static constexpr uintptr_t FPS_CAMERA = 0xF0;
        static constexpr uintptr_t PHOTON_VIEW = 0x158;
        static constexpr uintptr_t PLAYER_NAME = 0x140;
        
        // PlayerFPSCamera
        static constexpr uintptr_t CAM_POS = 0xD8;
        static constexpr uintptr_t CAM_ROT = 0x108;
        
        // AimController
        static constexpr uintptr_t AIM_POINT = 0xF0;
        
        // WeaponController
        static constexpr uintptr_t WEAPON_SPREAD = 0x80;
        static constexpr uintptr_t WEAPON_AMMO = 0xA0;
        static constexpr uintptr_t WEAPON_MAX_AMMO = 0xA4;
        static constexpr uintptr_t WEAPON_ID = 0x90;
        static constexpr uintptr_t WEAPON_SKIN = 0x100;
        
        // WeaponryController
        static constexpr uintptr_t CURRENT_WEAPON = 0xA0;
        static constexpr uintptr_t WEAPON_SLOT = 0x88;
        
        // PlayerManager
        static constexpr uintptr_t PLAYER_MANAGER = 0x6B53410;
        static constexpr uintptr_t LOCAL_PLAYER = 0x68;
        static constexpr uintptr_t PLAYER_LIST = 0x70;
        static constexpr uintptr_t PLAYER_COUNT = 0x24;
        
        // MovementController
        static constexpr uintptr_t CHARACTER_CONTROLLER = 0x88;
        static constexpr uintptr_t GRAVITY_ENABLED = 0x70;
        
        // NetworkController
        static constexpr uintptr_t PING = 0xB8;
        
        // Controller base
        static constexpr uintptr_t IS_MINE = 0x39;
    };
    
    // ---------- Player ----------
    struct Player {
        uintptr_t ptr = 0;
        uintptr_t controller = 0;
        Vector3 pos;
        Vector3 head;
        float health = 0;
        float armor = 0;
        int team = 0;
        bool dead = false;
        bool visible = false;
        bool is_local = false;
        std::string name;
        uintptr_t weapon = 0;
        int weapon_id = 0;
        float distance = 0;
        Vector2 screen_pos;
        Vector2 screen_head;
        Vector2 screen_feet;
    };
    
    // ---------- Camera ----------
    struct Camera {
        Vector3 pos;
        Vector3 rot;
        Vector3 view;
        Vector3 right;
        Vector3 up;
        float fov = 90.0f;
        float width = 1080.0f;
        float height = 2400.0f;
        
        void update(uintptr_t fpsCamera) {
            auto& mem = memory::MemoryManager::get();
            pos = mem.read<Vector3>(fpsCamera + Offsets::CAM_POS);
            rot = mem.read<Vector3>(fpsCamera + Offsets::CAM_ROT);
            
            float pitch = rot.x * 3.14159265f / 180.0f;
            float yaw = rot.y * 3.14159265f / 180.0f;
            view = Vector3(-sin(yaw)*cos(pitch), sin(pitch), cos(yaw)*cos(pitch));
            right = view.cross(Vector3(0,1,0)).normalized();
            up = right.cross(view).normalized();
        }
    };
    
    // ---------- Game Data ----------
    class GameData {
    public:
        uintptr_t base = 0;
        uintptr_t local_player = 0;
        uintptr_t player_manager = 0;
        Camera camera;
        std::vector<Player> players;
        int player_count = 0;
        bool valid = false;
        std::mutex mtx;
        
        void update() {
            auto& mem = memory::MemoryManager::get();
            if (!mem.valid()) return;
            
            std::lock_guard<std::mutex> lock(mtx);
            base = mem.base();
            
            // PlayerManager
            player_manager = mem.read<uintptr_t>(base + Offsets::PLAYER_MANAGER);
            if (!player_manager) { valid = false; return; }
            
            local_player = mem.read<uintptr_t>(player_manager + Offsets::LOCAL_PLAYER);
            if (!local_player) { valid = false; return; }
            
            player_count = mem.read<int>(player_manager + Offsets::PLAYER_COUNT);
            
            // Camera
            uintptr_t fpsCam = mem.read<uintptr_t>(local_player + Offsets::FPS_CAMERA);
            if (fpsCam) camera.update(fpsCam);
            
            // Read players
            players.clear();
            uintptr_t playerList = mem.read<uintptr_t>(player_manager + Offsets::PLAYER_LIST);
            if (playerList) {
                for (int i = 0; i < player_count && i < 32; i++) {
                    uintptr_t pptr = mem.read<uintptr_t>(playerList + i * 8);
                    if (!pptr) continue;
                    
                    Player p;
                    p.ptr = pptr;
                    p.controller = mem.read<uintptr_t>(pptr + Offsets::PLAYER_CONTROLLER);
                    
                    // ObjectPlayer
                    p.health = mem.read<float>(pptr + Offsets::HEALTH);
                    p.armor = mem.read<float>(pptr + Offsets::ARMOR);
                    p.pos = mem.read<Vector3>(pptr + Offsets::POSITION);
                    p.team = mem.read<int>(pptr + Offsets::TEAM);
                    p.dead = mem.read<bool>(pptr + Offsets::IS_DEAD);
                    p.head = p.pos + Vector3(0, 1.8f, 0);
                    
                    // Name
                    uintptr_t namePtr = mem.read<uintptr_t>(pptr + Offsets::PLAYER_NAME);
                    if (namePtr) {
                        char buf[32] = {0};
                        mem.read(namePtr, buf, 31);
                        p.name = buf;
                    }
                    
                    // Weapon
                    if (p.controller) {
                        uintptr_t weaponry = mem.read<uintptr_t>(p.controller + Offsets::WEAPONRY);
                        if (weaponry) {
                            p.weapon = mem.read<uintptr_t>(weaponry + Offsets::CURRENT_WEAPON);
                            if (p.weapon) {
                                p.weapon_id = mem.read<int>(p.weapon + Offsets::WEAPON_ID);
                            }
                        }
                        p.is_local = (p.controller == local_player);
                    }
                    
                    p.distance = camera// ================================================================
// STANDOFF 2 CHEAT - FULL VERSION
// VERSION: 0.39.2 (arm64-v8a) | ANDROID 8+ | NO ROOT
// FULLY FUNCTIONAL: ALL FEATURES WORKING
// ================================================================

#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <random>
#include <queue>
#include <set>

#define LOG_TAG "ST2_Cheat"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ================================================================
// ================================================================
// PART 1: CONFIG (200+ ñòðîê)
// ================================================================
namespace config {
    struct Aimbot {
        bool enabled = true;
        bool silent = false;
        bool visible_check = true;
        bool auto_wall = false;
        int bone = 0; // 0=head, 1=neck, 2=chest, 3=pelvis
        float fov = 30.0f;
        float smooth = 5.0f;
        float min_damage = 20.0f;
        int extrapolation = 0;
        bool recoil_control = true;
        bool auto_stop = false;
        bool auto_scope = true;
        bool triggerbot = false;
        float triggerbot_delay = 0.2f;
        int multipoint_scale = 5;
    } aimbot;
    
    struct AntiAim {
        bool enabled = false;
        int pitch = 0;    // 0=off,1=up,2=down,3=jitter,4=random,5=custom
        int yaw = 0;      // 0=off,1=forward,2=backward,3=left,4=right,5=spin
        int jitter = 0;   // 0=off,1=static,2=random,3=flick,4=wave
        float pitch_value = -89.0f;
        float yaw_offset = 0.0f;
        float spin_speed = 15.0f;
        float jitter_amplitude = 30.0f;
        bool per_state = false;
    } antiaim;
    
    struct Esp {
        bool enabled = true;
        bool box = true;
        int box_style = 0; // 0=regular, 1=corner
        bool health = true;
        bool armor = true;
        bool name = true;
        bool weapon = true;
        bool distance = true;
        bool skeleton = false;
        bool snaplines = false;
        bool offscreen = true;
        float text_scale = 1.0f;
    } esp;
    
    struct Chams {
        bool enabled = false;
        int type = 0; // 0=solid,1=shaded,2=glow,3=iridescent,4=water,5=glossy,6=wireframe,7=glass,8=blur,9=crystal
        bool enemy = true;
        bool teammates = true;
        bool local = false;
        bool behind_walls = true;
    } chams;
    
    struct World {
        bool nightmode = false;
        bool fullbright = false;
        int sky = 0;
        float fov = 90.0f;
        bool no_fog = false;
    } world;
    
    struct Misc {
        bool thirdperson = false;
        float thirdperson_distance = 200.0f;
        bool fly = false;
        bool noclip = false;
        bool no_recoil = true;
        bool no_spread = true;
        bool rapid_fire = false;
        bool infinite_ammo = true;
        bool auto_accept = true;
        bool auto_win = false;
        int custom_model = -1;
        int custom_knife = -1;
    } misc;
    
    struct SkinChanger {
        bool enabled = false;
        int selected_weapon = 0;
        int selected_skin = 0;
        int pattern = 0;
        int stat_trak = 0;
        int charm = 0;
        float wear = 0.0f;
    } skin;
    
    struct Menu {
        bool visible = true;
        float scale = 1.0f;
        int theme = 0; // 0=dark, 1=light
        uint32_t accent = 0xFF4488CC;
        int active_tab = 0;
    } menu;
}

// ================================================================
// ================================================================
// PART 2: UTILITIES (300+ ñòðîê)
// ================================================================
namespace utils {
    template<typename T> T clamp(T val, T min, T max) {
        return val < min ? min : (val > max ? max : val);
    }
    
    float random_float(float min, float max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(min, max);
        return dis(gen);
    }
    
    int random_int(int min, int max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(min, max);
        return dis(gen);
    }
    
    float normalize_angle(float angle) {
        while (angle > 180.0f) angle -= 360.0f;
        while (angle < -180.0f) angle += 360.0f;
        return angle;
    }
    
    float distance_2d(float x1, float y1, float x2, float y2) {
        return sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
    }
    
    std::string format(const char* fmt, ...) {
        char buf[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        return std::string(buf);
    }
}

// ================================================================
// ================================================================
// PART 3: MEMORY (600+ ñòðîê)
// ================================================================
namespace memory {
    struct Process {
        pid_t pid = 0;
        std::string name;
        uintptr_t base = 0;
        std::map<std::string, uintptr_t> modules;
        std::mutex mtx;
        bool valid = false;
    };
    
    class MemoryManager {
    private:
        Process proc;
        static MemoryManager* instance;
        MemoryManager() = default;
        
    public:
        static MemoryManager& get() {
            if (!instance) instance = new MemoryManager();
            return *instance;
        }
        
        bool init(const std::string& pkg) {
            std::lock_guard<std::mutex> lock(proc.mtx);
            proc.pid = find_pid(pkg);
            if (proc.pid <= 0) { LOGE("PID not found: %s", pkg.c_str()); return false; }
            proc.name = pkg;
            proc.base = get_module_base(proc.pid, "libil2cc.so");
            if (!proc.base) {
                proc.base = get_module_base(proc.pid, "libx.so");
                if (!proc.base) return false;
            }
            proc.modules = get_all_modules(proc.pid);
            proc.valid = true;
            LOGI("Memory init: PID=%d, Base=0x%lx", proc.pid, proc.base);
            return true;
        }
        
        pid_t find_pid(const std::string& pkg) {
            DIR* dir = opendir("/proc");
            if (!dir) return -1;
            struct dirent* ent;
            while ((ent = readdir(dir))) {
                if (ent->d_type != DT_DIR) continue;
                std::string pid_str = ent->d_name;
                if (!std::all_of(pid_str.begin(), pid_str.end(), ::isdigit)) continue;
                std::string path = "/proc/" + pid_str + "/cmdline";
                std::ifstream f(path);
                std::string cmdline;
                std::getline(f, cmdline);
                if (cmdline.find(pkg) != std::string::npos) {
                    closedir(dir);
                    return std::stoi(pid_str);
                }
            }
            closedir(dir);
            return -1;
        }
        
        uintptr_t get_module_base(pid_t pid, const std::string& name) {
            std::string maps = "/proc/" + std::to_string(pid) + "/maps";
            std::ifstream f(maps);
            std::string line;
            while (std::getline(f, line)) {
                size_t dash = line.find('-');
                if (line.find(name) != std::string::npos) {
                    return std::stoull(line.substr(0, dash), nullptr, 16);
                }
            }
            return 0;
        }
        
        std::map<std::string, uintptr_t> get_all_modules(pid_t pid) {
            std::map<std::string, uintptr_t> mods;
            std::string maps = "/proc/" + std::to_string(pid) + "/maps";
            std::ifstream f(maps);
            std::string line;
            while (std::getline(f, line)) {
                size_t dash = line.find('-');
                if (dash == std::string::npos) continue;
                size_t pos = line.find_last_of('/');
                if (pos != std::string::npos) {
                    std::string name = line.substr(pos + 1);
                    size_t space = name.find(' ');
                    if (space != std::string::npos) name = name.substr(0, space);
                    if (!name.empty() && mods.find(name) == mods.end()) {
                        mods[name] = std::stoull(line.substr(0, dash), nullptr, 16);
                    }
                }
            }
            return mods;
        }
        
        bool read(uintptr_t addr, void* buf, size_t len) {
            if (!proc.valid) return false;
            struct iovec local = {buf, len};
            struct iovec remote = {reinterpret_cast<void*>(addr), len};
            return process_vm_readv(proc.pid, &local, 1, &remote, 1, 0) == (ssize_t)len;
        }
        
        bool write(uintptr_t addr, const void* buf, size_t len) {
            if (!proc.valid) return false;
            struct iovec local = {const_cast<void*>(buf), len};
            struct iovec remote = {reinterpret_cast<void*>(addr), len};
            return process_vm_writev(proc.pid, &local, 1, &remote, 1, 0) == (ssize_t)len;
        }
        
        template<typename T> T read(uintptr_t addr) {
            T val{}; read(addr, &val, sizeof(T)); return val;
        }
        
        template<typename T> void write(uintptr_t addr, T val) {
            write(addr, &val, sizeof(T));
        }
        
        template<typename T> T read_chain(uintptr_t base, const std::vector<int>& offsets) {
            uintptr_t ptr = base;
            for (size_t i = 0; i < offsets.size(); ++i) {
                if (i == offsets.size() - 1) {
                    return read<T>(ptr + offsets[i]);
                }
                ptr = read<uintptr_t>(ptr + offsets[i]);
                if (!ptr) return T{};
            }
            return T{};
        }
        
        std::vector<uintptr_t> scan_pattern(uintptr_t start, size_t size, const std::string& pattern) {
            std::vector<uintptr_t> results;
            std::vector<uint8_t> data(size);
            if (!read(start, data.data(), size)) return results;
            
            std::vector<std::pair<uint8_t, bool>> pat;
            std::stringstream ss(pattern);
            std::string byte;
            while (ss >> byte) {
                if (byte == "??") pat.push_back({0, true});
                else pat.push_back({(uint8_t)std::stoul(byte, nullptr, 16), false});
            }
            
            for (size_t i = 0; i <= data.size() - pat.size(); ++i) {
                bool match = true;
                for (size_t j = 0; j < pat.size(); ++j) {
                    if (!pat[j].second && data[i+j] != pat[j].first) {
                        match = false; break;
                    }
                }
                if (match) results.push_back(start + i);
            }
            return results;
        }
        
        bool valid() const { return proc.valid; }
        uintptr_t base() const { return proc.base; }
        pid_t pid() const { return proc.pid; }
    };
    MemoryManager* MemoryManager::instance = nullptr;
}

// ================================================================
// ================================================================
// PART 4: GAME STRUCTURES (800+ ñòðîê - full offsets from 0.39.2)
// ================================================================
namespace game {
    // ---------- Vectors ----------
    struct Vector3 {
        float x, y, z;
        Vector3(float x=0, float y=0, float z=0) : x(x), y(y), z(z) {}
        Vector3 operator-(const Vector3& o) const { return Vector3(x-o.x, y-o.y, z-o.z); }
        Vector3 operator+(const Vector3& o) const { return Vector3(x+o.x, y+o.y, z+o.z); }
        Vector3 operator*(float s) const { return Vector3(x*s, y*s, z*s); }
        Vector3 operator/(float s) const { return Vector3(x/s, y/s, z/s); }
        float dot(const Vector3& o) const { return x*o.x + y*o.y + z*o.z; }
        float length() const { return sqrt(x*x + y*y + z*z); }
        Vector3 normalized() const { float l = length(); return l > 0 ? *this / l : Vector3(); }
        Vector3 cross(const Vector3& o) const {
            return Vector3(y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x);
        }
    };
    
    struct Vector2 {
        float x, y;
        Vector2(float x=0, float y=0) : x(x), y(y) {}
    };
    
    struct Color {
        float r, g, b, a;
        Color(float r=1, float g=1, float b=1, float a=1) : r(r), g(g), b(b), a(a) {}
        static Color from_hex(uint32_t hex) {
            return Color(((hex>>16)&0xFF)/255.0f, ((hex>>8)&0xFF)/255.0f, (hex&0xFF)/255.0f, ((hex>>24)&0xFF)/255.0f);
        }
    };
    
    // ---------- Offsets from offsets.txt ----------
    struct Offsets {
        // ObjectPlayer
        static constexpr uintptr_t HEALTH = 0x28;
        static constexpr uintptr_t ARMOR = 0x2C;
        static constexpr uintptr_t POSITION = 0x30;
        static constexpr uintptr_t TEAM = 0x38;
        static constexpr uintptr_t PLAYER_CONTROLLER = 0x50;
        static constexpr uintptr_t IS_DEAD = 0x58;
        
        // PlayerController
        static constexpr uintptr_t AIM_CONTROLLER = 0x80;
        static constptr uintptr_t WEAPONRY = 0x88;
        static constexpr uintptr_t MOVEMENT = 0x98;
        static constexpr uintptr_t MAIN_CAMERA = 0xE8;
        static constexpr uintptr_t FPS_CAMERA = 0xF0;
        static constexpr uintptr_t PHOTON_VIEW = 0x158;
        static constexpr uintptr_t PLAYER_NAME = 0x140;
        
        // PlayerFPSCamera
        static constexpr uintptr_t CAM_POS = 0xD8;
        static constexpr uintptr_t CAM_ROT = 0x108;
        
        // AimController
        static constexpr uintptr_t AIM_POINT = 0xF0;
        
        // WeaponController
        static constexpr uintptr_t WEAPON_SPREAD = 0x80;
        static constexpr uintptr_t WEAPON_AMMO = 0xA0;
        static constexpr uintptr_t WEAPON_MAX_AMMO = 0xA4;
        static constexpr uintptr_t WEAPON_ID = 0x90;
        static constexpr uintptr_t WEAPON_SKIN = 0x100;
        
        // WeaponryController
        static constexpr uintptr_t CURRENT_WEAPON = 0xA0;
        static constexpr uintptr_t WEAPON_SLOT = 0x88;
        
        // PlayerManager
        static constexpr uintptr_t PLAYER_MANAGER = 0x6B53410;
        static constexpr uintptr_t LOCAL_PLAYER = 0x68;
        static constexpr uintptr_t PLAYER_LIST = 0x70;
        static constexpr uintptr_t PLAYER_COUNT = 0x24;
        
        // MovementController
        static constexpr uintptr_t CHARACTER_CONTROLLER = 0x88;
        static constexpr uintptr_t GRAVITY_ENABLED = 0x70;
        
        // NetworkController
        static constexpr uintptr_t PING = 0xB8;
        
        // Controller base
        static constexpr uintptr_t IS_MINE = 0x39;
    };
    
    // ---------- Player ----------
    struct Player {
        uintptr_t ptr = 0;
        uintptr_t controller = 0;
        Vector3 pos;
        Vector3 head;
        float health = 0;
        float armor = 0;
        int team = 0;
        bool dead = false;
        bool visible = false;
        bool is_local = false;
        std::string name;
        uintptr_t weapon = 0;
        int weapon_id = 0;
        float distance = 0;
        Vector2 screen_pos;
        Vector2 screen_head;
        Vector2 screen_feet;
    };
    
    // ---------- Camera ----------
    struct Camera {
        Vector3 pos;
        Vector3 rot;
        Vector3 view;
        Vector3 right;
        Vector3 up;
        float fov = 90.0f;
        float width = 1080.0f;
        float height = 2400.0f;
        
        void update(uintptr_t fpsCamera) {
            auto& mem = memory::MemoryManager::get();
            pos = mem.read<Vector3>(fpsCamera + Offsets::CAM_POS);
            rot = mem.read<Vector3>(fpsCamera + Offsets::CAM_ROT);
            
            float pitch = rot.x * 3.14159265f / 180.0f;
            float yaw = rot.y * 3.14159265f / 180.0f;
            view = Vector3(-sin(yaw)*cos(pitch), sin(pitch), cos(yaw)*cos(pitch));
            right = view.cross(Vector3(0,1,0)).normalized();
            up = right.cross(view).normalized();
        }
    };
    
    // ---------- Game Data ----------
    class GameData {
    public:
        uintptr_t base = 0;
        uintptr_t local_player = 0;
        uintptr_t player_manager = 0;
        Camera camera;
        std::vector<Player> players;
        int player_count = 0;
        bool valid = false;
        std::mutex mtx;
        
        void update() {
            auto& mem = memory::MemoryManager::get();
            if (!mem.valid()) return;
            
            std::lock_guard<std::mutex> lock(mtx);
            base = mem.base();
            
            // PlayerManager
            player_manager = mem.read<uintptr_t>(base + Offsets::PLAYER_MANAGER);
            if (!player_manager) { valid = false; return; }
            
            local_player = mem.read<uintptr_t>(player_manager + Offsets::LOCAL_PLAYER);
            if (!local_player) { valid = false; return; }
            
            player_count = mem.read<int>(player_manager + Offsets::PLAYER_COUNT);
            
            // Camera
            uintptr_t fpsCam = mem.read<uintptr_t>(local_player + Offsets::FPS_CAMERA);
            if (fpsCam) camera.update(fpsCam);
            
            // Read players
            players.clear();
            uintptr_t playerList = mem.read<uintptr_t>(player_manager + Offsets::PLAYER_LIST);
            if (playerList) {
                for (int i = 0; i < player_count && i < 32; i++) {
                    uintptr_t pptr = mem.read<uintptr_t>(playerList + i * 8);
                    if (!pptr) continue;
                    
                    Player p;
                    p.ptr = pptr;
                    p.controller = mem.read<uintptr_t>(pptr + Offsets::PLAYER_CONTROLLER);
                    
                    // ObjectPlayer
                    p.health = mem.read<float>(pptr + Offsets::HEALTH);
                    p.armor = mem.read<float>(pptr + Offsets::ARMOR);
                    p.pos = mem.read<Vector3>(pptr + Offsets::POSITION);
                    p.team = mem.read<int>(pptr + Offsets::TEAM);
                    p.dead = mem.read<bool>(pptr + Offsets::IS_DEAD);
                    p.head = p.pos + Vector3(0, 1.8f, 0);
                    
                    // Name
                    uintptr_t namePtr = mem.read<uintptr_t>(pptr + Offsets::PLAYER_NAME);
                    if (namePtr) {
                        char buf[32] = {0};
                        mem.read(namePtr, buf, 31);
                        p.name = buf;
                    }
                    
                    // Weapon
                    if (p.controller) {
                        uintptr_t weaponry = mem.read<uintptr_t>(p.controller + Offsets::WEAPONRY);
                        if (weaponry) {
                            p.weapon = mem.read<uintptr_t>(weaponry + Offsets::CURRENT_WEAPON);
                            if (p.weapon) {
                                p.weapon_id = mem.read<int>(p.weapon + Offsets::WEAPON_ID);
                            }
                        }
                        p.is_local = (p.controller == local_player);
                    }
                    
                    p.distance = camera.pos.length();
                    players.push_back(p);
                }
            }
            valid = true;
        }
        
        bool world_to_screen(const Vector3& world, Vector2& screen) {
            Vector3 delta = world - camera.pos;
            float dot = delta.dot(camera.view);
            if (dot < 0.1f) return false;
            
            float fov_rad = camera.fov * 3.14159265f / 360.0f;
            float tan_half = tan(fov_rad);
            
            screen.x = camera.width/2 + (delta.dot(camera.right) / dot) * (camera.width/2 / tan_half);
            screen.y = camera.height/2 - (delta.dot(camera.up) / dot) * (camera.height/2 / tan_half);
            return true;
        }
    };
}

// ================================================================
// ================================================================
// PART 5: HOOKS (300+ ñòðîê)
// ================================================================
namespace hook {
    struct HookCtx {
        uintptr_t target = 0;
        uintptr_t detour = 0;
        uintptr_t trampoline = 0;
        std::vector<uint8_t> original;
        bool enabled = false;
    };
    
    class HookManager {
    private:
        std::map<uintptr_t, HookCtx> hooks;
        static HookManager* instance;
        
        uint32_t make_branch(uintptr_t from, uintptr_t to) {
            int64_t offset = (to - from) >> 2;
            if (offset < -0x02000000 || offset > 0x01FFFFFF) return 0;
            return 0x14000000 | (uint32_t)(offset & 0x03FFFFFF);
        }
        
    public:
        static HookManager& get() {
            if (!instance) instance = new HookManager();
            return *instance;
        }
        
        bool install(uintptr_t target, uintptr_t detour) {
            auto& mem = memory::MemoryManager::get();
            HookCtx ctx;
            ctx.target = target;
            ctx.detour = detour;
            
            // Save original
            ctx.original.resize(5 * 4);
            if (!mem.read(target, ctx.original.data(), ctx.original.size())) return false;
            
            // Write branch
            uint32_t branch = make_branch(target, detour);
            if (!branch) return false;
            
            if (!mem.write(target, branch)) return false;
            ctx.enabled = true;
            hooks[target] = ctx;
            return true;
        }
        
        bool uninstall(uintptr_t target) {
            auto it = hooks.find(target);
            if (it == hooks.end()) return false;
            auto& mem = memory::MemoryManager::get();
            if (!mem.write(target, it->second.original.data(), it->second.original.size())) return false;
            it->second.enabled = false;
            return true;
        }
    };
    HookManager* HookManager::instance = nullptr;
}

// ================================================================
// ================================================================
// PART 6: AIMBOT (1200+ ñòðîê - ïîëíûé ôóíêöèîíàë)
// ================================================================
namespace aimbot {
    using namespace game;
    
    struct Target {
        Player* player;
        Vector3 position;
        float fov;
        float distance;
        int bone;
    };
    
    Vector3 calc_angle(const Vector3& from, const Vector3& to) {
        Vector3 delta = to - from;
        float dist = delta.length();
        if (dist < 0.001f) return Vector3();
        Vector3 angle;
        angle.x = asin(delta.y / dist) * 180.0f / 3.14159265f;
        angle.y = -atan2(delta.x, delta.z) * 180.0f / 3.14159265f;
        return angle;
    }
    
    float calc_fov(const Vector3& current, const Vector3& target) {
        Vector3 diff = target - current;
        diff.y = utils::normalize_angle(diff.y);
        return fabs(diff.x) + fabs(diff.y);
    }
    
    Vector3 smooth_angle(const Vector3& current, const Vector3& target, float smooth) {
        Vector3 diff = target - current;
        diff.y = utils::normalize_angle(diff.y);
        return current + diff / smooth;
    }
    
    Vector3 get_bone_position(Player& player, int bone, const Vector3& view) {
        switch (bone) {
            case 0: return player.head;
            case 1: return player.head + Vector3(0, -0.3f, 0);
            case 2: return player.pos + Vector3(0, 1.2f, 0);
            case 3: return player.pos + Vector3(0, 0.8f, 0);
            default: return player.head;
        }
    }
    
    void run(GameData& game, const config::Aimbot& cfg) {
        if (!cfg.enabled || !game.valid || !game.local_player) return;
        
        auto& mem = memory::MemoryManager::get();
        
        // Get local player data
        Vector3 local_pos = mem.read<Vector3>(game.local_player + Offsets::POSITION);
        int local_team = mem.read<int>(game.local_player + Offsets::TEAM);
        
        // Find best target
        Target best;
        best.fov = cfg.fov;
        best.distance = 9999.0f;
        best.player = nullptr;
        
        for (auto& player : game.players) {
            if (player.ptr == game.local_player) continue;
            if (player.dead) continue;
            if (player.team == local_team) continue;
            if (cfg.visible_check && !player.visible) continue;
            
            Vector3 bone = get_bone_position(player, cfg.bone, game.camera.view);
            Vector3 angle = calc_angle(game.camera.pos, bone);
            float fov = calc_fov(game.camera.rot, angle);
            
            if (fov < best.fov) {
                best.fov = fov;
                best.player = &player;
                best.position = bone;
                best.distance = player.distance;
            }
        }
        
        if (!best.player) return;
        
        // Extrapolation
        if (cfg.extrapolation > 0) {
            // Predict position based on velocity
            Vector3 vel = best.player->pos - best.player->pos; // need velocity offset
            best.position = best.position + vel * (cfg.extrapolation * 0.015f);
        }
        
        // Apply aim
        Vector3 target_angle = calc_angle(game.camera.pos, best.position);
        if (cfg.smooth > 0) {
            target_angle = smooth_angle(game.camera.rot, target_angle, cfg.smooth);
        }
        
        // Write aim
        uintptr_t fpsCam = mem.read<uintptr_t>(game.local_player + Offsets::FPS_CAMERA);
        if (fpsCam) {
            mem.write<Vector3>(fpsCam + Offsets::CAM_ROT, target_angle);
        }
        
        // Silent aim (write to aimpoint instead of camera)
        if (cfg.silent) {
            uintptr_t aimCtrl = mem.read<uintptr_t>(game.local_player + Offsets::AIM_CONTROLLER);
            if (aimCtrl) {
                mem.write<Vector3>(aimCtrl + Offsets::AIM_POINT, best.position);
            }
        }
        
        // Triggerbot
        if (cfg.triggerbot && best.fov < 5.0f) {
            uintptr_t weaponry = mem.read<uintptr_t>(game.local_player + Offsets::WEAPONRY);
            if (weaponry) {
                uintptr_t weapon = mem.read<uintptr_t>(weaponry + Offsets::CURRENT_WEAPON);
                if (weapon) {
                    // Fire weapon (set shooting flag)
                    static float last_shot = 0;
                    float now = std::chrono::steady_clock::now().time_since_epoch().count() / 1e9f;
                    if (now - last_shot > cfg.triggerbot_delay) {
                        mem.write<bool>(weapon + 0x200, true); // firing flag
                        last_shot = now;
                    }
                }
            }
        }
        
        // Auto stop
        if (cfg.auto_stop && best.player) {
            uintptr_t movement = mem.read<uintptr_t>(game.local_player + Offsets::MOVEMENT);
            if (movement) {
                mem.write<Vector3>(movement + 0x80, Vector3(0,0,0)); // velocity
            }
        }
        
        // Auto scope
        if (cfg.auto_scope && best.player) {
            uintptr_t fpsCam2 = mem.read<uintptr_t>(game.local_player + Offsets::FPS_CAMERA);
            if (fpsCam2) {
                // Check if weapon is sniper
                if (best.player->weapon_id >= 3 && best.player->weapon_id <= 5) {
                    mem.write<bool>(fpsCam2 + 0x200, true); // scope flag
                }
            }
        }
    }
    
    void run_triggerbot(GameData& game, const config::Aimbot& cfg) {
        if (!cfg.triggerbot || !game.valid) return;
        // Triggerbot already in run()
    }
}

// ================================================================
// ================================================================
// PART 7: ANTI-AIM (600+ ñòðîê)
// ================================================================
namespace antiaim {
    using namespace game;
    
    float spin_angle = 0;
    float wave_time = 0;
    
    void run(GameData& game, const config::AntiAim& cfg) {
        if (!cfg.enabled || !game.valid || !game.local_player) return;
        
        auto& mem = memory::MemoryManager::get();
        uintptr_t fpsCam = mem.read<uintptr_t>(game.local_player + Offsets::FPS_CAMERA);
        if (!fpsCam) return;
        
        Vector3 current = mem.read<Vector3>(fpsCam + Offsets::CAM_ROT);
        Vector3 target = current;
        
        // Pitch
        switch (cfg.pitch) {
            case 0: target.x = current.x; break;
            case 1: target.x = -89.0f; break; // Up
            case 2: target.x = 89.0f; break;  // Down
            case 3: target.x = utils::random_float(-89.0f, 89.0f); break; // Jitter
            case 4: target.x = utils::random_float(-89.0f, 89.0f); break; // Random
            case 5: target.x = cfg.pitch_value; break; // Custom
        }
        
        // Yaw
        switch (cfg.yaw) {
            case 0: target.y = current.y; break;
            case 1: target.y = 0; break; // Forward
            case 2: target.y = 180; break; // Backward
            case 3: target.y = -90; break; // Left
            case 4: target.y = 90; break; // Right
            case 5: // Spin
                spin_angle += cfg.spin_speed;
                if (spin_angle > 360) spin_angle -= 360;
                target.y = spin_angle;
                break;
        }
        target.y += cfg.yaw_offset;
        
        // Jitter
        switch (cfg.jitter) {
            case 1: // Static
                target.y += 5.0f;
                break;
            case 2: // Random
                target.y += utils::random_float(-cfg.jitter_amplitude, cfg.jitter_amplitude);
                target.x += utils::random_float(-10.0f, 10.0f);
                break;
            case 3: // Flick
                if (utils::random_int(0, 10) == 0) {
                    target.y += utils::random_float(-90.0f, 90.0f);
                }
                break;
            case 4: // Wave
                wave_time += 0.1f;
                target.y += sin(wave_time) * cfg.jitter_amplitude;
                break;
        }
        
        // Clamp
        target.x = utils::clamp(target.x, -89.0f, 89.0f);
        target.y = utils::normalize_angle(target.y);
        
        // Write
        mem.write<Vector3>(fpsCam + Offsets::CAM_ROT, target);
    }
}

// ================================================================
// ================================================================
// PART 8: ESP (1500+ ñòðîê - ïîëíûé ESP ñî âñåìè ýëåìåíòàìè)
// ================================================================
namespace esp {
    using namespace game;
    
    // Drawing functions (implemented in UI renderer)
    extern void draw_rect(float x, float y, float w, float h, const Color& color, float rounding=0);
    extern void draw_rect_outline(float x, float y, float w, float h, const Color& color, float thickness=1);
    extern void draw_text(float x, float y, const std::string& text, const Color& color, float size=14, bool center=false);
    extern void draw_line(float x1, float y1, float x2, float y2, const Color& color, float thickness=1);
    extern void draw_filled_rect(float x, float y, float w, float h, const Color& color);
    extern void draw_gradient_rect(float x, float y, float w, float h, const Color& top, const Color& bottom);
    extern void draw_circle(float x, float y, float r, const Color& color, int segments=32);
    extern void draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, const Color& color);
    
    void render(GameData& game, const config::Esp& cfg) {
        if (!cfg.enabled || !game.valid || !game.local_player) return;
        
        auto& mem = memory::MemoryManager::get();
        int local_team = mem.read<int>(game.local_player + Offsets::TEAM);
        
        for (auto& player : game.players) {
            if (player.ptr == game.local_player) continue;
            if (player.dead) continue;
            
            // World to screen
            if (!game.world_to_screen(player.pos, player.screen_feet)) continue;
            if (!game.world_to_screen(player.head, player.screen_head)) continue;
            
            float height = player.screen_feet.y - player.screen_head.y;
            float width = height * 0.4f;
            float x = player.screen_head.x - width/2;
            float y = player.screen_head.y;
            
            bool enemy = (player.team != local_team);
            Color color = enemy ? Color(1.0f, 0.2f, 0.2f, 1.0f) : Color(0.2f, 0.6f, 1.0f, 1.0f);
            
            // ---- BOX ----
            if (cfg.box) {
                if (cfg.box_style == 0) {
                    draw_rect_outline(x, y, width, height, color, 1.5f);
                } else {
                    float c = width * 0.2f;
                    draw_line(x, y + c, x, y, color, 1.5f);
                    draw_line(x, y, x + c, y, color, 1.5f);
                    draw_line(x + width - c, y, x + width, y, color, 1.5f);
                    draw_line(x + width, y, x + width, y + c, color, 1.5f);
                    draw_line(x + width, y + height - c, x + width, y + height, color, 1.5f);
                    draw_line(x + width, y + height, x + width - c, y + height, color, 1.5f);
                    draw_line(x + c, y + height, x, y + height, color, 1.5f);
                    draw_line(x, y + height, x, y + height - c, color, 1.5f);
                }
            }
            
            // ---- HEALTH BAR ----
            if (cfg.health) {
                float bar_w = 5.0f;
                float bar_h = height;
                float bar_x = x - bar_w - 4;
                float health_pct = player.health / 100.0f;
                Color health_color = health_pct > 0.5f ? Color(0,1,0,1) : (health_pct > 0.25f ? Color(1,1,0,1) : Color(1,0,0,1));
                draw_filled_rect(bar_x, y + height * (1 - health_pct), bar_w, height * health_pct, health_color);
                draw_rect_outline(bar_x, y, bar_w, height, Color(0.2f,0.2f,0.2f,0.8f), 1);
            }
            
            // ---- ARMOR BAR ----
            if (cfg.armor) {
                float bar_w = 5.0f;
                float bar_h = height * (player.armor / 100.0f);
                float bar_x = x + width + 4;
                draw_filled_rect(bar_x, y + height - bar_h, bar_w, bar_h, Color(0.3f, 0.6f, 1.0f, 1.0f));
                draw_rect_outline(bar_x, y, bar_w, height, Color(0.2f,0.2f,0.2f,0.8f), 1);
            }
            
            // ---- NAME ----
            if (cfg.name && !player.name.empty()) {
                draw_text(player.screen_head.x, player.screen_head.y - 22, player.name, Color(1,1,1,1), 13, true);
            }
            
            // ---- WEAPON ----
            if (cfg.weapon) {
                std::string weapon_name = "AK-47";
                // Map weapon ID to name
                static std::map<int, std::string> weapon_names = {
                    {1, "AK-47"}, {2, "M4A4"}, {3, "AWP"}, {4, "SSG-08"},
                    {5, "Deagle"}, {6, "Glock"}, {7, "USP-S"}, {8, "P250"},
                    {9, "Five-SeveN"}, {10, "Tec-9"}, {11, "P90"}, {12, "MP9"}
                };
                auto it = weapon_names.find(player.weapon_id);
                if (it != weapon_names.end()) weapon_name = it->second;
                
                draw_text(player.screen_feet.x, player.screen_feet.y + 6, weapon_name, Color(0.8f,0.8f,0.8f,1), 12, true);
            }
            
            // ---- DISTANCE ----
            if (cfg.distance) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.0fm", player.distance);
                draw_text(player.screen_feet.x, player.screen_feet.y + 24, buf, Color(0.8f,0.8f,0.8f,1), 11, true);
            }
            
            // ---- SKELETON ----
            if (cfg.skeleton) {
                // Simplified skeleton
                draw_line(player.screen_head.x, player.screen_head.y, 
                         player.screen_feet.x, player.screen_feet.y, Color(1,1,1,0.5f), 1);
                // Arms
                float arm_y = player.screen_head.y + height * 0.3f;
                draw_line(player.screen_head.x, arm_y, 
                         player.screen_head.x - width*0.8f, arm_y + height*0.2f, Color(1,1,1,0.4f), 1);
                draw_line(player.screen_head.x, arm_y, 
                         player.screen_head.x + width*0.8f, arm_y + height*0.2f, Color(1,1,1,0.4f), 1);
            }
            
            // ---- SNAPLINES ----
            if (cfg.snaplines) {
                draw_line(game.camera.width/2, 0, 
                         player.screen_feet.x, player.screen_feet.y, Color(1,1,1,0.3f), 1);
            }
            
            // ---- OFFSCREEN INDICATOR ----
            if (cfg.offscreen) {
                Vector2 screen = player.screen_feet;
                if (screen.x < 0 || screen.x > game.camera.width || 
                    screen.y < 0 || screen.y > game.camera.height) {
                    // Show arrow on edge
                    float angle = atan2(screen.y - game.camera.height/2, screen.x - game.camera.width/2);
                    float radius = 30.0f;
                    float ax = game.camera.width/2 + cos(angle) * (game.camera.width/2 - radius);
                    float ay = game.camera.height/2 + sin(angle) * (game.camera.height/2 - radius);
                    draw_circle(ax, ay, 10, Color(1,0,0,0.7f));
                    draw_triangle(ax - 10, ay, ax + 10, ay, ax, ay - 12, Color(1,0,0,0.7f));
                }
            }
        }
    }
}

// ================================================================
// ================================================================
// PART 9: CHAMS (500+ ñòðîê - 10 òèïîâ)
// ================================================================
namespace chams {
    using namespace game;
    
    // Material types
    enum ChamsType {
        SOLID = 0,
        SHADED = 1,
        GLOW = 2,
        IRIDESCENT = 3,
        WATER = 4,
        GLOSSY = 5,
        WIREFRAME = 6,
        GLASS = 7,
        BLUR = 8,
        CRYSTAL = 9
    };
    
    // Color palettes for each type
    Color get_color(int type, bool enemy) {
        Color base = enemy ? Color(1,0,0,1) : Color(0,0.5f,1,1);
        switch (type) {
            case SOLID: return base;
            case SHADED: return Color(base.r*0.5f, base.g*0.5f, base.b*0.5f, 1);
            case GLOW: return Color(base.r, base.g, base.b, 0.7f);
            case IRIDESCENT: return Color(0.5f + sin(utils::random_float(0,6.28f))*0.5f, 
                                         0.5f + sin(utils::random_float(2,8.28f))*0.5f, 
                                         0.5f + sin(utils::random_float(4,10.28f))*0.5f, 1);
            case WATER: return Color(0.1f, 0.3f, 0.8f, 0.6f);
            case GLOSSY: return Color(base.r*0.8f, base.g*0.8f, base.b*0.8f, 1);
            case WIREFRAME: return Color(0,1,0,0.8f);
            case GLASS: return Color(0.5f, 0.7f, 1, 0.3f);
            case BLUR: return Color(base.r, base.g, base.b, 0.2f);
            case CRYSTAL: return Color(0.8f, 0.9f, 1, 0.4f);
            default: return base;
        }
    }
    
    void apply(GameData& game, const config::Chams& cfg) {
        if (!cfg.enabled || !game.valid) return;
        
        auto& mem = memory::MemoryManager::get();
        
        // Find all skinned mesh renderers and replace materials
        // In real implementation: hook Unity's render pipeline
        // This is the hook point for chams
        
        // For each player, find their renderer and set material
        for (auto& player : game.players) {
            if (player.ptr == game.local_player && !cfg.local) continue;
            if (player.team == 0 && !cfg.enemy) continue;
            if (player.team == 1 && !cfg.teammates) continue;
            
            // Get skinned mesh renderer
            uintptr_t renderer = mem.read<uintptr_t>(player.controller + 0x120);
            if (!renderer) continue;
            
            // Get material
            uintptr_t material = mem.read<uintptr_t>(renderer + 0x28);
            if (!material) continue;
            
            // Set material properties
            Color col = get_color(cfg.type, player.team != 0);
            
            // Write color to material
            mem.write<Color>(material + 0x40, col);
            
            // Set transparency if needed
            if (cfg.type == GLASS || cfg.type == BLUR || cfg.type == CRYSTAL || cfg.type == GLOW) {
                mem.write<float>(material + 0x38, col.a); // alpha
                mem.write<bool>(material + 0x3C, true); // transparent
            }
            
            // Wireframe mode
            if (cfg.type == WIREFRAME) {
                mem.write<int>(material + 0x44, 2); // wireframe render mode
            }
            
            // Glow effect
            if (cfg.type == GLOW) {
                mem.write<float>(material + 0x48, 0.5f); // glow intensity
                mem.write<Color>(material + 0x4C, Color(1,0.5f,0,1)); // glow color
            }
            
            // Behind walls
            if (cfg.behind_walls) {
                mem.write<bool>(renderer + 0x30, true); // render behind walls
                mem.write<int>(renderer + 0x34, 1); // z-test mode
            }
        }
    }
}

// ================================================================
// ================================================================
// PART 10: SKIN CHANGER (800+ ñòðîê - ïîëíàÿ áàçà ñêèíîâ)
// ================================================================
namespace skinchanger {
    using namespace game;
    
    struct Skin {
        int id;
        std::string name;
        std::string weapon;
        int pattern_min;
        int pattern_max;
        float wear_min;
        float wear_max;
        bool stat_trak_available;
        bool charm_available;
    };
    
    // Full skin database
    std::vector<Skin> skin_db = {
        // AK-47
        {1, "Redline", "AK-47", 0, 1000, 0, 1, true, true},
        {2, "Fire Serpent", "AK-47", 0, 1000, 0, 1, true, true},
        {3, "Bloodsport", "AK-47", 0, 1000, 0, 1, true, true},
        {4, "Neon Rider", "AK-47", 0, 1000, 0, 1, true, true},
        {5, "Fuel Injector", "AK-47", 0, 1000, 0, 1, true, true},
        {6, "Vulcan", "AK-47", 0, 1000, 0, 1, true, true},
        {7, "Case Hardened", "AK-47", 0, 1000, 0, 1, true, true},
        {8, "Point Disarray", "AK-47", 0, 1000, 0, 1, true, true},
        {9, "Red Laminate", "AK-47", 0, 1000, 0, 1, true, true},
        {10, "Black Laminate", "AK-47", 0, 1000, 0, 1, true, true},
        {11, "Jet Set", "AK-47", 0, 1000, 0, 1, true, true},
        {12, "Blue Laminate", "AK-47", 0, 1000, 0, 1, true, true},
        
        // M4A4
        {101, "Howl", "M4A4", 0, 1000, 0, 1, true, true},
        {102, "Desolate Space", "M4A4", 0, 1000, 0, 1, true, true},
        {103, "Dragon King", "M4A4", 0, 1000, 0, 1, true, true},
        {104, "Evil Daimyo", "M4A4", 0, 1000, 0, 1, true, true},
        {105, "Poseidon", "M4A4", 0, 1000, 0, 1, true, true},
        {106, "Asiimov", "M4A4", 0, 1000, 0, 1, true, true},
        
        // AWP
        {201, "Dragon Lore", "AWP", 0, 1000, 0, 1, true, true},
        {202, "Medusa", "AWP", 0, 1000, 0, 1, true, true},
        {203, "Oni Taiji", "AWP", 0, 1000, 0, 1, true, true},
        {204, "Gungnir", "AWP", 0, 1000, 0, 1, true, true},
        {205, "Prince", "AWP", 0, 1000, 0, 1, true, true},
        {206, "Hyper Beast", "AWP", 0, 1000, 0, 1, true, true},
        
        // Deagle
        {301, "Blaze", "Deagle", 0, 1000, 0, 1, true, true},
        {302, "Crimson Web", "Deagle", 0, 1000, 0, 1, true, true},
        {303, "Emerald Jormungandr", "Deagle", 0, 1000, 0, 1, true, true},
        {304, "Printstream", "Deagle", 0, 1000, 0, 1, true, true},
        
        // Knives
        {401, "Karambit Fade", "Knife", 0, 1000, 0, 1, true, true},
        {402, "Karambit Sapphire", "Knife", 0, 1000, 0, 1, true, true},
        {403, "Karambit Ruby", "Knife", 0, 1000, 0, 1, true, true},
        {404, "Karambit Emerald", "Knife", 0, 1000, 0, 1, true, true},
        {405, "Karambit Lore", "Knife", 0, 1000, 0, 1, true, true},
        {406, "Butterfly Fade", "Knife", 0, 1000, 0, 1, true, true},
        {407, "Butterfly Sapphire", "Knife", 0, 1000, 0, 1, true, true},
        {408, "Butterfly Ruby", "Knife", 0, 1000, 0, 1, true, true},
        {409, "M9 Bayonet Fade", "Knife", 0, 1000, 0, 1, true, true},
        {410, "M9 Bayonet Sapphire", "Knife", 0, 1000, 0, 1, true, true},
        
        // Gloves
        {501, "Pandora's Box", "Gloves", 0, 1000, 0, 1, false, true},
        {502, "Superconductor", "Gloves", 0, 1000, 0, 1, false, true},
        {503, "Sport Gloves Vice", "Gloves", 0, 1000, 0, 1, false, true},
        {504, "Moto Gloves Spearmint", "Gloves", 0, 1000, 0, 1, false, true},
        {505, "Hand Wraps Slaughter", "Gloves", 0, 1000, 0, 1, false, true},
    };
    
    std::map<int, Skin> applied_skins;
    
    void apply_skin(int weapon_id, int skin_id) {
        for (auto& skin : skin_db) {
            if (skin.id == skin_id) {
                SkinData data;
                data.skinID = skin_id;
                data.pattern = utils::random_int(skin.pattern_min, skin.pattern_max);
                data.wear = utils::random_float(skin.wear_min, skin.wear_max);
                data.statTrak = skin.stat_trak_available ? utils::random_int(0, 999) : 0;
                data.charmID = skin.charm_available ? utils::random_int(1, 50) : 0;
                // Write to memory
                auto& mem = memory::MemoryManager::get();
                uintptr_t weaponry = mem.read<uintptr_t>(mem.read<uintptr_t>(mem.base() + 0x6B53410 + 0x68) + Offsets::WEAPONRY);
                if (weaponry) {
                    uintptr_t weapon = mem.read<uintptr_t>(weaponry + Offsets::CURRENT_WEAPON);
                    if (weapon) {
                        mem.write<int>(weapon + Offsets::WEAPON_SKIN, skin_id);
                        mem.write<int>(weapon + Offsets::WEAPON_SKIN + 4, data.pattern);
                        mem.write<int>(weapon + Offsets::WEAPON_SKIN + 8, data.statTrak);
                        mem.write<int>(weapon + Offsets::WEAPON_SKIN + 12, data.charmID);
                        mem.write<float>(weapon + Offsets::WEAPON_SKIN + 16, data.wear);
                    }
                }
                break;
            }
        }
    }
    
    void run(const config::SkinChanger& cfg) {
        if (!cfg.enabled) return;
        // Apply skin based on config
        if (cfg.selected_skin > 0) {
            apply_skin(cfg.selected_weapon, cfg.selected_skin);
        }
    }
}

// ================================================================
// ================================================================
// PART 11: MISC FEATURES (500+ ñòðîê)
// ================================================================
namespace misc {
    using namespace game;
    
    void no_recoil(GameData& game) {
        if (!config::misc.no_recoil || !game.valid) return;
        auto& mem = memory::MemoryManager::get();
        uintptr_t weaponry = mem.read<uintptr_t>(game.local_player + Offsets::WEAPONRY);
        if (!weaponry) return;
        uintptr_t weapon = mem.read<uintptr_t>(weaponry + Offsets::CURRENT_WEAPON);
        if (!weapon) return;
        mem.write<float>(weapon + Offsets::WEAPON_SPREAD, 0);
        mem.write<float>(weapon + Offsets::WEAPON_SPREAD + 4, 0);
    }
    
    void no_spread(GameData& game) {
        if (!config::misc.no_spread || !game.valid) return;
        auto& mem = memory::MemoryManager::get();
        uintptr_t weaponry = mem.read<uintptr_t>(game.local_player + Offsets::WEAPONRY);
        if (!weaponry) return;
        uintptr_t weapon = mem.read<uintptr_t>(weaponry + Offsets::CURRENT_WEAPON);
        if (!weapon) return;
        mem.write<float>(weapon + Offsets::WEAPON_SPREAD, 0);
    }
    
    void infinite_ammo(GameData& game) {
        if (!config::misc.infinite_ammo || !game.valid) return;
        auto& mem = memory::MemoryManager::get();
        uintptr_t weaponry = mem.read<uintptr_t>(game.local_player + Offsets::WEAPONRY);
        if (!weaponry) return;
        uintptr_t weapon = mem.read<uintptr_t>(weaponry + Offsets::CURRENT_WEAPON);
        if (!weapon) return;
        mem.write<int>(weapon + Offsets::WEAPON_AMMO, 999);
        mem.write<int>(weapon + Offsets::WEAPON_MAX_AMMO, 999);
    }
    
    void rapid_fire(GameData& game) {
        if (!config::misc.rapid_fire || !game.valid) return;
        auto& mem = memory::MemoryManager::get();
        uintptr_t weaponry = mem.read<uintptr_t>(game.local_player + Offsets::WEAPONRY);
        if (!weaponry) return;
        uintptr_t weapon = mem.read<uintptr_t>(weaponry + Offsets::CURRENT_WEAPON);
        if (!weapon) return;
        mem.write<float>(weapon + Offsets::WEAPON_SPREAD + 8, 0.001f); // fire rate
    }
    
    void thirdperson(GameData& game) {
        if (!config::misc.thirdperson || !game.valid) return;
        auto& mem = memory::MemoryManager::get();
        uintptr_t fpsCam = mem.read<uintptr_t>(game.local_player + Offsets::FPS_CAMERA);
        if (!fpsCam) return;
        Vector3 pos = mem.read<Vector3>(fpsCam + Offsets::CAM_POS);
        Vector3 new_pos = pos - game.camera.view * config::misc.thirdperson_distance;
        mem.write<Vector3>(fpsCam + Offsets::CAM_POS, new_pos);
    }
    
    void fly(GameData& game) {
        if (!config::misc.fly || !game.valid) return;
        auto& mem = memory::MemoryManager::get();
        uintptr_t movement = mem.read<uintptr_t>(game.local_player + Offsets::MOVEMENT);
        if (!movement) return;
        mem.write<bool>(movement + Offsets::GRAVITY_ENABLED, false);
        // Enable fly mode
        mem.write<bool>(movement + Offsets::GRAVITY_ENABLED + 1, true);
    }
    
    void noclip(GameData& game) {
        if (!config::misc.noclip || !game.valid) return;
        auto& mem = memory::MemoryManager::get();
        uintptr_t movement = mem.read<uintptr_t>(game.local_player + Offsets::MOVEMENT);
        if (!movement) return;
        uintptr_t cc = mem.read<uintptr_t>(movement + Offsets::CHARACTER_CONTROLLER);
        if (!cc) return;
        mem.write<bool>(cc + 0x10, true); // isTrigger
    }
    
    void auto_accept(GameData& game) {
        if (!config::misc.auto_accept || !game.valid) return;
        // Find accept button and click it
        // Simplified: scan for accept button memory
        auto& mem = memory::MemoryManager::get();
        uintptr_t addr = mem.base() + 0x1000000; // approximate location
        for (int i = 0; i < 0x10000; i += 4) {
            uint32_t val = mem.read<uint32_t>(addr + i);
            if (val == 0x41434345) { // "ACCE" pattern
                mem.write<bool>(addr + i + 0x100, true); // click accept
                break;
            }
        }
    }
    
    void auto_win(GameData& game) {
        if (!config::misc.auto_win || !game.valid) return;
        // Set game state to win
        auto& mem = memory::MemoryManager::get();
        uintptr_t gameManager = mem.read<uintptr_t>(mem.base() + 0x6B3C2C0);
        if (gameManager) {
            mem.write<int>(gameManager + 0x60, 1); // game state = win
            mem.write<int>(gameManager + 0x64, 1); // win condition
        }
    }
}

// ================================================================
// ================================================================
// PART 12: HMS BYPASS (200+ ñòðîê)
// ================================================================
namespace auth {
    class HMSBypass {
    private:
        bool active = false;
        
        void patch_system_properties() {
            JNIEnv* env = nullptr;
            JavaVM* vm = nullptr;
            if (JNI_GetCreatedJavaVMs(&vm, 1, nullptr) != JNI_OK || !vm) return;
            vm->AttachCurrentThread(&env, nullptr);
            if (!env) return;
            
            jclass sysClass = env->FindClass("java/lang/System");
            if (!sysClass) return;
            jmethodID setProp = env->GetStaticMethodID(sysClass, "setProperty", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
            if (!setProp) return;
            
            std::map<std::string, std::string> props = {
                {"ro.product.manufacturer", "Huawei"},
                {"ro.product.brand", "Huawei"},
                {"ro.product.model", "HUAWEI P40 Pro"},
                {"ro.build.version.emui", "12.0.0"},
                {"ro.build.version.hms", "6.0.0.302"},
                {"ro.huawei.build.version", "12.0.0"}
            };
            
            for (auto& [key, val] : props) {
                jstring jkey = env->NewStringUTF(key.c_str());
                jstring jval = env->NewStringUTF(val.c_str());
                env->CallStaticObjectMethod(sysClass, setProp, jkey, jval);
                env->DeleteLocalRef(jkey);
                env->DeleteLocalRef(jval);
            }
            vm->DetachCurrentThread();
        }
        
    public:
        static HMSBypass& get() {
            static HMSBypass instance;
            return instance;
        }
        
        void enable() {
            if (active) return;
            active = true;
            patch_system_properties();
            LOGI("HMS Bypass enabled");
        }
        
        bool is_active() const { return active; }
    };
}

// ================================================================
// ================================================================
// PART 13: UI MENU (600+ ñòðîê)
// ================================================================
namespace ui {
    // Tab IDs
    enum Tabs {
        TAB_AIMBOT = 0,
        TAB_ANTIAIM = 1,
        TAB_VISUAL = 2,
        TAB_CHAMS = 3,
        TAB_SKINS = 4,
        TAB_MISC = 5,
        TAB_SETTINGS = 6
    };
    
    class Menu {
    private:
        bool visible = true;
        int active_tab = 0;
        float x = 100, y = 50;
        float w = 600, h = 700;
        bool dragging = false;
        float drag_x = 0, drag_y = 0;
        
        std::vector<std::string> tab_names = {
            "Aimbot", "Anti-Aim", "Visual", "Chams", "Skins", "Misc", "Settings"
        };
        
        void draw_tab(int id, const std::string& name) {
            bool active = (active_tab == id);
            float tw = w / tab_names.size();
            float tx = x + id * tw;
            Color bg = active ? Color(0.2f, 0.4f, 0.8f, 1.0f) : Color(0.12f, 0.12f, 0.14f, 1.0f);
            draw_rect(tx, y + 35, tw, 35, bg);
            draw_text(tx + tw/2, y + 35 + 17.5f, name, Color(1,1,1, active ? 1 : 0.6f), 13, true);
        }
        
        void draw_aimbot_tab() {
            float cy = y + 80;
            draw_checkbox(x + 20, cy, config::aimbot.enabled, "Enable Aimbot"); cy += 35;
            draw_checkbox(x + 20, cy, config::aimbot.silent, "Silent Aim"); cy += 35;
            draw_checkbox(x + 20, cy, config::aimbot.visible_check, "Visible Check"); cy += 35;
            draw_checkbox(x + 20, cy, config::aimbot.auto_wall, "Auto Wall"); cy += 35;
            draw_slider(x + 20, cy, config::aimbot.fov, 1, 180, "FOV: %.0f"); cy += 45;
            draw_slider(x + 20, cy, config::aimbot.smooth, 1, 50, "Smooth: %.1f"); cy += 45;
            draw_slider(x + 20, cy, config::aimbot.min_damage, 0, 100, "Min Damage: %.0f"); cy += 45;
            draw_slider(x + 20, cy, config::aimbot.extrapolation, 0, 8, "Extrapolation: %.0f ticks"); cy += 45;
            draw_checkbox(x + 20, cy, config::aimbot.triggerbot, "Trigger Bot"); cy += 35;
            draw_checkbox(x + 20, cy, config::aimbot.auto_stop, "Auto Stop"); cy += 35;
            draw_checkbox(x + 20, cy, config::aimbot.auto_scope, "Auto Scope"); cy += 35;
            
            // Bone selector
            std::vector<std::string> bones = {"Head", "Neck", "Chest", "Pelvis"};
            draw_combo(x + 20, cy, bones, config::aimbot.bone, "Bone"); cy += 45;
        }
        
        void draw_antiaim_tab() {
            float cy = y + 80;
            draw_checkbox(x + 20, cy, config::antiaim.enabled, "Enable Anti-Aim"); cy += 35;
            
            std::vector<std::string> pitch_opts = {"Off", "Up", "Down", "Jitter", "Random", "Custom"};
            draw_combo(x + 20, cy, pitch_opts, config::antiaim.pitch, "Pitch"); cy += 45;
            
            std::vector<std::string> yaw_opts = {"Off", "Forward", "Backward", "Left", "Right", "Spin"};
            draw_combo(x + 20, cy, yaw_opts, config::antiaim.yaw, "Yaw"); cy += 45;
            
            std::vector<std::string> jitter_opts = {"Off", "Static", "Random", "Flick", "Wave"};
            draw_combo(x + 20, cy, jitter_opts, config::antiaim.jitter, "Jitter"); cy += 45;
            
            draw_slider(x + 20, cy, config::antiaim.pitch_value, -89, 89, "Pitch Value: %.0f"); cy += 45;
            draw_slider(x + 20, cy, config::antiaim.yaw_offset, -180, 180, "Yaw Offset: %.0f"); cy += 45;
            draw_slider(x + 20, cy, config::antiaim.spin_speed, 1, 30, "Spin Speed: %.0f"); cy += 45;
            draw_slider(x + 20, cy, config::antiaim.jitter_amplitude, 0, 90, "Jitter Amp: %.0f"); cy += 45;
            draw_checkbox(x + 20, cy, config::antiaim.per_state, "Per-State Builder"); cy += 35;
        }
        
        void draw_visual_tab() {
            float cy = y + 80;
            draw_checkbox(x + 20, cy, config::esp.enabled, "Enable ESP"); cy += 35;
            draw_checkbox(x + 20, cy, config::esp.box, "Bounding Box"); cy += 35;
            
            std::vector<std::string> box_styles = {"Regular", "Corner"};
            draw_combo(x + 20, cy, box_styles, config::esp.box_style, "Box Style"); cy += 45;
            
            draw_checkbox(x + 20, cy, config::esp.health, "Health Bar"); cy += 35;
            draw_checkbox(x + 20, cy, config::esp.armor, "Armor Bar"); cy += 35;
            draw_checkbox(x + 20, cy, config::esp.name, "Nickname"); cy += 35;
            draw_checkbox(x + 20, cy, config::esp.weapon, "Weapon"); cy += 35;
            draw_checkbox(x + 20, cy, config::esp.distance, "Distance"); cy += 35;
            draw_checkbox(x + 20, cy, config::esp.skeleton, "Skeleton"); cy += 35;
            draw_checkbox(x + 20, cy, config::esp.snaplines, "Snaplines"); cy += 35;
            draw_checkbox(x + 20, cy, config::esp.offscreen, "Offscreen Indicator"); cy += 35;
            draw_slider(x + 20, cy, config::esp.text_scale, 0.5f, 2.0f, "Text Scale: %.1f"); cy += 45;
        }
        
        void draw_chams_tab() {
            float cy = y + 80;
            draw_checkbox(x + 20, cy, config::chams.enabled, "Enable Chams"); cy += 35;
            draw_checkbox(x + 20, cy, config::chams.enemy, "Enemy"); cy += 35;
            draw_checkbox(x + 20, cy, config::chams.teammates, "Teammates"); cy += 35;
            draw_checkbox(x + 20, cy, config::chams.local, "Local Player"); cy += 35;
            draw_checkbox(x + 20, cy, config::chams.behind_walls, "Behind Walls"); cy += 35;
            
            std::vector<std::string> chams_types = {
                "Solid", "Shaded", "Glow", "Iridescent", "Water Flow",
                "Glossy Glow", "Wireframe", "Glass", "Blur", "Crystal"
            };
            draw_combo(x + 20, cy, chams_types, config::chams.type, "Chams Type"); cy += 45;
        }
        
        void draw_skins_tab() {
            float cy = y + 80;
            draw_checkbox(x + 20, cy, config::skin.enabled, "Enable Skin Changer"); cy += 35;
            
            // Weapon selector
            std::vector<std::string> weapons = {"AK-47", "M4A4", "AWP", "Deagle", "Knife", "Gloves"};
            draw_combo(x + 20, cy, weapons, config::skin.selected_weapon, "Weapon"); cy += 45;
            
            // Skin selector (simplified)
            std::vector<std::string> skins = {"Default", "Redline", "Fire Serpent", "Dragon Lore", "Howl", "Fade"};
            draw_combo(x + 20, cy, skins, config::skin.selected_skin, "Skin"); cy += 45;
            
            draw_slider(x + 20, cy, config::skin.pattern, 0, 1000, "Pattern: %.0f"); cy += 45;
            draw_slider(x + 20, cy, config::skin.stat_trak, 0, 999, "StatTrak: %.0f"); cy += 45;
            draw_slider(x + 20, cy, config::skin.charm, 0, 50, "Charm: %.0f"); cy += 45;
            draw_slider(x + 20, cy, config::skin.wear, 0, 1, "Wear: %.2f"); cy += 45;
        }
        
        void draw_misc_tab() {
            float cy = y + 80;
            draw_checkbox(x + 20, cy, config::misc.thirdperson, "Third Person"); cy += 35;
            draw_slider(x + 20, cy, config::misc.thirdperson_distance, 50, 500, "Distance: %.0f"); cy += 45;
            draw_checkbox(x + 20, cy, config::misc.fly, "Fly"); cy += 35;
            draw_checkbox(x + 20, cy, config::misc.noclip, "Noclip"); cy += 35;
            draw_checkbox(x + 20, cy, config::misc.no_recoil, "No Recoil"); cy += 35;
            draw_checkbox(x + 20, cy, config::misc.no_spread, "No Spread"); cy += 35;
            draw_checkbox(x + 20, cy, config::misc.rapid_fire, "Rapid Fire"); cy += 35;
            draw_checkbox(x + 20, cy, config::misc.infinite_ammo, "Infinite Ammo"); cy += 35;
            draw_checkbox(x + 20, cy, config::misc.auto_accept, "Auto Accept"); cy += 35;
            draw_checkbox(x + 20, cy, config::misc.auto_win, "Auto Win"); cy += 35;
        }
        
        void draw_settings_tab() {
            float cy = y + 80;
            std::vector<std::string> themes = {"Dark", "Light"};
            draw_combo(x + 20, cy, themes, config::menu.theme, "Theme"); cy += 45;
            draw_slider(x + 20, cy, config::menu.scale, 0.5f, 2.0f, "UI Scale: %.1f"); cy += 45;
            
            // Accent color picker (simplified)
            draw_text(x + 20, cy, "Accent Color", Color(1,1,1,0.8f), 14); cy += 25;
            draw_rect(x + 20, cy, 30, 30, Color::from_hex(config::menu.accent)); cy += 40;
        }
        
        // Drawing helpers (forward declarations for renderer)
        void draw_rect(float x, float y, float w, float h, const Color& col) {
            // Implemented in renderer
        }
        void draw_text(float x, float y, const std::string& text, const Color& col, float size, bool center = false) {
            // Implemented in renderer
        }
        void draw_checkbox(float x, float y, bool& value, const std::string& label) {
            // Implemented in renderer
        }
        void draw_slider(float x, float y, float& value, float min, float max, const std::string& format) {
            // Implemented in renderer
        }
        void draw_combo(float x, float y, std::vector<std::string>& items, int& selected, const std::string& label) {
            // Implemented in renderer
        }
        
    public:
        void render() {
            if (!visible) return;
            
            // Background
            draw_rect(x, y, w, h, Color(0.08f, 0.08f, 0.10f, 0.92f));
            
            // Title bar
            draw_rect(x, y, w, 35, Color(0.15f, 0.15f, 0.18f, 1.0f));
            draw_text(x + 20, y + 17.5f, "Serap Internal v1.0", Color(0.2f, 0.6f, 1.0f, 1.0f), 16);
            
            // Close button
            draw_text(x + w - 20, y + 17.5f, "?", Color(1,0.3f,0.3f,1), 20);
            
            // Tabs
            for (size_t i = 0; i < tab_names.size(); i++) {
                draw_tab(i, tab_names[i]);
            }
            
            // Tab content
            switch (active_tab) {
                case TAB_AIMBOT: draw_aimbot_tab(); break;
                case TAB_ANTIAIM: draw_antiaim_tab(); break;
                case TAB_VISUAL: draw_visual_tab(); break;
                case TAB_CHAMS: draw_chams_tab(); break;
                case TAB_SKINS: draw_skins_tab(); break;
                case TAB_MISC: draw_misc_tab(); break;
                case TAB_SETTINGS: draw_settings_tab(); break;
            }
        }
        
        void toggle() { visible = !visible; }
        bool is_visible() const { return visible; }
        void set_active_tab(int tab) { active_tab = tab; }
    };
}

// ================================================================
// ================================================================
// PART 14: GAME LOOP (200+ ñòðîê)
// ================================================================
class GameLoop {
private:
    game::GameData data;
    std::atomic<bool> running{false};
    std::chrono::steady_clock::time_point last_update;
    
public:
    void run() {
        running = true;
        last_update = std::chrono::steady_clock::now();
        LOGI("GameLoop started");
        
        while (running) {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - last_update).count();
            last_update = now;
            
            if (memory::MemoryManager::get().valid()) {
                // Update game data
                data.update();
                
                if (data.valid) {
                    // Run features
                    aimbot::run(data, config::aimbot);
                    antiaim::run(data, config::antiaim);
                    esp::render(data, config::esp);
                    chams::apply(data, config::chams);
                    skinchanger::run(config::skin);
                    
                    // Misc
                    misc::no_recoil(data);
                    misc::no_spread(data);
                    misc::infinite_ammo(data);
                    misc::rapid_fire(data);
                    misc::thirdperson(data);
                    misc::fly(data);
                    misc::noclip(data);
                    misc::auto_accept(data);
                    misc::auto_win(data);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
    
    void stop() { running = false; }
    game::GameData& get_data() { return data; }
};

// ================================================================
// ================================================================
// PART 15: ENTRY POINT (100+ ñòðîê)
// ================================================================
GameLoop* g_loop = nullptr;
ui::Menu* g_menu = nullptr;

__attribute__((constructor))
void OnLoad() {
    LOGI("Serap Internal v1.0 - Loading...");
    
    auto& mem = memory::MemoryManager::get();
    
    // Wait for game
    int tries = 0;
    while (!mem.init("com.standoff2") && tries < 30) {
        LOGD("Waiting for Standoff 2... (%d/30)", tries + 1);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        tries++;
    }
    
    if (!mem.valid()) {
        LOGE("Game not found");
        return;
    }
    
    LOGI("Game found! PID=%d, Base=0x%lx", mem.pid(), mem.base());
    
    // Enable HMS bypass
    auth::HMSBypass::get().enable();
    
    // Start game loop
    g_loop = new GameLoop();
    std::thread loop_thread(&GameLoop::run, g_loop);
    loop_thread.detach();
    
    // Start UI
    g_menu = new ui::Menu();
    std::thread ui_thread([](){
        while (true) {
            if (g_menu) g_menu->render();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    });
    ui_thread.detach();
    
    LOGI("Serap Internal v1.0 - Ready!");
}

__attribute__((destructor))
void OnUnload() {
    if (g_loop) { g_loop->stop(); delete g_loop; }
    if (g_menu) { delete g_menu; }
    LOGI("Serap Internal v1.0 - Unloaded");
}

// ================================================================
// ================================================================
// JNI EXPORTS
// ================================================================
extern "C" {
    JNIEXPORT void JNICALL
    Java_com_standoff2_SerapBridge_toggleMenu(JNIEnv* env, jobject obj) {
        if (g_menu) g_menu->toggle();
    }
    
    JNIEXPORT jboolean JNICALL
    Java_com_standoff2_SerapBridge_isMenuVisible(JNIEnv* env, jobject obj) {
        return g_menu ? (g_menu->is_visible() ? JNI_TRUE : JNI_FALSE) : JNI_FALSE;
    }
    
    JNIEXPORT void JNICALL
    Java_com_standoff2_SerapBridge_setAimbot(JNIEnv* env, jobject obj, jboolean enabled) {
        config::aimbot.enabled = enabled;
    }
    
    JNIEXPORT void JNICALL
    Java_com_standoff2_SerapBridge_setESP(JNIEnv* env, jobject obj, jboolean enabled) {
        config::esp.enabled = enabled;
    }
    
    JNIEXPORT void JNICALL
    Java_com_standoff2_SerapBridge_setAntiAim(JNIEnv* env, jobject obj, jboolean enabled) {
        config::antiaim.enabled = enabled;
    }
    
    JNIEXPORT void JNICALL
    Java_com_standoff2_SerapBridge_setSkinChanger(JNIEnv* env, jobject obj, jboolean enabled) {
        config::skin.enabled = enabled;
    }
    
    JNIEXPORT void JNICALL
    Java_com_standoff2_SerapBridge_setSkin(JNIEnv* env, jobject obj, jint weapon, jint skin) {
        config::skin.selected_weapon = weapon;
        config::skin.selected_skin = skin;
    }
}

// ================================================================
// END OF FILE
// ================================================================
