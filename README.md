# neo

![neo is AWESOME](assets/neo_is_awesome.png)

**WARNING: neo may cause discomfort and seizures in people with photosensitive epilepsy. User discretion is advised.**

**neo** recreates the digital rain effect from "The Matrix". Streams of random
characters will endlessly scroll down your terminal screen.

Cool features:

- Simulates the effect convincingly:
  - Similar color palatte and uneven colors
  - Glitchy characters
  - Half-width katakana characters
- Can display a message similar to the title crawl in the movies
- Unicode support
- Supports 16/256 colors and 32-bit color
- Automatic detection of terminal color and Unicode support
- Handles terminal resizing
- User-defined characters and colors
- Many key controls and command-line options for customization

**neo** is best enjoyed with some good Scotch while listening to Aphex Twin and working on tech.

## Prerequisites

**neo** works with Linux, and it should work with other POSIX-compliant operating systems such as macOS and FreeBSD. Windows is not supported.

The following packages are required to build and run **neo**:

- autoconf: **neo** is built using autotools
- autoconf-archive: required for some of the autoconf checks
- build-essential: make and g++ are used for compilation
- libncurses-dev: **neo** uses ncursesw to control the terminal

You will need to ensure that your C++ compiler supports C++11 and that your autoconf version is at least 2.61. g++ and clang should both work for compilation.

A fast terminal emulator such as Alacritty is highly recommended. neo can be a bit of a CPU hog, especially on large screens with slow terminal emulators.

## Building

See [doc/INSTALL](doc/INSTALL) for more details.

First, clone this repository, open a terminal window, and navigate to the repo directory.

Run the following commands:

```Shell
./autogen.sh
./configure
make -j3
sudo make install
```

## Running

Once **neo** is installed, simply run:

```Shell
neo
```

**neo** has many options and key controls, arguably *too* many, and definitely too many to list here. Check the help message and manual for more info:

```Shell
neo -h
man neo
```

## Screenshots

![In Soviet Russia](assets/in_soviet_russia.png)

![Green Hexadecimal](assets/green_hex.png)

![Golden Greek](assets/golden_greek.png)

## FAQ/Troubleshooting

###
**Q:** **neo** displays garbage characters on the screen. How can this be fixed?

**A:** **neo** will attempt to use half-width katakana characters by default. You may not have a font installed that can display them correctly, or your terminal might not support Unicode well. Try the "--ascii" option or changing your font. You may also need to use the "--mono" option to disable color.

###
**Q:** Colors aren't working. How can this be fixed?

**A:** Make sure your terminal supports colors. Double check if you need to set the TERM environment variable to enable colors. You may want to try the "--16", "--256", or "--truecolor" options.

###
**Q:** How do I disable the blinking characters?

**A:** Use the --noglitch option.

###
**Q:** Can I make the text scroll faster or slower?

**A:** Yes, use the -S/--speed option. Also, the UP and DOWN keys change the speed. The --async option may be fun to try.

###
**Q:** How do I change the colors?

**A:** Use the -c/--color option (e.g. "-c red"). The number keys also change the color while running. Check out the "COLOR FILE" section in the manual if you want to customize **neo** with your own colors.

###
**Q:** How do I change the characters displayed?

**A:** Use the --charset and/or --chars option. You may also need to use the -F/--fullwidth option depending on the characters you selected.

###
**Q:** How do I display a message in the center of the screen?

**A:** Use the -m/--message option. Don't forget to use double quotes!

###
**Q:** neo just shows simple ASCII characters. How can I make it show Unicode characters?

**A:** neo detects if your locale supports Unicode. Typically, your $LANG environment variable should have "UTF" somewhere if it does (e.g. "en_US.UTF-8"). You can use commands such as localectl to change these settings. You can force **neo** to attempt to use Unicode by setting a custom charset (e.g. --charset=extended), but this still may not work due to other OS and terminal settings.

## Bugs

File a GitHub issue. Crashes and build failures will be prioritized. Minor bugs, documentation errors, etc should hopefully get triaged and fixed... eventually.

## Contributing

See [doc/HACKING](doc/HACKING) for more implementation details and a list of things that could be improved.

Requests for enhancement (RFEs) are not likely to be considered or implemented unless they are:

- Within the scope of the original application
- Simple
- Likely to be used by more than one person

The original author deliberately avoided some features present in similar projects (e.g. custom fonts and Windows support) for simplicity.

Pull requests will be handled in a similar manner. Pull requests for bug fixes are more likely to be accepted than new features.

## Acknowledgments

- Chris Allegretta, the original author of cmatrix, and Abishek V Ashok, its current maintainer. cmatrix was a source of inspiration for **neo**.
- Thomas E. Dickey, because **neo** would have been a PITA to write without ncursesw
- Everyone involved in the production of "The Matrix" and the rest of the franchise

## License

**neo** is provided under the GNU GPL v3. See [doc/COPYING](doc/COPYING) for more details.

## Disclaimer

This project is not affiliated with "The Matrix" or "Warner Bros".
