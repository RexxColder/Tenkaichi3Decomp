/* Pass this file to the linker with the `-T linker_script_extra.ld` flag */
ENTRY(_start);

/* Menu overlay (DBZP.BIN, loaded at 0x334C00) symbols that C files of the main executable refer to
 * (src/sys/late_a*.c). splat only writes undefined_*_auto entries for references made from assembly.
 * Replace the placeholders when the overlay's symbol files (config/symbols/menu_*.txt) are listed. */
D_003B0EB4 = 0x003B0EB4;
func_00399240 = 0x00399240;
func_00399430 = 0x00399430;
func_00399478 = 0x00399478;
func_00399730 = 0x00399730;
func_00399760 = 0x00399760;
