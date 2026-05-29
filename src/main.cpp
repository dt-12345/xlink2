#include "common/args.hpp"
#include "db/db.hpp"
#include "db/username.hpp"
#include "db/version.hpp"

#include <fmt/base.h>

#include <algorithm>

[[nodiscard]] static auto ToLower(const std::string& str) -> std::string {
    auto out = std::string{};
    std::transform(str.begin(), str.end(), std::back_inserter(out), [](char c) { return std::tolower(c); });
    return out;
}

[[nodiscard]] static auto GetGame(std::string_view name) -> mango::Game {
    if (name == "uking") return mango::Game::UKing;
    if (name == "tandem") return mango::Game::Tandem;
    if (name == "turbos") return mango::Game::TurboS;
    if (name == "arms") return mango::Game::ARMS;
    if (name == "blitz") return mango::Game::Blitz;
    if (name == "babel") return mango::Game::Babel;
    if (name == "slope") return mango::Game::Slope;
    if (name == "billy") return mango::Game::Billy;
    if (name == "park") return mango::Game::Park;
    if (name == "stardust") return mango::Game::Stardust;
    if (name == "elsa") return mango::Game::Elsa;
    if (name == "thunder") return mango::Game::Thunder;
    if (name == "exking") return mango::Game::EXKing;
    if (name == "secred") return mango::Game::Secred;
    if (name == "grandes") return mango::Game::GrandeS;
    if (name == "rockstock") return mango::Game::Rockstock;
    if (name == "colony") return mango::Game::Colony;

    fmt::println(stderr, "Failed to match game: {}", name);

    return mango::Game::EXKing;
}

[[nodiscard]] static auto GetPlatform(std::string_view name) -> mango::Platform {
    if (name == "cafe") return mango::Platform::Cafe;
    if (name == "ns") return mango::Platform::NX;
    if (name == "ounce") return mango::Platform::Ounce;

    fmt::println(stderr, "Failed to match platform: {}", name);

    return mango::Platform::NX;
}

enum FileType {
    Auto,
    Binary,
    Text,
};

[[nodiscard]] static auto GetType(std::string_view name) -> FileType {
    if (name == "auto") return Auto;
    if (name == "binary") return Binary;
    if (name == "text") return Text;

    fmt::println("Failed to match file type: {}", name);

    return Auto;
}

struct AppState {
    std::string inputPath = "";
    std::string outputPath = "";
    std::string nameDbPath = "";
    FileType inputType = Auto;
    FileType outputType = Auto;
    mango::Game game = mango::Game::EXKing;
    mango::Platform platform = mango::Platform::NX;
#ifdef XLINK_DEBUG
    bool roundtrip = false;
    bool parseonly = false;
#endif
};

static auto PrintHelpMessage() -> void {
    fmt::print(
        "XLink Tool\n"
        "\n"
        "Basic Usage:\n"
        "  xlink -i <input_file> -o <output_file>\n"
        "\n"
        "Options:\n"
        "  --in, -i:\n"
        "    Input File Path\n"
        "  --out, -o:\n"
        "    Output File Path\n"
        "  --game, -g:\n"
        "    Game (default: EXKing)\n"
        "      Choices:\n"
        "        - UKing        # The Legend of Zelda: Breath of the Wild\n"
        "        - Tandem       # 1-2-Switch\n"
        "        - TurboS       # Mario Kart 8 Deluxe\n"
        "        - ARMS         # Arms\n"
        "        - Blitz        # Splatoon 2\n"
        "        - Babel        # Nintendo Labo Toy-Con 01 - 04, Game Builder Garage\n"
        "        - Slope        # Super Mario Maker 2\n"
        "        - Billy        # Ring Fit Adventure\n"
        "        - Park         # Animal Crossing: New Horizons\n"
        "        - Stardust     # Super Mario 3D All-Stars\n"
        "        - Elsa         # Nintendo Switch Sports\n"
        "        - Thunder      # Splatoon 3\n"
        "        - EXKing       # The Legend of Zelda: Tears of the Kingdom\n"
        "        - Secred       # Super Mario Bros. Wonder\n"
        "        - GrandeS      # Mario vs. Donkey Kong\n"
        "        - Rockstock    # Nintendo Switch Online: Playtest Program\n"
        "        - Colony       # Tomodachi Life: Living the Dream\n"
        "  --platform, -p:\n"
        "    Platform (default: NX)\n"
        "      Choices:\n"
        "        - Cafe         # Wii U\n"
        "        - NX           # Nintendo Switch\n"
        "        - Ounce        # Nintendo Switch 2\n"
        "  --input-type, -it\n"
        "    Input File Type (default: Auto)\n"
        "      Choices:\n"
        "        Auto\n"
        "        Binary\n"
        "        Text\n"
        "  --output-type, -ot\n"
        "    Output File Type (default: Auto)\n"
        "      Choices:\n"
        "        Auto\n"
        "        Binary\n"
        "        Text\n"
        "  --names, -n\n"
        "    Username Database Path\n"
        "      This is a text file with usernames separated by newlines, used to resolve username hashes inside the database\n"
        "  --help, -h\n"
        "    Print Help Message\n"
    );
}

