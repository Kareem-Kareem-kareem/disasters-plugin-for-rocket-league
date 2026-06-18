# ⚡ RL Disasters — BakkesMod Plugin

A BakkesMod plugin that adds toggleable chaos disasters to Rocket League. Each disaster can be turned on or off independently from the in-game HUD or the F2 settings menu.

---

## 🎮 Disasters

| Disaster | What it does |
|---|---|
| **Closest Spawn** | On each respawn, teleports you to the spawn point directly in line with your own goal (straight shot at your net) |
| **Bigger Goals** | Every goal scored makes both goals 15% larger — stacks until they're massive |
| **Bigger Field** | The arena grows by 10% each goal, capped at 3× original size |
| **1-sec Rumble** | Every player gets a brand-new random rumble item every second |
| **Persistent Rumble** | A random rumble pickup always exists somewhere on the field |

---

## 📦 Installation

### Pre-built (recommended)
1. Go to [Releases](../../releases) and download `RLDisasters.dll`
2. Copy `RLDisasters.dll` to:
   ```
   %APPDATA%\bakkesmod\bakkesmod\plugins\
   ```
3. Open Rocket League → open BakkesMod console (F6) → type:
   ```
   plugin load RLDisasters
   ```
4. Press **F2** → Plugins → RL Disasters to open the settings

---

## 🔧 Building from Source

### Requirements
- Windows 10/11
- Visual Studio 2022 (with C++ Desktop workload)
- CMake 3.15+
- [BakkesMod SDK](https://github.com/bakkesmodorg/BakkesModSDK)

### Steps

```bash
# 1. Clone the repo
git clone https://github.com/YourName/RLDisasters.git
cd RLDisasters

# 2. Download BakkesMod SDK
#    Either clone it or download the ZIP:
git clone https://github.com/bakkesmodorg/BakkesModSDK.git bakkesmod-sdk

# 3. Configure and build
cmake -B build -S . -DBAKKESMOD_SDK="./bakkesmod-sdk"
cmake --build build --config Release

# 4. Output is at build/Release/RLDisasters.dll
#    Copy it to %APPDATA%\bakkesmod\bakkesmod\plugins\
```

### Or use the GitHub Actions CI
Push to `main` and the workflow automatically builds and uploads the DLL as an artifact. Create a GitHub Release to automatically attach the DLL to it.

---

## 🕹️ Usage

### In-game HUD
A panel appears in the top-right corner showing all disasters. The current state (on/off) is visible at a glance with color-coded toggles.

> **To toggle from HUD:** Open F2 → Plugins → RL Disasters and check/uncheck boxes. Changes apply instantly.

### F2 Settings
- Each disaster has a checkbox + description
- A **Reset All** button resets everything and removes all active effects
- Live stats show current goal count and field scale

---

## 📁 File Structure

```
RLDisasters/
├── src/
│   ├── RLDisasters.h       # Plugin header
│   └── RLDisasters.cpp     # All disaster logic + HUD + settings
├── .github/
│   └── workflows/
│       └── build.yml       # GitHub Actions CI/CD
├── CMakeLists.txt          # Build config
├── RLDisasters.set         # BakkesMod plugin metadata
└── README.md
```

---

## ⚠️ Notes

- **Freeplay & Custom Training** work for most disasters
- **Online/Ranked** — some effects (goal scaling, field scaling) may be server-authoritative and only work in LAN/private matches
- Rumble disasters work best in Rumble game mode or private matches with Rumble mutators enabled
- The closest spawn disaster overrides the normal random spawn, replacing it with the centre-line spawn facing your own goal

---

## 🤝 Contributing

PRs welcome! Ideas for new disasters:
- Gravity changes
- Random camera perspectives
- Ball size changes
- Speed boost drain
