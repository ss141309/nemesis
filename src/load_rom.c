/*
Copyright 2025 समीर सिंह Sameer Singh

This file is part of nemesis.

nemesis is free software: you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

nemesis is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with nemesis. If not, see
<https://www.gnu.org/licenses/>. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include "utils.h"

// TODO: support loading from compressed files

// Assume that the ROM size cannot exceed 5MiB, and I am being generous here
static constexpr uint32_t MAX_ROM_SIZE = 5 * 1024 * 1024;
static constexpr uint8_t HEADER_SIZE = 16;
static constexpr uint8_t INES2_EXPONENTIATION_MODE = 0x0F;
static constexpr uint16_t CHR_ROM_UNIT_SIZE = 8 * 1024;
static constexpr uint16_t INES_PRG_RAM_UNIT_SIZE = 8 * 1024;
static constexpr uint16_t PRG_ROM_UNIT_SIZE = 16 * 1024;

// TODO: support more formats
typedef enum { FORMAT_TYPE_INES, FORMAT_TYPE_INES2, FORMAT_TYPE_NONE } format_type_t;
static const char *format_type_string[] = {"INES", "NES2.0", "NONE"};

typedef enum {
  NAMETABLE_VERTICAL,
  NAMETABLE_HORIZONTAL,
  NAMETABLE_NONE
} hard_wired_nametable_layout_t;
static const char *hard_wired_nametable_layout_string[] = {
    "Vertical Arrangement or Mapper Controlled", "Horizontal Arrangement", "None"};

typedef enum {
  CONSOLE_NES,
  CONSOLE_VS_SYSTEM,
  CONSOLE_PLAYCHOICE_10,
  CONSOLE_EXTENDED,
  CONSOLE_NONE
} console_type_t;
static const char *console_type_string[] = {"Nintendo Entertainment System/Family Computer",
                                            "Nintendo Vs. System", "Nintendo Playchoice 10",
                                            "Extended Console Type", "None"};

typedef enum {
  TIMING_NTSC,
  TIMING_PAL,
  TIMING_MULTIPLE,
  TIMING_DENDY,
  TIMING_NONE
} cpu_ppu_timing_t;
static const char *cpu_ppu_timing_string[] = {
    "RP2C02 (\"NTSC NES\") Regions: North America, Japan, South Korea, Taiwan",
    "RP2C07 (\"Licensed PAL NES\") Regions: Western Europe, Australia", "Multiple-region",
    "UA6538 (\"Dendy\") Regions: Eastern Europe, Russia, Mainland China, India, Africa", "None"};

typedef enum {
  VS_PPU_ANY_RP2C03_OR_RC2C03,
  VS_PPU_RP2C04_0001 = 0x02,
  VS_PPU_RP2C04_0002,
  VS_PPU_RP2C04_0003,
  VS_PPU_RP2C04_0004,
  VS_PPU_RC2C05_01 = 0x08,
  VS_PPU_RC2C05_02,
  VS_PPU_RC2C05_03,
  VS_PPU_RC2C05_04,
  VS_PPU_NONE = 0x10
} vs_ppu_type_t;
static const char *vs_ppu_type_string[] = {"Any RP2C03/RC2C03 variant",
                                           "reserved",
                                           "RP2C04_0001",
                                           "RP2C04_0002",
                                           "RP2C04_0003",
                                           "RP2C04_0004",
                                           "reserved",
                                           "reserved",
                                           "RC2C05-01",
                                           "RC2C05-02",
                                           "RC2C05-03",
                                           "RC2C05-04",
                                           "reserved",
                                           "reserved",
                                           "reserved",
                                           "reserved",
                                           "None"};

typedef enum {
  VS_HARDWARE_UNISYSTEM_NORMAL,
  VS_HARDWARE_UNISYSTEM_RBI_BASEBALL,
  VS_HARDWARE_UNISYSTEM_TKO_BOXING,
  VS_HARDWARE_UNISYSTEM_SUPER_XEVIOUS,
  VS_HARDWARE_UNISYSTEM_VS_ICE_CLIMBER_JAPAN,
  VS_HARDWARE_DUAL_SYSTEM_NORMAL,
  VS_HARDWARE_DUAL_SYSTEM_RAID_ON_BUNGELING_BAY,
  VS_HARDWARE_NONE
} vs_hardware_type_t;
static const char *vs_hardware_type_string[] = {
    "Vs. Unisystem (normal)",
    "Vs. Unisystem (RBI Baseball protection)",
    "Vs. Unisystem (TKO Boxing protection)",
    "Vs. Unisystem (Super Xevious protection)",
    "Vs. Unisystem (Vs. Ice Climber Japan protection)",
    "Vs. Dual System (normal)",
    "Vs. Dual System (Raid on Bungeling Bay protection)",
    "None"};

typedef enum {
  EXTENDED_CONSOLE_REGULAR_NES_FAMICOM_DENDY,
  EXTENDED_CONSOLE_VS_SYSTEM,
  EXTENDED_CONSOLE_PLAYCHOICE_10,
  EXTENDED_CONSOLE_REGULAR_FAMICLONE,
  EXTENDED_CONSOLE_REGULAR_NES_WITH_EPSM,
  EXTENDED_CONSOLE_VR_TECHNOLOGY_VT01,
  EXTENDED_CONSOLE_VR_TECHNOLOGY_VT02,
  EXTENDED_CONSOLE_VR_TECHNOLOGY_VT03,
  EXTENDED_CONSOLE_VR_TECHNOLOGY_VT09,
  EXTENDED_CONSOLE_VR_TECHNOLOGY_VT32,
  EXTENDED_CONSOLE_VR_TECHNOLOGY_VT369,
  EXTENDED_CONSOLE_UMC_UM6578,
  EXTENDED_CONSOLE_FAMICOM_NETWORK_SYSTEM,
  EXTENDED_CONSOLE_NONE = 0x10
} extended_console_type_t;
static const char *extended_console_type_string[] = {
    "Regular NES/Famicom/Dendy",
    "Nintendo Vs. System",
    "Playchoice 10",
    "Regular Famiclone, but with CPU that supports Decimal Mode",
    "Regular NES/Famicom with EPSM module or plug-through cartridge",
    "V.R. Technology VT01 with red/cyan STN palette",
    "V.R. Technology VT02",
    "V.R. Technology VT03",
    "V.R. Technology VT09",
    "V.R. Technology VT32",
    "V.R. Technology VT369",
    "UMC UM6578",
    "Famicom Network System",
    "reserved",
    "reserved",
    "reserved",
    "None"};

typedef enum {
  DEFAULT_EXPANSION_DEVICE_UNSPECIFIED,
  DEFAULT_EXPANSION_DEVICE_STANDARD_NES_CONTROLLERS,
  DEFAULT_EXPANSION_DEVICE_NES_FOUR_SCORE_SATELLITE,
  DEFAULT_EXPANSION_DEVICE_FAMICOM_FOUR_PLAYERS_ADAPTER,
  DEFAULT_EXPANSION_DEVICE_VS_SYSTEM_1P_VIA_4016,
  DEFAULT_EXPANSION_DEVICE_VS_SYSTEM_1P_VIA_4017,
  DEFAULT_EXPANSION_DEVICE_RESERVED,
  DEFAULT_EXPANSION_DEVICE_VS_ZAPPER,
  DEFAULT_EXPANSION_DEVICE_ZAPPER_4017,
  DEFAULT_EXPANSION_DEVICE_TWO_ZAPPERS,
  DEFAULT_EXPANSION_DEVICE_BANDAI_HYPER_SHOT_LIGHTGUN,
  DEFAULT_EXPANSION_DEVICE_POWER_PAD_SIDE_A,
  DEFAULT_EXPANSION_DEVICE_POWER_PAD_SIDE_B,
  DEFAULT_EXPANSION_DEVICE_FAMILY_TRAINER_SIDE_A,
  DEFAULT_EXPANSION_DEVICE_FAMILY_TRAINER_SIDE_B,
  DEFAULT_EXPANSION_DEVICE_ARKANOID_VAUS_CONTROLLER_NES,
  DEFAULT_EXPANSION_DEVICE_ARKANOID_VAUS_CONTROLLER_FAMICOM,
  DEFAULT_EXPANSION_DEVICE_TWO_VAUS_CONTROLLERS,
  DEFAULT_EXPANSION_DEVICE_KONAMI_HYPER_SHOT_CONTROLLER,
  DEFAULT_EXPANSION_DEVICE_COCONUTS_PACHINKO_CONTROLLER,
  DEFAULT_EXPANSION_DEVICE_EXCITING_BOXING_PUNCHING_BAG,
  DEFAULT_EXPANSION_DEVICE_JISSEN_MAHJONG_CONTROLLER,
  DEFAULT_EXPANSION_DEVICE_PARTY_TAP,
  DEFAULT_EXPANSION_DEVICE_OEKA_KIDS_TABLET,
  DEFAULT_EXPANSION_DEVICE_SUNSOFT_BARCODE_BATTLER,
  DEFAULT_EXPANSION_DEVICE_MIRACLE_PIANO_KEYBOARD,
  DEFAULT_EXPANSION_DEVICE_POKKUN_MOGURAA,
  DEFAULT_EXPANSION_DEVICE_TOP_RIDER,
  DEFAULT_EXPANSION_DEVICE_DOUBLE_FISTED,
  DEFAULT_EXPANSION_DEVICE_FAMICOM_3D_SYSTEM,
  DEFAULT_EXPANSION_DEVICE_DOREMIKKO_KEYBOARD,
  DEFAULT_EXPANSION_DEVICE_ROB_GYRO_SET,
  DEFAULT_EXPANSION_DEVICE_FAMICOM_DATA_RECORDER,
  DEFAULT_EXPANSION_DEVICE_ASCII_TURBO_FILE,
  DEFAULT_EXPANSION_DEVICE_IGS_STORAGE_BATTLE_BOX,
  DEFAULT_EXPANSION_DEVICE_FAMILY_BASIC_KEYBOARD_PLUS_FAMICOM_DATA_RECORDER,
  DEFAULT_EXPANSION_DEVICE_DONGDA_PEC_KEYBOARD,
  DEFAULT_EXPANSION_DEVICE_PUZE_BIT_79_KEYBOARD,
  DEFAULT_EXPANSION_DEVICE_XIAOBAWANG_KEYBOARD,
  DEFAULT_EXPANSION_DEVICE_XIAOBAWANG_KEYBOARD_PLUS_MOUSE_3X8_BIT_PROTOCOL,
  DEFAULT_EXPANSION_DEVICE_XIAOBAWANG_KEYBOARD_PLUS_MOUSE_24_BIT_PROTOCOL_VIA_4016,
  DEFAULT_EXPANSION_DEVICE_SNES_MOUSE_4016_D0,
  DEFAULT_EXPANSION_DEVICE_MULTICART,
  DEFAULT_EXPANSION_DEVICE_TWO_SNES_CONTROLLERS,
  DEFAULT_EXPANSION_DEVICE_RACERMATE_BICYCLE,
  DEFAULT_EXPANSION_DEVICE_UFORCE,
  DEFAULT_EXPANSION_DEVICE_ROB_STACK_UP,
  DEFAULT_EXPANSION_DEVICE_CITY_PATROLMAN_LIGHTGUN,
  DEFAULT_EXPANSION_DEVICE_SHARP_C1_CASSETTE_INTERFACE,
  DEFAULT_EXPANSION_DEVICE_STANDARD_CONTROLLER_WITH_SWAP,
  DEFAULT_EXPANSION_DEVICE_EXCALIBUR_SUDOKU_PAD,
  DEFAULT_EXPANSION_DEVICE_ABL_PINBALL,
  DEFAULT_EXPANSION_DEVICE_GOLDEN_NUGGET_CASINO_EXTRA_BUTTONS,
  DEFAULT_EXPANSION_DEVICE_KEDA_KEYBOARD,
  DEFAULT_EXPANSION_DEVICE_XIAOBAWANG_KEYBOARD_PLUS_MOUSE_24_BIT_PROTOCOL_VIA_4017,
  DEFAULT_EXPANSION_DEVICE_PORT_TEST_CONTROLLER,
  DEFAULT_EXPANSION_DEVICE_BANDAI_MULTI_GAME_PLAYER_GAMEPAD_BUTTONS,
  DEFAULT_EXPANSION_DEVICE_VENOM_TV_DANCE_MAT,
  DEFAULT_EXPANSION_DEVICE_LG_TV_REMOTE_CONTROL,
  DEFAULT_EXPANSION_DEVICE_FAMICOM_NETWORK_CONTROLLER,
  DEFAULT_EXPANSION_DEVICE_KING_FISHING_CONTROLLER,
  DEFAULT_EXPANSION_DEVICE_CROAKY_KARAOKE_CONTROLLER,
  DEFAULT_EXPANSION_DEVICE_KEWANG_KEYBOARD,
  DEFAULT_EXPANSION_DEVICE_ZECHENG_KEYBOARD,
  DEFAULT_EXPANSION_DEVICE_XIAOBAWANG_KEYBOARD_PLUS_ROTATED_PS2_MOUSE_IN_4017,
  DEFAULT_EXPANSION_DEVICE_PS2_KEYBOARD_IN_UM6578_PS2_PORT_VIA_4017,
  DEFAULT_EXPANSION_DEVICE_PS2_MOUSE_IN_UM6578_PS2_PORT,
  DEFAULT_EXPANSION_DEVICE_YUXING_MOUSE_VIA_4016,
  DEFAULT_EXPANSION_DEVICE_XIAOBAWANG_KEYBOARD_PLUS_YUXING_MOUSE_IN_4016,
  DEFAULT_EXPANSION_DEVICE_GIGGGLE_TV_PUMP,
  DEFAULT_EXPANSION_DEVICE_BUBUGAO_KEYBOARD_PLUS_ROTATED_PS2_MOUSE_IN_4017,
  DEFAULT_EXPANSION_DEVICE_MAGICAL_COOKING,
  DEFAULT_EXPANSION_DEVICE_SNES_MOUSE_4017_D0,
  DEFAULT_EXPANSION_DEVICE_ZAPPER_4016,
  DEFAULT_EXPANSION_DEVICE_ARKANOID_VAUS_CONTROLLER_PROTOTYPE
} default_expansion_device_t;
static const char *default_expansion_device_string[] = {
    "Unspecified", "Standard NES/Famicom controllers",
    "NES Four Score/Satellite with two additional standard controllers",
    // clang-format off
	"Famicom Four Players Adapter with two additional standard controllers using the \"simple\" protocol",
    // clang-format on
    "Vs. System (1P via $4016)", "Vs. System (1P via $4017)", "Reserved", "Vs. Zapper",
    "Zapper ($4017)", "Two Zappers", "Bandai Hyper Shot Lightgun", "Power Pad Side A",
    "Power Pad Side B", "Family Trainer Side A", "Family Trainer Side B",
    "Arkanoid Vaus Controller (NES)", "Arkanoid Vaus Controller (Famicom)",
    "Two Vaus Controllers plus Famicom Data Recorder", "Konami Hyper Shot Controller",
    "Coconuts Pachinko Controller", "Exciting Boxing Punching Bag (Blowup Doll)",
    "Jissen Mahjong Controller", "Party Tap", "Oeka Kids Tablet", "Sunsoft Barcode Battler",
    "Miracle Piano Keyboard", "Pokkun Moguraa (Whack-a-Mole Mat and Mallet)",
    "Top Rider (Inflatable Bicycle)",
    "Double-Fisted (Requires or allows use of two controllers by one player)", "Famicom 3D System",
    "Doremikko Keyboard", "R.O.B. Gyro Set", "Famicom Data Recorder (\"silent\" keyboard)",
    "ASCII Turbo File", "IGS Storage Battle Box",
    "Family BASIC Keyboard plus Famicom Data Recorder", "东达 (Dōngdá) PEC Keyboard",
    "普澤 (Pǔzé, a.k.a. Bit Corp.) Bit-79 Keyboard", "小霸王 (Xiǎobàwáng, a.k.a. Subor) Keyboard",
    "小霸王 (Xiǎobàwáng, a.k.a. Subor) Keyboard plus mouse (3x8-bit protocol)",
    "小霸王 (Xiǎobàwáng, a.k.a. Subor) Keyboard plus mouse (24-bit protocol via $4016)",
    "SNES Mouse ($4016.d0)", "Multicart",
    "Two SNES controllers replacing the two standard NES controllers", "RacerMate Bicycle",
    "U-Force", "R.O.B. Stack-Up", "City Patrolman Lightgun", "Sharp C1 Cassette Interface",
    "Standard Controller with swapped Left-Right/Up-Down/B-A", "Excalibur Sudoku Pad",
    "ABL Pinball", "Golden Nugget Casino extra buttons", "科达 (Kēdá) Keyboard",
    "小霸王 (Xiǎobàwáng, a.k.a. Subor) Keyboard plus mouse (24-bit protocol via $4017)",
    "Port test controller", "Bandai Multi Game Player Gamepad buttons", "Venom TV Dance Mat",
    "LG TV Remote Control", "Famicom Network Controller", "King Fishing Controller",
    "Croaky Karaoke Controller", "科王 (Kēwáng, a.k.a. Kingwon) Keyboard",
    "泽诚 (Zéchéng) Keyboard",
    "小霸王 (Xiǎobàwáng, a.k.a. Subor) Keyboard plus rotated PS/2 mouse in $4017",
    "PS/2 Keyboard in UM6578 PS/2 port, PS/2 Mouse via $4017", "PS/2 Mouse in UM6578 PS/2 port",
    "裕兴 (Yùxìng) Mouse via $4016",
    "小霸王 (Xiǎobàwáng, a.k.a. Subor) Keyboard plus 裕兴 (Yùxìng) mouse in $4016",
    "Gigggle TV Pump", "步步高 (Bùbùgāo, a.k.a. BBK) Keyboard plus rotated PS/2 mouse in $4017",
    "Magical Cooking", "SNES Mouse ($4017.d0)", "Zapper ($4016)",
    "Arkanoid Vaus Controller (Prototype)"};

typedef enum { TV_SYSTEM_NTSC, TV_SYSTEM_PAL, TV_SYSTEM_NONE } tv_system_t;
static const char *tv_system_string[] = {"NTSC", "PAL", "None"};

typedef struct {
  bool trainer_area_exists;
  bool battery_present;
  size_t prg_rom_size;
  size_t chr_rom_size;
  uint16_t mapper_number;
  uint8_t submapper_number;
  hard_wired_nametable_layout_t hard_wired_nametable_layout;
  bool alternative_nametables;
  console_type_t console_type;
  uint32_t prg_ram_size;
  uint32_t prg_nvram_size;
  uint32_t chr_ram_size;
  uint32_t chr_nvram_size;
  cpu_ppu_timing_t cpu_ppu_timing;
  vs_ppu_type_t vs_ppu_type;
  vs_hardware_type_t vs_hardware_type;
  extended_console_type_t extended_console_type;
  uint8_t misc_rom_number;
  ptrdiff_t misc_rom_area_size;
  default_expansion_device_t default_expansion_device;
} ines2_header_t;

typedef struct {
  bool trainer_area_exists;
  bool battery_present;
  bool chr_ram_exists;
  size_t prg_rom_size;
  size_t chr_rom_size;
  hard_wired_nametable_layout_t hard_wired_nametable_layout;
  bool alternative_nametables;
  uint8_t mapper_number;
  console_type_t console_type;
  uint32_t prg_ram_size;
  tv_system_t tv_system;
} ines_header_t;

typedef struct {
  format_type_t format_type;
  uint8_t rom_data[MAX_ROM_SIZE];
  size_t rom_size;
  ines_header_t ines_header;
  ines2_header_t ines2_header;
} cartridge_t;

cartridge_t cartridge = {
    FORMAT_TYPE_NONE,
    {},
    0,
    {false, false, false, 0, 0, NAMETABLE_NONE, false, 0, CONSOLE_NONE, 0, TV_SYSTEM_NONE},
    {false,
     false,
     0,
     0,
     0,
     0,
     NAMETABLE_NONE,
     false,
     CONSOLE_NONE,
     0,
     0,
     0,
     0,
     TIMING_NONE,
     VS_PPU_NONE,
     VS_HARDWARE_NONE,
     EXTENDED_CONSOLE_NONE,
     0,
     0,
     DEFAULT_EXPANSION_DEVICE_UNSPECIFIED}};

private
void cleanup_file(FILE **fp) {
  if (*fp) {
    fclose(*fp);
    *fp = nullptr;
  }
}

[[nodiscard]] private
bool load_rom_file(const char *file_path) {
  return_value_if(file_path == nullptr, false, ERR_NULL_FILEPATH);

  // prevent file pointer leak, as the file will never be closed if we return early in case of an
  // error
  FILE *rom_filep __attribute__((cleanup(cleanup_file))) = fopen(file_path, "rb");
  return_value_if(rom_filep == nullptr, false, "cannot read file: %s", file_path);

  // get the file size
  struct stat st;
  stat(file_path, &st);
  off_t file_size = st.st_size;
  return_value_if(file_size < 0, false, ERR_INVALID_FILE_SIZE);
  return_value_if(file_size > MAX_ROM_SIZE, false, ERR_ROM_FILE_TOO_LARGE);

  size_t bytes_read = fread(cartridge.rom_data, sizeof(uint8_t), file_size, rom_filep);
  return_value_if(bytes_read < (size_t)file_size, false, "cannot read file: %s", file_path);

  cartridge.rom_size = bytes_read;
  log_info("Loaded ROM: %s", get_filename_from_path(file_path));

  if (bytes_read > 1024 * 1024) {
    log_info("ROM size: %.2fMiB", (double)bytes_read / (1024 * 1024));
  } else {
    log_info("ROM size: %.2fKiB", (double)bytes_read / 1024);
  }

  return true;
}

[[nodiscard]] private
bool ines2_set_prg_rom_size(const uint8_t *header) {
  uint8_t prg_lsb = header[4];
  uint8_t prg_msb = get_lower_4_bits(header[9]);

  if (prg_msb == INES2_EXPONENTIATION_MODE) {
    uint8_t multiplier = (uint8_t)(get_lower_2_bits(prg_lsb) * 2 + 1);
    uint8_t exponent = get_upper_6_bits(prg_lsb);

    return_value_if(1UL << exponent > SIZE_MAX / multiplier, false, ERR_ARITHMETIC_OVERFLOW);
    cartridge.ines2_header.prg_rom_size = (1UL << exponent) * multiplier;
  } else {
    cartridge.ines2_header.prg_rom_size = ((prg_msb << 4) | prg_lsb) * PRG_ROM_UNIT_SIZE;
  }

  if (cartridge.ines2_header.prg_rom_size > 1024 * 1024) {
    log_info("PRG ROM size: %.2fMiB", (double)cartridge.ines2_header.prg_rom_size / (1024 * 1024));
  } else {
    log_info("PRG ROM size: %.2fKiB", (double)cartridge.ines2_header.prg_rom_size / 1024);
  }

  return true;
}

[[nodiscard]] private
bool ines2_set_chr_rom_size(const uint8_t *header) {
  uint8_t chr_lsb = header[5];
  uint8_t chr_msb = get_upper_4_bits(header[9]);

  if (chr_msb == INES2_EXPONENTIATION_MODE) {
    uint8_t multiplier = (uint8_t)(get_lower_2_bits(chr_lsb) * 2 + 1);
    uint8_t exponent = get_upper_6_bits(chr_lsb);

    return_value_if(1UL << exponent > SIZE_MAX / multiplier, false, ERR_ARITHMETIC_OVERFLOW);
    cartridge.ines2_header.chr_rom_size = (1UL << exponent) * multiplier;
  } else {
    cartridge.ines2_header.chr_rom_size = ((chr_msb << 4) | chr_lsb) * CHR_ROM_UNIT_SIZE;
  }

  if (cartridge.ines2_header.chr_rom_size > 1024 * 1024) {
    log_info("CHR ROM size: %.2fMiB", (double)cartridge.ines2_header.chr_rom_size / (1024 * 1024));
  } else {
    log_info("CHR ROM size: %.2fKiB", (double)cartridge.ines2_header.chr_rom_size / 1024);
  }

  return true;
}

private
void ines2_set_mapper_number(const uint8_t *header) {
  uint8_t mapper_bits_0_3 = get_upper_4_bits(header[6]);
  uint8_t mapper_bits_4_7 = get_upper_4_bits(header[7]);
  uint8_t mapper_bits_8_11 = get_lower_4_bits(header[8]);

  cartridge.ines2_header.mapper_number =
      (uint16_t)((mapper_bits_8_11 << 8) | (mapper_bits_4_7 << 4) | mapper_bits_0_3);

  log_info("Mapper Number: %d", cartridge.ines2_header.mapper_number);
}

private
void ines2_set_submapper_number(const uint8_t *header) {
  cartridge.ines2_header.submapper_number = get_upper_4_bits(header[8]);
  log_info("Submapper Number: %d", cartridge.ines2_header.submapper_number);
}

private
void ines2_set_nametable_layout(const uint8_t *header) {
  cartridge.ines2_header.hard_wired_nametable_layout = get_0th_bit(header[6]);
  cartridge.ines2_header.alternative_nametables = get_3rd_bit(header[6]);

  log_info("Hard wired nametable layout type: %s",
           hard_wired_nametable_layout_string[cartridge.ines2_header.hard_wired_nametable_layout]);

  log_info("Alternative nametables present? %s",
           cartridge.ines2_header.alternative_nametables ? "Yes" : "No");
}

private
void ines2_check_battery_present(const uint8_t *header) {
  cartridge.ines2_header.battery_present = get_1st_bit(header[6]);
  log_info("Is \"Battery\" and other non-volatile memory present? %s",
           cartridge.ines2_header.battery_present ? "Yes" : "No");
}

private
void ines2_check_trainer_area_present(const uint8_t *header) {
  cartridge.ines2_header.trainer_area_exists = get_2nd_bit(header[6]);
  log_info("Does trainer area exist? %s",
           cartridge.ines2_header.trainer_area_exists ? "Yes" : "No");
}

private
void ines2_set_console_type(const uint8_t *header) {
  cartridge.ines2_header.console_type = get_lower_2_bits(header[7]);
  log_info("Console type: %s", console_type_string[cartridge.ines2_header.console_type]);
}

private
void ines2_set_prg_ram_sizes(const uint8_t *header) {
  uint8_t prg_ram_shift_count = get_lower_4_bits(header[10]);
  uint8_t prg_nvram_shift_count = get_upper_4_bits(header[10]);

  cartridge.ines2_header.prg_ram_size = (prg_ram_shift_count == 0) ? 0 : 64 << prg_ram_shift_count;
  cartridge.ines2_header.prg_nvram_size =
      (prg_nvram_shift_count == 0) ? 0 : 64 << prg_nvram_shift_count;

  if (cartridge.ines2_header.prg_ram_size > 1024 * 1024) {
    log_info("PRG RAM size: %.2fMiB", (double)cartridge.ines2_header.prg_ram_size / (1024 * 1024));
  } else {
    log_info("PRG RAM size: %.2fKiB", (double)cartridge.ines2_header.prg_ram_size / 1024);
  }

  if (cartridge.ines2_header.prg_nvram_size > 1024 * 1024) {
    log_info("PRG NVRAM size: %.2fMiB",
             (double)cartridge.ines2_header.prg_nvram_size / (1024 * 1024));
  } else {
    log_info("PRG NVRAM size: %.2fKiB", (double)cartridge.ines2_header.prg_nvram_size / 1024);
  }

  if (cartridge.ines2_header.prg_nvram_size > 0 && !cartridge.ines2_header.battery_present) {
    log_warn(
        "This ROM is not compatible with iNES only emulators as the PRG NVRAM size is > 0 while "
        "the "
        "battery is not present");
  }
}

private
void ines2_set_chr_ram_sizes(const uint8_t *header) {
  uint8_t chr_ram_shift_count = get_lower_4_bits(header[11]);
  uint8_t chr_nvram_shift_count = get_upper_4_bits(header[11]);

  cartridge.ines2_header.chr_ram_size = (chr_ram_shift_count == 0) ? 0 : 64 << chr_ram_shift_count;
  cartridge.ines2_header.chr_nvram_size =
      (chr_nvram_shift_count == 0) ? 0 : 64 << chr_nvram_shift_count;

  if (cartridge.ines2_header.chr_ram_size > 1024 * 1024) {
    log_info("CHR RAM size: %.2fMiB", (double)cartridge.ines2_header.chr_ram_size / (1024 * 1024));
  } else {
    log_info("CHR RAM size: %.2fKiB", (double)cartridge.ines2_header.chr_ram_size / 1024);
  }

  if (cartridge.ines2_header.chr_nvram_size > 1024 * 1024) {
    log_info("CHR NVRAM size: %.2fMiB",
             (double)cartridge.ines2_header.chr_nvram_size / (1024 * 1024));
  } else {
    log_info("CHR NVRAM size: %.2fKiB", (double)cartridge.ines2_header.chr_nvram_size / 1024);
  }
}

private
void ines2_set_cpu_ppu_timing(const uint8_t *header) {
  cartridge.ines2_header.cpu_ppu_timing = get_lower_2_bits(header[12]);
  log_info("CPU/PPU timing mode: %s", cpu_ppu_timing_string[cartridge.ines2_header.cpu_ppu_timing]);
}

private
void detect_rom_format(const uint8_t *header) {
  if (header[0] == 'N' && header[1] == 'E' && header[2] == 'S' && header[3] == 0x1A) {
    cartridge.format_type = FORMAT_TYPE_INES;
  }

  if (cartridge.format_type == FORMAT_TYPE_INES && (header[7] & 0x0C) == 0x08) {
    cartridge.format_type = FORMAT_TYPE_INES2;
  }

  log_info("ROM Format: %s", format_type_string[cartridge.format_type]);
}

private
void ines2_set_extended_system_type(const uint8_t *header) {
  switch (cartridge.ines2_header.console_type) {
    case CONSOLE_VS_SYSTEM:
      cartridge.ines2_header.vs_ppu_type = get_lower_4_bits(header[13]);
      cartridge.ines2_header.vs_hardware_type = get_upper_4_bits(header[13]);

      log_info("Vs. PPU Type: %s", vs_ppu_type_string[cartridge.ines2_header.vs_ppu_type]);
      log_info("Vs. Hardware Type: %s",
               vs_hardware_type_string[cartridge.ines2_header.vs_hardware_type]);
      break;

    case CONSOLE_EXTENDED:
      cartridge.ines2_header.extended_console_type = get_lower_4_bits(header[13]);
      log_info("Extended Console Type: %s",
               extended_console_type_string[cartridge.ines2_header.extended_console_type]);
      break;

    default:
      return;
  }
}

[[nodiscard]] private
bool ines2_set_misc_rom_area_size(const uint8_t *header) {
  cartridge.ines2_header.misc_rom_number = get_lower_2_bits(header[14]);

  // convert the types of the sizes to their signed equivalent for easier overflow check.
  // I did not changed the types of the fields itself because I do not want to go back and check
  // what effect it will have on the rest of the code. इतनी माथापच्ची नहीं करनी ।
  ptrdiff_t signed_rom_size = cartridge.rom_size;
  ptrdiff_t signed_header_size = HEADER_SIZE;
  ptrdiff_t signed_trainer_area_size = cartridge.ines2_header.trainer_area_exists ? 512 : 0;
  ptrdiff_t signed_prg_rom_size = cartridge.ines2_header.prg_rom_size;
  ptrdiff_t signed_chr_rom_size = cartridge.ines2_header.chr_rom_size;

  ptrdiff_t total_other_sizes =
      signed_header_size + signed_trainer_area_size + signed_prg_rom_size + signed_chr_rom_size;

  return_value_if(signed_header_size > PTRDIFF_MAX - signed_trainer_area_size -
                                           signed_prg_rom_size - signed_chr_rom_size,
                  false, ERR_ARITHMETIC_OVERFLOW);

  return_value_if(signed_rom_size < total_other_sizes, false, ERR_INVALID_MISC_ROM_AREA_SIZE);

  cartridge.ines2_header.misc_rom_area_size = signed_rom_size - total_other_sizes;

  log_info("Number of miscellaneous ROMs present: %d", cartridge.ines2_header.misc_rom_number);
  log_info("Miscellaneous ROM area size: %ld", cartridge.ines2_header.misc_rom_area_size);

  return true;
}

private
void ines2_set_default_expansion_device(const uint8_t *header) {
  cartridge.ines2_header.default_expansion_device = get_lower_6_bits(header[15]);
  log_info("Default Expansion Device: %s",
           default_expansion_device_string[cartridge.ines2_header.default_expansion_device]);
}

private
void ines_set_prg_rom_size(const uint8_t *header) {
  cartridge.ines_header.prg_rom_size = header[4] * PRG_ROM_UNIT_SIZE;

  if (cartridge.ines_header.prg_rom_size > 1024 * 1024) {
    log_info("PRG ROM size: %.2fMiB", (double)cartridge.ines_header.prg_rom_size / (1024 * 1024));
  } else {
    log_info("PRG ROM size: %.2fKiB", (double)cartridge.ines_header.prg_rom_size / 1024);
  }
}

private
void ines_set_chr_rom_size(const uint8_t *header) {
  cartridge.ines_header.chr_rom_size = header[5] * CHR_ROM_UNIT_SIZE;
  cartridge.ines_header.chr_ram_exists = (header[5] == 0) ? true : false;

  if (cartridge.ines_header.chr_rom_size > 1024 * 1024) {
    log_info("CHR ROM size: %.2fMiB", (double)cartridge.ines_header.chr_rom_size / (1024 * 1024));
  } else {
    log_info("CHR ROM size: %.2fKiB", (double)cartridge.ines_header.chr_rom_size / 1024);
  }

  log_info("CHR RAM exists? %s", cartridge.ines_header.chr_ram_exists ? "Yes" : "No");
}

private
void ines_set_nametable_layout(const uint8_t *header) {
  cartridge.ines_header.hard_wired_nametable_layout = get_0th_bit(header[6]);
  cartridge.ines_header.alternative_nametables = get_3rd_bit(header[6]);

  log_info("Hard wired nametable layout type: %s",
           hard_wired_nametable_layout_string[cartridge.ines_header.hard_wired_nametable_layout]);

  log_info("Alternative nametables present? %s",
           cartridge.ines_header.alternative_nametables ? "Yes" : "No");
}

private
void ines_check_battery_present(const uint8_t *header) {
  cartridge.ines_header.battery_present = get_1st_bit(header[6]);
  log_info("Is \"Battery\" and other non-volatile memory present? %s",
           cartridge.ines_header.battery_present ? "Yes" : "No");
}

private
void ines_check_trainer_area_present(const uint8_t *header) {
  cartridge.ines_header.trainer_area_exists = get_2nd_bit(header[6]);
  log_info("Does trainer area exist? %s", cartridge.ines_header.trainer_area_exists ? "Yes" : "No");
}

private
void ines_set_mapper_number(const uint8_t *header) {
  uint8_t mapper_lo = get_upper_4_bits(header[6]);
  uint8_t mapper_hi = get_upper_4_bits(header[7]);

  cartridge.ines_header.mapper_number = (uint8_t)(mapper_hi << 4) | mapper_lo;

  log_info("Mapper Number: %d", cartridge.ines_header.mapper_number);
}

private
void ines_set_console_type(const uint8_t *header) {
  cartridge.ines_header.console_type = get_lower_2_bits(header[7]);

  if (cartridge.ines_header.console_type == CONSOLE_EXTENDED) {
    log_warn("Console type cannot be both VS Unisystem and PlayChoice-10 simultaneously");
  }

  log_info("Console type: %s", console_type_string[cartridge.ines_header.console_type]);
}

private
void ines_set_prg_ram_size(const uint8_t *header) {
  cartridge.ines_header.prg_ram_size =
      (header[8] == 0) ? 1 * INES_PRG_RAM_UNIT_SIZE : header[8] * INES_PRG_RAM_UNIT_SIZE;

  if (cartridge.ines_header.prg_ram_size > 1024 * 1024) {
    log_info("PRG RAM size: %.2fMiB", (double)cartridge.ines_header.prg_ram_size / (1024 * 1024));
  } else {
    log_info("PRG RAM size: %.2fKiB", (double)cartridge.ines_header.prg_ram_size / 1024);
  }
}

private
void ines_set_tv_system(const uint8_t *header) {
  cartridge.ines_header.tv_system = get_0th_bit(header[9]);
  log_info("TV system: %s", tv_system_string[cartridge.ines_header.tv_system]);
}

[[nodiscard]] private
bool fill_header(void) {
  // ines and nes2.0 formats have 16B header size
  uint8_t header[HEADER_SIZE] = {};
  bool out = false;

  memcpy(header, cartridge.rom_data, HEADER_SIZE);

  detect_rom_format(header);

  switch (cartridge.format_type) {
    case FORMAT_TYPE_INES:
      ines_set_prg_rom_size(header);
      ines_set_chr_rom_size(header);
      ines_set_nametable_layout(header);
      ines_check_battery_present(header);
      ines_check_trainer_area_present(header);
      ines_set_mapper_number(header);
      ines_set_console_type(header);
      ines_set_prg_ram_size(header);
      ines_set_tv_system(header);
      break;
    case FORMAT_TYPE_INES2:
      ines2_check_trainer_area_present(header);
      out = ines2_set_prg_rom_size(header);
      out = ines2_set_chr_rom_size(header);
      out = ines2_set_misc_rom_area_size(header);
      ines2_set_mapper_number(header);
      ines2_set_submapper_number(header);
      ines2_set_nametable_layout(header);
      ines2_check_battery_present(header);
      ines2_set_console_type(header);
      ines2_set_prg_ram_sizes(header);
      ines2_set_chr_ram_sizes(header);
      ines2_set_cpu_ppu_timing(header);
      ines2_set_extended_system_type(header);
      ines2_set_default_expansion_device(header);
      break;
    default:
      log_warn(ERR_ROM_TYPE_NOT_SUPPORTED);
  }

  return_value_if(out, false, "Could not set one or more ROM area sizes");
  return true;
}
