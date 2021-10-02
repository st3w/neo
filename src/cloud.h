/*
    cloud.h

    Copyright (C) 2021 Stewart Reive

    neo is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    neo is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with neo. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#ifndef CLOUD_H
#define CLOUD_H

#include "droplet.h"
#include "neo.h"

#include <ncurses.h>

#include <random>
#include <vector>

using namespace std;

class Cloud {
public:
    Cloud(ColorMode cm, bool def2ascii); // Must be called *AFTER* InitCurses

    enum class ShadingMode : unsigned {
        RANDOM,
        DISTANCE_FROM_HEAD,
        INVALID
    };
    enum class BoldMode : unsigned {
        OFF,
        RANDOM,
        ALL,
        INVALID
    };

    void Rain();
    void Reset();

    struct CharAttr {
        int colorPair;
        bool isBold;
    };
    void GetAttr(uint16_t line, uint16_t col, wchar_t val, Droplet::CharLoc ct,
                 CharAttr* pAttr, high_resolution_clock::time_point time,
                 uint16_t headPutLine, uint16_t len) const;

    float GetCharsPerSec() const { return _charsPerSec; }
    void SetCharsPerSec(float cps);
    wchar_t GetChar(uint16_t line, uint16_t charPoolIdx) const;
    bool IsGlitched(uint16_t line, uint16_t col) const;

    static constexpr size_t CHAR_POOL_SIZE = 2048;
    static constexpr size_t GLITCH_POOL_SIZE = 1024;

    void ForceDrawEverything() { _forceDrawEverything = true; }
    ShadingMode GetShadingMode() const { return _shadingMode; }
    void SetShadingMode(ShadingMode sm) { _shadingMode = sm; ForceDrawEverything(); }
    void TogglePause();
    Color GetColor() const { return _color; }
    void SetColor(Color c);
    void SetDropletDensity(float density);
    float GetDropletDensity() const { return _dropletDensity; }
    void SetFullWidth() { _fullWidth = true; }
    void SetDefaultBackground() { _defaultBackground = true; }
    bool GetAsync() const { return _async; }
    void SetAsync(bool b) { _async = b; }
    void SetColumnSpeeds();
    void UpdateDropletSpeeds();
    void SetCharset(Charset a) { _charset = a; }
    void AddChars(wchar_t begin, wchar_t end);
    void InitChars();
    bool Raining() { return _raining; }
    void SetRaining(bool b) { _raining = b; }
    void SetBoldMode(BoldMode bm) { _boldMode = bm; }
    float GetGlitchPct() const { return _glitchPct; }
    void SetGlitchPct(float pct);
    void SetGlitchTimes(uint16_t low_ms, uint16_t high_ms);
    bool GetGlitchy() const { return _glitchy; }
    void SetGlitchy(bool b) { _glitchy = b; }
    void SetShortPct(float pct) { _shortPct = pct; }
    void SetDieEarlyPct(float pct) { _dieEarlyPct = pct; }
    void SetLingerTimes(uint16_t low_ms, uint16_t high_ms);

    void SetMessage(const char* msg);
    ColorMode GetColorMode() const { return _colorMode; }
    uint16_t GetLines() const { return _lines; }
    uint16_t GetCols() const { return _cols; }
    void SetColumnSpawn(uint16_t col, bool b);
    void SetMaxDropletsPerColumn(uint8_t val) { _maxDropletsPerColumn = val; }
    void SetUserColors(vector<ColorContent>&& vals) { _usrColors = std::move(vals); }

private:
    vector<Droplet> _droplets = {};
    size_t _numDroplets = 0;

    // ncurses can change the LINES/COLS variables. So keep a local copy, lest
    // we overrun some buffer.
    uint16_t _lines = 25;
    uint16_t _cols = 80;
    Charset _charset = Charset::NONE;
    vector<wchar_t> _chars = {}; // The chars that can be displayed
    vector<wchar_t> _userChars = {}; // chars passed directly from the user
    vector<wchar_t> _charPool = {}; // Precomputed random chars
    vector<wchar_t> _glitchPool = {}; // Precomputed random chars used for glitching
    size_t _glitchPoolIdx = 0;
    vector<bool> _glitchMap = {}; // Which screen positions are glitched
    vector<int> _colorPairMap = {}; // Color for each screen position
    float _dropletDensity = 1.0f; // How many columns should have droplets
    float _dropletsPerSec = 5.0f; // Number of droplets to spawn each second
    static constexpr size_t MAX_DROPLETS_PER_COL = 4;
    struct ColumnStatus {
        float maxSpeedPct; // how fast droplets in this column travel
        uint8_t numDroplets;
        bool canSpawn; // true if more droplets can be added to this column
    };
    vector<ColumnStatus> _colStat = {};
    high_resolution_clock::time_point _lastGlitchTime = {};
    high_resolution_clock::time_point _nextGlitchTime = {};
    high_resolution_clock::time_point _pauseTime = {};
    high_resolution_clock::time_point _lastSpawnTime = {};
    float _charsPerSec = 8.0f; // Neo/Cypher scene is ~8.3333333f
    ShadingMode _shadingMode = ShadingMode::RANDOM;
    bool _forceDrawEverything = false;
    bool _pause = false;
    bool _fullWidth = false;
    Color _color = Color::GREEN;
    bool _defaultBackground = false;
    bool _async = false;
    bool _raining = true;
    BoldMode _boldMode = BoldMode::RANDOM;
    float _glitchPct = 0.1f;
    uint16_t _glitchLowMs = 300;
    uint16_t _glitchHighMs = 400;
    bool _glitchy = true;
    float _shortPct = 0.5f;
    float _dieEarlyPct = 0.3333333f;
    uint16_t _lingerLowMs = 1;
    uint16_t _lingerHighMs = 3000;
    uint8_t _maxDropletsPerColumn = 3;
    bool _defaultToAscii = false;

    struct MsgChr {
        explicit MsgChr(char v) : line(0), col(0), val(v), draw(false) {}
        uint16_t line = 0;
        uint16_t col = 0;
        char val = '\0';
        bool draw = false;
    };
    vector<MsgChr> _message = {};

    // RNG stuff
    mt19937 mt = {};
    uniform_int_distribution<int> _randColorPair = {};
    uniform_real_distribution<float> _randChance = {};
    uniform_int_distribution<uint16_t> _randLine = {};
    uniform_int_distribution<uint16_t> _randCpIdx = {};
    uniform_int_distribution<uint16_t> _randLen = {};
    uniform_int_distribution<uint16_t> _randCol = {};
    uniform_int_distribution<uint16_t> _randGlitchMs = {};
    uniform_int_distribution<uint16_t> _randLingerMs = {};
    uniform_int_distribution<size_t> _randCharIdx = {};
    uniform_real_distribution<float> _randSpeed = {};

    ColorMode _colorMode = ColorMode::MONO;
    int _numColorPairs = 7;
    vector<ColorContent> _usrColors = {};

    bool TimeForGlitch(high_resolution_clock::time_point time) const;
    void DoGlitch(const Droplet& droplet);
    bool IsBright(high_resolution_clock::time_point time) const;
    bool IsDim(high_resolution_clock::time_point time) const;
    void FillDroplet(Droplet* pDroplet, uint16_t col);

    void SpawnDroplets(high_resolution_clock::time_point curTime);
    void FillColorMap(size_t screenSize);
    void FillGlitchMap(size_t screenSize);
    void ResetMessage();
    void CalcMessage();
    void DrawMessage() const;
};

#endif