auto main(int argc, const char** argv) -> int {
    auto args = common::CommandLineArgs(argc, argv);
    if (args.finished()) {
        PrintHelpMessage();
        return 0;
    }

    auto state = AppState{};
    while (!args.finished()) {
        const auto arg = args.pop();
        if (arg == "--in" || arg == "-i") {
            state.inputPath = args.pop("Input path");
        } else if (arg == "--out" || arg == "-o") {
            state.outputPath = args.pop("Output path");
        } else if (arg == "--game" || arg == "-g") {
            state.game = GetGame(ToLower(args.pop("Game")));
        } else if (arg == "--platform" || arg == "-p") {
            state.platform = GetPlatform(ToLower(args.pop("Platform")));
        } else if (arg == "--input-type" || arg == "-it") {
            state.inputType = GetType(ToLower(args.pop("Input File Type")));
        } else if (arg == "--output-type" || arg == "-ot") {
            state.outputType = GetType(ToLower(args.pop("Output File Type")));
        } else if (arg == "--names" || arg == "-n") {
            state.nameDbPath = args.pop("Username Database Path");
        } else if (arg == "--help" || arg == "-h") {
            PrintHelpMessage();
            return 0;
#ifdef XLINK_DEBUG
        } else if (arg == "--round-trip" || arg == "-rt") {
            state.roundtrip = true;
        } else if (arg == "--parse-only" || arg == "-po") {
            state.parseonly = true;
#endif
        }
    }

    if (state.inputPath == "") {
        fmt::println(stderr, "Please provide an input file\n");
        PrintHelpMessage();
        return 0;
    }

    if (state.outputPath == "") {
        switch (state.outputType) {
            case Auto:
                switch (state.inputType) {
                    case Auto:
                        if (state.inputPath.ends_with(".txt")) {
                            state.outputPath = state.inputPath + ".bin";
                        } else {
                            state.outputPath = state.inputPath + ".txt";
                        }
                        break;
                    case Binary:
                        state.outputPath = state.inputPath + ".txt";
                        break;
                    case Text:
                        state.outputPath = state.inputPath + ".bin";
                        break;
                }
                break;
            case Binary:
                state.outputPath = state.inputPath + ".bin";
                break;
            case Text:
                state.outputPath = state.inputPath + ".txt";
                break;
        }
    }

    if (state.inputType == Auto) {
        state.inputType = state.inputPath.ends_with(".txt") ? Text : Binary;
    }

    if (state.outputType == Auto) {
        state.outputType = state.outputPath.ends_with(".txt") ? Text : Binary;
    }

    auto db = mango::Database();
    if (state.inputType == Binary) {
        db.load(state.inputPath, state.game, state.platform);
    } else {
        db.parse(state.inputPath);
    }

    if (!state.nameDbPath.empty()) {
        db.resolveUsernames(mango::UsernameDatabase(state.nameDbPath));
    }

#ifdef XLINK_DEBUG
    if (state.parseonly) {
        return 0;
    }

    if (state.roundtrip) {
        auto newDb = mango::Database();
        const auto origText = db.text();
        if (state.inputType == Binary) {
            newDb.load(db.save(state.game, state.platform), state.game, state.platform);
        } else {
            newDb.parse(origText, mango::ParseDataTag{});
        }

        if (origText == newDb.text()) {
            return 0;
        } else {
            fmt::println(stderr, "NOT MATCHING\n");
            return 1;
        }
    }
#endif

    if (state.outputType == Binary) {
        db.save(state.outputPath, state.game, state.platform);
    } else {
        db.text(state.outputPath);
    }

    return 0;
}