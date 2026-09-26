/*
 * Smc_tx.c -- translation-unit record placeholder, NOT reconstructed content.
 *
 * The reference object carries an `STT_FILE` record spelled `Smc_tx.c`
 * immediately after `Smc.c` (208); the symbol-table run from the record to the
 * next carries no symbol in any section (F11405).  This tree had no input with
 * that spelling, so the partial link's FILE set could not match the original
 * build.
 *
 * This empty translation unit reproduces exactly that FILE record and nothing
 * else: measured on the period compiler it emits the FILE symbol plus the five
 * empty-section symbols an empty input always emits, and `ld -r` adds no bytes
 * to any section.  It is a file-set representation, not recovered content.
 */
