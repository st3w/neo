/*
    neo.cpp - The main program logic for neo

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

#include "neo.h"
#include "droplet.h"
#include "cloud.h"

#include <getopt.h>
#include <locale.h>
#include <ncurses.h>
#include <cassert>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <random>
#include <thread>
#include <utility>
using namespace std;
using namespace chrono;

void Die(const char* fmtStr, ...) {
    Cleanup();
    va_list vargs;
    va_start(vargs, fmtStr);
    vfprintf(stderr, fmtStr, vargs);
    va_end(vargs);
    exit(1);
}

static bool cursesInit = false;
static bool screensaver = false;

ColorContent ParseColorLine(char* line, size_t lineNum) {
    ColorContent cc;
    cc.color = static_cast<short>(strtol(line, nullptr, 10));
    if (cc.color >= COLORS) {
        Die("Bad color value (%d) on line %zu (max %d)\n",
            cc.color, lineNum, COLORS-1);
    }
    if (!strstr(line, ",")) {
        // No commas found - the user provided a single 16 or 256 color value.
        return cc;
    }
    char* tok = strtok(line, ",");
    for (int i = 0; i < 3; i++) {
        tok = strtok(nullptr, ",");
        if (!tok || *tok == '\0' || *tok == '\n') {
            Die("Color file line %zu does not have four components\n", lineNum);
            return cc;
        }

        const long int val = strtol(tok, nullptr, 10);
        if (val < 0 || val > 1000)
            Die("Bad RGB component value (%ld) on line %zu\n", val, lineNum);

        const short sval = static_cast<short>(val);
        switch (i) {
            case 0:
                cc.r = sval;
                break;
            case 1:
                cc.g = sval;
                break;
            case 2:
                cc.b = sval;
                break;
            default:
                break;
        }
    }
    return cc;
}

unsigned ParseColorFileVersion(FILE* colorFile, size_t* numLines) {
    static constexpr unsigned long latestVersion = 1;
    unsigned long version = latestVersion;
    char* line = nullptr;
    size_t lineLen;

    // Ignore any comments and blank lines at the beginning of the file
    while (getline(&line, &lineLen, colorFile) != -1) {
        *numLines += 1;
        if (!line || *line == '\0' || *line == '\n' || *line == ';' ||
            *line == '#' || *line == '/' || *line == '*' || *line == '@')
        {
            continue;
        }
        break;
    }
    if (!line)
        Die("Invalid color file\n");

    if (strstr(line, "neo_color_version")) {
        char* tok = strtok(line, " ");
        if (!tok) {
            if (line)
                free(line);
            Die("Invalid color file version\n");
        }
        tok = strtok(nullptr, " ");
        if (!tok) {
            if (line)
                free(line);
            Die("Invalid color file version\n");
        }
        version = strtoul(tok, nullptr, 10);
        if (version == ULONG_MAX || !version)
            Die("Invalid color file version\n");
        else if (version > latestVersion)
            Die("Color file version (%lu) is newer than supported (%lu)\n",
                version, latestVersion);
    } else {
        // Assume that the first line is color content since no version string
        // was found
        rewind(colorFile);
    }

    if (line)
        free(line);

    return version;
}

vector<ColorContent> ParseColorFile(const char* filename) {
    FILE* colorFile = fopen(filename, "r");
    if (!colorFile)
        Die("Could not read colorfile: %s\n", filename);

    size_t numLines = 0;

    (void) ParseColorFileVersion(colorFile, &numLines);

    vector<ColorContent> colors;
    char* line = nullptr;
    size_t lineLen;
    size_t numColorPairs = 0;

    while (getline(&line, &lineLen, colorFile) != -1) {
        numLines++;
        if (!line || *line == '\0' || *line == '\n' || *line == ';' ||
            *line == '#' || *line == '/' || *line == '*' || *line == '@')
        {
            continue;
        }
        numColorPairs++;
        if (numColorPairs > static_cast<size_t>(COLOR_PAIRS))
            Die("Color file has too many lines (max %d)\n", COLOR_PAIRS);

        ColorContent cc = ParseColorLine(line, numLines);
        colors.push_back(cc);
    }
    if (line)
        free(line);
    if (numLines < 2)
        Die("Color file must have at least two colors\n");

    return colors;
}

// Determine the correct ColorMode to use based on user input and
// the color capabilities that ncurses advertises.
ColorMode PickColorMode(ColorMode usrColorMode) {
    if (usrColorMode != ColorMode::INVALID)
        return usrColorMode;
    if (!has_colors())
        return ColorMode::MONO;
    if (COLORS >= 256) {
        if (can_change_color())
            return ColorMode::TRUECOLOR;
        else
            return ColorMode::COLOR256;
    }

    return ColorMode::COLOR16;
}

int InitCurses(ColorMode usrColorMode, ColorMode* pOutColorMode) {
    initscr();
    if (cbreak() != OK)
        Die("cbreak() failed\n");
    curs_set(0); // If this fails, the cursor may blink but it isn't fatal
    if (noecho() != OK)
        Die("noecho() failed\n");
    if (nodelay(stdscr, TRUE) != OK)
        Die("nodelay() failed\n");
    if (keypad(stdscr, true) != OK)
        Die("keypad() failed\n");

    if (usrColorMode != ColorMode::MONO && has_colors())
        start_color();
    *pOutColorMode = PickColorMode(usrColorMode);

    if (clear() != OK)
        Die("clear() failed\n");
    if (refresh() != OK)
        Die("refresh() failed\n");

    cursesInit = true;

    return 0;
}

void Cleanup() {
    if (cursesInit) {
        endwin();
    }
    cursesInit = false;
}

void HandleInput(Cloud* pCloud) {
    int ch = getch();
    if (ch == -1)
        return;
    if (screensaver && ch != KEY_RESIZE) {
        Cleanup();
        exit(0);
    }
    switch (ch) {
        case KEY_RESIZE:
        case ' ':
            pCloud->Reset();
            pCloud->ForceDrawEverything();
            break;
        case 'a':
            pCloud->SetAsync(!pCloud->GetAsync());
            pCloud->SetColumnSpeeds();
            pCloud->UpdateDropletSpeeds();
            break;
        case 'p':
            pCloud->TogglePause();
            break;
        case KEY_UP: {
            float cps = pCloud->GetCharsPerSec();
            if (cps <= 0.5f)
                cps *= 2.0f;
            else
                cps += 1.0f;
            cps = min(cps, 1000.0f);
            pCloud->SetCharsPerSec(cps);
            break;
        }
        case KEY_DOWN: {
            float cps = pCloud->GetCharsPerSec();
            if (cps <= 1.0f)
                cps /= 2.0f;
            else
                cps -= 1.0f;
            pCloud->SetCharsPerSec(cps);
            break;
        }
        case KEY_LEFT: {
            if (pCloud->GetGlitchy()) {
                float gpct = pCloud->GetGlitchPct();
                gpct -= 0.05f;
                gpct = max(gpct, 0.0f);
                pCloud->SetGlitchPct(gpct);
            }
            break;
        }
        case KEY_RIGHT: {
            if (pCloud->GetGlitchy()) {
                float gpct = pCloud->GetGlitchPct();
                gpct += 0.05f;
                gpct = min(gpct, 1.0f);
                pCloud->SetGlitchPct(gpct);
            }
            break;
        }
        case '\t': {
            if (pCloud->GetShadingMode() == Cloud::ShadingMode::RANDOM)
                pCloud->SetShadingMode(Cloud::ShadingMode::DISTANCE_FROM_HEAD);
            else
                pCloud->SetShadingMode(Cloud::ShadingMode::RANDOM);
            break;
        }
        case 'q':
        case 27: // ESC
            pCloud->SetRaining(false);
            break;
        case '1':
            pCloud->SetColor(Color::GREEN);
            break;
        case '2':
            pCloud->SetColor(Color::GREEN2);
            break;
        case '3':
            pCloud->SetColor(Color::GREEN3);
            break;
        case '4':
            pCloud->SetColor(Color::GOLD);
            break;
        case '5':
            pCloud->SetColor(Color::PINK2);
            break;
        case '6':
            pCloud->SetColor(Color::RED);
            break;
        case '7':
            pCloud->SetColor(Color::BLUE);
            break;
        case '8':
            pCloud->SetColor(Color::CYAN);
            break;
        case '9':
            pCloud->SetColor(Color::PURPLE);
            break;
        case '0':
            pCloud->SetColor(Color::GRAY);
            break;
        case '!':
            pCloud->SetColor(Color::RAINBOW);
            break;
        case '@':
            pCloud->SetColor(Color::YELLOW);
            break;
        case '#':
            pCloud->SetColor(Color::ORANGE);
            break;
        case '$':
            pCloud->SetColor(Color::PINK);
            break;
        case '%':
            pCloud->SetColor(Color::VAPORWAVE);
            break;
        case '-': {
            float density = pCloud->GetDropletDensity();
            density -= 0.25f;
            density = max(density, 0.01f);
            pCloud->SetDropletDensity(density);
            break;
        }
        case '+': {
            float density = pCloud->GetDropletDensity();
            density += 0.25f;
            density = min(density, 5.0f);
            pCloud->SetDropletDensity(density);
            break;
        }
        default:
            break;
    }
}

void PrintVersion() {
    printf("neo %s\n", VERSION);
    printf("Built on %s\n", __DATE__);
    printf("Copyright (C) 2021 Stewart Reive\n"
           "License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n"
           "This is free software: you are free to change and redistribute it.\n"
           "There is NO WARRANTY, to the extent permitted by law.\n");
    printf("This program is not affiliated with \"The Matrix\",\n"
           "Warner Bros. Entertainment Inc., Village Roadshow Pictures, Silver Pictures,\n"
           "nor any of their parent companies, subsidiaries, partners, or affiliates.\n");
    exit(0);
}

void PrintHelp(bool bErr) {
    FILE* f = bErr ? stderr : stdout;
    fprintf(f, "Usage: neo [OPTIONS]\n");
    fprintf(f, "\n");
    fprintf(f, "Simulate the digital rain from \"The Matrix\"\n");
    fprintf(f, "\n");
    fprintf(f, "Options:\n");
    fprintf(f, "  -a, --async            asynchronous scroll speed\n");
    fprintf(f, "  -b, --bold=NUM         control character boldness\n");
    fprintf(f, "  -C, --colorfile=FILE   read the colors from a file\n");
    fprintf(f, "  -c, --color=COLOR      select the foreground text color\n");
    fprintf(f, "  -D, --defaultbg        use the default terminal background color\n");
    fprintf(f, "  -d, --density=NUM      set the density of droplets\n");
    fprintf(f, "  -F, --fullwidth        use two columns per character\n");
    fprintf(f, "  -f, --fps=NUM          set the frames per second target/limit\n");
    fprintf(f, "  -G, --glitchpct=NUM    set the percentage of screen chars that glitch\n");
    fprintf(f, "  -g, --glitchms=NUM1,2  control how often characters glitch\n");
    fprintf(f, "  -h, --help             show this help message\n");
    fprintf(f, "  -l, --lingerms=NUM1,2  control how long characters linger after scrolling\n");
    fprintf(f, "  -M, --shadingmode=NUM  set the shading mode\n");
    fprintf(f, "  -m, --message=STR      display a message\n");
    fprintf(f, "  -p, --profile          enable profiling mode\n");
    fprintf(f, "  -r, --rippct=NUM       set the percentage of droplets that die early\n");
    fprintf(f, "  -S, --speed=NUM        set the scroll speed in chars per second\n");
    fprintf(f, "  -s, --screensaver      exit on the first key press\n");
    fprintf(f, "  -V, --version          print the version\n");
    fprintf(f, "      --chars=NUM1,2     use a range of unicode chars\n");
    fprintf(f, "      --charset=STR      set the character set\n");
    fprintf(f, "      --colormode=NUM    set the color mode\n");
    fprintf(f, "      --maxdpc=NUM       set the maximum droplets per column\n");
    fprintf(f, "      --noglitch         disable character glitching\n");
    fprintf(f, "      --shortpct=NUM     set the percentage of shortened droplets\n");
    fprintf(f, "\n");
    fprintf(f, "See the manual page for more info: man neo\n");
    exit(bErr ? 1 : 0);
}

// Long form options that have no short equivalent
enum LongOpts {
    CHARS = CHAR_MAX + 1,
    CHARSET,
    COLORMODE,
    MAXDPC,
    NOGLITCH,
    SHORTPCT,
};

static constexpr option long_options[] = {
    { "async",       no_argument,       nullptr, 'a' },
    { "bold",        required_argument, nullptr, 'b' },
    { "chars",       required_argument, nullptr, LongOpts::CHARS },
    { "charset",     required_argument, nullptr, LongOpts::CHARSET },
    { "color",       required_argument, nullptr, 'c' },
    { "colorfile",   required_argument, nullptr, 'C' },
    { "colormode",   required_argument, nullptr, LongOpts::COLORMODE },
    { "defaultbg",   no_argument,       nullptr, 'D' },
    { "density",     required_argument, nullptr, 'd' },
    { "fps",         required_argument, nullptr, 'f' },
    { "fullwidth",   no_argument,       nullptr, 'F' },
    { "glitchms",    required_argument, nullptr, 'g' },
    { "glitchpct",   required_argument, nullptr, 'G' },
    { "help",        no_argument,       nullptr, 'h' },
    { "lingerms",    required_argument, nullptr, 'l' },
    { "maxdpc",      required_argument, nullptr, LongOpts::MAXDPC },
    { "message",     required_argument, nullptr, 'm' },
    { "noglitch",    no_argument,       nullptr, LongOpts::NOGLITCH },
    { "screensaver", no_argument,       nullptr, 's' },
    { "shadingmode", required_argument, nullptr, 'M' },
    { "profile",     no_argument,       nullptr, 'p' },
    { "rippct",      required_argument, nullptr, 'r' },
    { "shortpct",    required_argument, nullptr, LongOpts::SHORTPCT },
    { "speed",       required_argument, nullptr, 'S' },
    { "version",     no_argument,       nullptr, 'V' },
    { nullptr,       no_argument,       nullptr, 0 }
};

const char* optstring = "A:ab:C:c:Dd:Ff:G:g:hl:M:m:pr:sS:V";

// Parse arguments before ncurses is initialized
void ParseArgsEarly(int argc, char* argv[], ColorMode* pUsrColorMode) {
    int opt;
    while ((opt = getopt_long(argc, argv, optstring, long_options, nullptr)) != -1) {
        if (opt != LongOpts::COLORMODE)
            continue;

        const long int mode = strtol(optarg, nullptr, 10);
        if (mode == 0) {
            *pUsrColorMode = ColorMode::MONO;
        } else if (mode == 16) {
            *pUsrColorMode = ColorMode::COLOR16;
        } else if (mode == 32) {
            *pUsrColorMode = ColorMode::TRUECOLOR;
        } else if (mode == 256) {
            *pUsrColorMode = ColorMode::COLOR256;
        } else {
            Die("--colormode must be one of 0, 16, 32, or 256\n");
        }
    }
}

vector<wchar_t> ParseUserChars(char* argStr) {
    vector<wchar_t> output;
    char* nextStr;
    int index = 1;
    wchar_t uniChar;

    while (*argStr) {
        uniChar = static_cast<wchar_t>(strtol(argStr, &nextStr, 16));
        if (!uniChar || uniChar > 0x10FFFF)
            Die("Invalid unicode char at index %d\n", index);

        output.push_back(uniChar);
        if (*nextStr)
            nextStr++; // skip the comma
        argStr = nextStr;
        index++;
    }

    return output;
}

void ParseArgs(int argc, char* argv[], Cloud* pCloud, double* targetFPS, bool* profiling) {
    optind = 1;
    int opt;

    while ((opt = getopt_long(argc, argv, optstring, long_options, nullptr)) != -1) {
        switch (opt) {
        case LongOpts::CHARSET: {
            if (strcasecmp(optarg, "ascii") == 0) {
                pCloud->SetCharset(Charset::DEFAULT);
            } else if (strcasecmp(optarg, "extended") == 0) {
                pCloud->SetCharset(Charset::EXTENDED_DEFAULT);
            } else if (strcasecmp(optarg, "english") == 0) {
                pCloud->SetCharset(Charset::ENGLISH_LETTERS);
            } else if (strcasecmp(optarg, "digits") == 0 ||
                       strcasecmp(optarg, "dec") == 0 ||
                       strcasecmp(optarg, "decimal") == 0) {
                pCloud->SetCharset(Charset::ENGLISH_DIGITS);
            } else if (strcasecmp(optarg, "punc") == 0) {
                pCloud->SetCharset(Charset::ENGLISH_PUNCTUATION);
            } else if (strcasecmp(optarg, "bin") == 0 ||
                       strcasecmp(optarg, "binary") == 0) {
                pCloud->SetCharset(Charset::BINARY);
            } else if (strcasecmp(optarg, "hex") == 0 ||
                       strcasecmp(optarg, "hexadecimal") == 0) {
                pCloud->SetCharset(Charset::HEX);
            } else if (strcasecmp(optarg, "katakana") == 0) {
                pCloud->SetCharset(Charset::KATAKANA);
            } else if (strcasecmp(optarg, "greek") == 0) {
                pCloud->SetCharset(Charset::GREEK);
            } else if (strcasecmp(optarg, "cyrillic") == 0) {
                pCloud->SetCharset(Charset::CYRILLIC);
            } else if (strcasecmp(optarg, "arabic") == 0) {
                pCloud->SetCharset(Charset::ARABIC);
            } else if (strcasecmp(optarg, "hebrew") == 0) {
                pCloud->SetCharset(Charset::HEBREW);
            } else if (strcasecmp(optarg, "devanagari") == 0) {
                pCloud->SetCharset(Charset::DEVANAGARI);
            } else if (strcasecmp(optarg, "braille") == 0) {
                pCloud->SetCharset(Charset::BRAILLE);
            } else if (strcasecmp(optarg, "runic") == 0) {
                pCloud->SetCharset(Charset::RUNIC);
            } else {
                Die("Unsupported charset specified: %s\n", optarg);
            }
            break;
        }
        case 'a':
            pCloud->SetAsync(true);
            pCloud->SetColumnSpeeds();
            pCloud->UpdateDropletSpeeds();
            break;
        case 'b': {
            const int bmi = atoi(optarg);
            if (bmi >= static_cast<int>(Cloud::BoldMode::INVALID) || bmi < 0)
                Die("-b/--bold option must be 0, 1, or 2\n");

            const Cloud::BoldMode bm = static_cast<Cloud::BoldMode>(bmi);
            pCloud->SetBoldMode(bm);
            break;
        }
        case 'C': {
            vector<ColorContent> usrColors = ParseColorFile(optarg);
            pCloud->SetUserColors(std::move(usrColors));
            pCloud->SetColor(Color::USER);
            break;
        }
        case 'c':
            if (strcasecmp(optarg, "green") == 0) {
                pCloud->SetColor(Color::GREEN);
            } else if (strcasecmp(optarg, "green2") == 0) {
                pCloud->SetColor(Color::GREEN2);
            } else if (strcasecmp(optarg, "green3") == 0) {
                pCloud->SetColor(Color::GREEN3);
            } else if (strcasecmp(optarg, "yellow") == 0) {
                pCloud->SetColor(Color::YELLOW);
            } else if (strcasecmp(optarg, "orange") == 0) {
                pCloud->SetColor(Color::ORANGE);
            } else if (strcasecmp(optarg, "red") == 0) {
                pCloud->SetColor(Color::RED);
            } else if (strcasecmp(optarg, "blue") == 0) {
                pCloud->SetColor(Color::BLUE);
            } else if (strcasecmp(optarg, "cyan") == 0) {
                pCloud->SetColor(Color::CYAN);
            } else if (strcasecmp(optarg, "gold") == 0) {
                pCloud->SetColor(Color::GOLD);
            } else if (strcasecmp(optarg, "rainbow") == 0) {
                pCloud->SetColor(Color::RAINBOW);
            } else if (strcasecmp(optarg, "purple") == 0) {
                pCloud->SetColor(Color::PURPLE);
            } else if (strcasecmp(optarg, "pink") == 0) {
                pCloud->SetColor(Color::PINK);
            } else if (strcasecmp(optarg, "pink2") == 0) {
                pCloud->SetColor(Color::PINK2);
            } else if (strcasecmp(optarg, "vaporwave") == 0) {
                pCloud->SetColor(Color::VAPORWAVE);
            } else if (strcasecmp(optarg, "gray") == 0) {
                pCloud->SetColor(Color::GRAY);
            } else {
                Die("Invalid color specified: %s\n", optarg);
            }
            break;
        case 'D': {
            pCloud->SetDefaultBackground();
            pCloud->SetColor(pCloud->GetColor());
            break;
        }
        case 'd': {
            const float density = atof(optarg);
            if (density <= 0.0f || density >= 100.0f)
                Die("-d/--density must be greater than 0 and less than 100.0\n");

            pCloud->SetDropletDensity(density);
            break;
        }
        case 'f': {
            *targetFPS = atof(optarg);
            if (*targetFPS <= 0.0) {
                Die("-f/--fps option must be greater than 0\n");
            }
            break;
        }
        case 'F': {
            pCloud->SetFullWidth();
            break;
        }
        case 'g': {
            char* nextStr;
            const long int low_ms = strtol(optarg, &nextStr, 10);
            if (!nextStr || *nextStr == '\0' || *(nextStr+1) == '\0')
                Die("Invalid -g/--glitchms option\n");

            nextStr++;
            const long int high_ms = strtol(nextStr, nullptr, 10);
            if (low_ms <= 0 || high_ms <= 0 || low_ms > high_ms || high_ms > 0xFFFFU)
                Die("Invalid -g/--glitchms option\n");

            pCloud->SetGlitchTimes(static_cast<uint16_t>(low_ms), static_cast<uint16_t>(high_ms));
            break;
        }
        case 'G': {
            const float gpct = atof(optarg);
            if (gpct < 0.0f || gpct > 100.0f)
                Die("-G/--glitchpct must be between 0 and 100.0 inclusive\n");

            pCloud->SetGlitchPct(gpct / 100.0f);
            break;
        }
        case 'h':
            Cleanup();
            PrintHelp(false);
            break;
        case 'l': {
            char* nextStr;
            const long int low_ms = strtol(optarg, &nextStr, 10);
            if (!nextStr || *nextStr == '\0' || *(nextStr+1) == '\0')
                Die("Invalid -l/--lingerms option\n");

            nextStr++;
            const long int high_ms = strtol(nextStr, nullptr, 10);
            if (low_ms <= 0 || high_ms <= 0 || low_ms > high_ms || high_ms > 0xFFFFU)
                Die("Invalid -l/--lingerms option\n");

            pCloud->SetLingerTimes(static_cast<uint16_t>(low_ms), static_cast<uint16_t>(high_ms));

            break;
        }
        case 'M': {
            const int smi = atoi(optarg);
            if (smi >= static_cast<int>(Cloud::ShadingMode::INVALID) || smi < 0)
                Die("-M/--shadingmode must be 0 or 1\n");

            const Cloud::ShadingMode sm = static_cast<Cloud::ShadingMode>(smi);
            pCloud->SetShadingMode(sm);
            break;
        }
        case 'm':
            pCloud->SetMessage(optarg);
            break;
        case 'p':
            *profiling = true;
            break;
        case 'r': {
            const float pct = atof(optarg);
            if (pct < 0.0f || pct > 100.0f)
                Die("-r/--rippct must be between 0 and 100.0 inclusive\n");

            pCloud->SetDieEarlyPct(pct / 100.0f);
            break;
        }
        case 's':
            screensaver = true;
            break;
        case 'S': {
            const float cps = atof(optarg);
            if (cps <= 0.0f || cps > 1000000.0f)
                Die("-s/--speed must be greater than 0 and less than 1000000\n");

            pCloud->SetCharsPerSec(cps);
            break;
        }
        case 't':
            break; // handled by ParseArgsEarly()
        case 'V':
            Cleanup();
            PrintVersion();
            break;
        case LongOpts::CHARS: {
            vector<wchar_t> uniChars = ParseUserChars(optarg);
            const size_t numChars = uniChars.size();
            if (numChars % 2)
                Die("--chars: odd number of unicode chars given (must be even)\n");

            for (size_t chIdx = 0; chIdx < numChars; chIdx += 2) {
                pCloud->AddChars(uniChars[chIdx], uniChars[chIdx+1]);
            }
            break;
        }
        case LongOpts::COLORMODE:
            break; // handled by ParseArgsEarly()
        case LongOpts::MAXDPC: {
            const long maxdpc = strtol(optarg, nullptr, 10);
            if (maxdpc < 1 || maxdpc > 3)
                Die("--maxdpc must be 1, 2, or 3\n");

            pCloud->SetMaxDropletsPerColumn(static_cast<uint8_t>(maxdpc));
            break;
        }
        case LongOpts::NOGLITCH:
            pCloud->SetGlitchy(false);
            pCloud->SetGlitchPct(0.0f);
            pCloud->SetGlitchTimes(0xFFFFU, 0xFFFFU);
            break;
        case LongOpts::SHORTPCT: {
            const float pct = atof(optarg);
            if (pct < 0.0f || pct > 100.0f)
                Die("--shortpct must be between 0 and 100.0 inclusive\n");

            pCloud->SetShortPct(pct / 100.0f);
            break;
        }
        case '?':
        default:
            Cleanup();
            if (optopt) {
                fprintf(stderr, "Invalid/malformed option: -%c\n", optopt);
            } else {
                fprintf(stderr, "Bad command line option\n");
            }
            PrintHelp(true);
            break;
        }
    }
}

// This is a rudimentary profiler that keeps track of how long this
// app takes and how long ncurses refresh() takes.
void Profiler(Cloud& cloud) {
    high_resolution_clock::time_point prevTime = high_resolution_clock::now();
    high_resolution_clock::time_point curTime;
    high_resolution_clock::time_point curTime2;
    nanoseconds elapsed;

    FILE* fp = fopen("time_profile.txt", "w+");

    while (cloud.Raining()) {
        HandleInput(&cloud);
        cloud.Rain();
        curTime2 = high_resolution_clock::now();
        elapsed = duration_cast<nanoseconds>(curTime2 - prevTime);
        fprintf(fp, "app_ns=%lld\n", static_cast<long long int>(elapsed.count()));
        refresh();

        curTime = high_resolution_clock::now();
        elapsed = duration_cast<nanoseconds>(curTime - curTime2);
        fprintf(fp, "refresh_ns=%lld\n", static_cast<long long int>(elapsed.count()));
        prevTime = curTime;
    }

    fclose(fp);
}

void MainLoop(Cloud& cloud, double targetFPS) {
    const nanoseconds targetPeriod(static_cast<uint64_t>(round(1.0 / targetFPS * 1.0e9)));
    high_resolution_clock::time_point prevTime = high_resolution_clock::now();
    high_resolution_clock::time_point curTime;
    nanoseconds elapsed;
    nanoseconds prevDelay(5);
    nanoseconds curDelay;
    nanoseconds calcDelay;

    while (cloud.Raining()) {
        HandleInput(&cloud);
        cloud.Rain();
        if (refresh() != OK)
            Die("refresh() failed\n");

        curTime = high_resolution_clock::now();
        elapsed = duration_cast<nanoseconds>(curTime - prevTime);
        if (elapsed >= targetPeriod) {
            calcDelay = nanoseconds(0);
        } else {
            calcDelay = nanoseconds(targetPeriod - elapsed);
        }
        curDelay = (7 * prevDelay + calcDelay) / 8;
        std::this_thread::sleep_for(curDelay);
        prevTime = curTime;
        prevDelay = curDelay;
    }
}

int main(int argc, char* argv[]) {
    ColorMode usrColorMode = ColorMode::INVALID;
    ColorMode colorMode = ColorMode::INVALID;

    ParseArgsEarly(argc, argv, &usrColorMode);

    // Determine whether to use UTF-8 or ASCII based on the locale
    bool ascii = true;
    char* loc = setlocale(LC_ALL, "");
    if (loc && strcasestr(loc, "UTF") != nullptr)
        ascii = false;

    if (InitCurses(usrColorMode, &colorMode) == ERR)
        return ERR;

    double targetFPS = 60.0;
    bool profiling = false;
    Cloud cloud(colorMode, ascii);
    ParseArgs(argc, argv, &cloud, &targetFPS, &profiling);
    cloud.InitChars();
    cloud.Reset();

    if (profiling)
        Profiler(cloud);
    else
        MainLoop(cloud, targetFPS);

    Cleanup();

    return 0;
}
