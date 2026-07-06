/*
 * dicom_reader.c - Reads a .dcm file and prints every tag in it.
 *
 * Usage: ./dicom_reader path/to/file.dcm
 *
 * What this does NOT do (on purpose, to keep it readable):
 *   - Decode pixel data into an image (just reports its length/offset)
 *   - Handle Explicit VR Big Endian or compressed transfer syntaxes
 *   - Handle sequences (SQ) recursively with nested item parsing beyond
 *     skipping their bytes correctly
 *   - Have a complete tag dictionary (~40 common tags only, see dict.h)
 *
 * What it DOES do:
 *   - Validate the 128-byte preamble + "DICM" magic
 *   - Parse the File Meta group (always Explicit VR LE) to find the
 *     Transfer Syntax UID
 *   - Switch to Implicit VR LE or Explicit VR LE parsing for the dataset
 *     based on that UID
 *   - Print (group,element) VR length name: value for every element
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "dict.h"

/* The two transfer syntaxes we actually decode the dataset for. */
#define TS_IMPLICIT_VR_LE   "1.2.840.10008.1.2"
#define TS_EXPLICIT_VR_LE   "1.2.840.10008.1.2.1"

typedef struct {
    FILE *fp;
    long file_size;
    char transfer_syntax[65];   /* UID string, e.g. "1.2.840.10008.1.2.1" */
    int implicit_vr;            /* 1 = Implicit VR LE, 0 = Explicit VR LE */
} DicomFile;

/* ---------- Low-level byte helpers ---------- */

/* DICOM (outside Big Endian TS) is little-endian on the wire. */
static uint16_t read_u16(FILE *fp) {
    unsigned char b[2];
    if (fread(b, 1, 2, fp) != 2) return 0xFFFF; /* sentinel: read failed */
    return (uint16_t)(b[0] | (b[1] << 8));
}

static uint32_t read_u32(FILE *fp) {
    unsigned char b[4];
    if (fread(b, 1, 4, fp) != 4) return 0xFFFFFFFF;
    return (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24));
}

/* ---------- VR classification ----------
 * In Explicit VR, some VRs use a 2-byte length field, others use a
 * 4-byte length field (with 2 reserved bytes in between). This matters
 * because reading the wrong width desyncs the rest of the file.
 */
static int vr_uses_4byte_length(const char *vr) {
    /* OB, OW, OF, SQ, UT, UN (and a few newer ones) use 4-byte lengths */
    return (strcmp(vr, "OB") == 0 || strcmp(vr, "OW") == 0 ||
            strcmp(vr, "OF") == 0 || strcmp(vr, "SQ") == 0 ||
            strcmp(vr, "UT") == 0 || strcmp(vr, "UN") == 0 ||
            strcmp(vr, "OD") == 0 || strcmp(vr, "OL") == 0 ||
            strcmp(vr, "UC") == 0 || strcmp(vr, "UR") == 0);
}

/* Is this VR text we can safely print as a string? */
static int vr_is_string_like(const char *vr) {
    static const char *string_vrs[] = {
        "AE","AS","CS","DA","DS","DT","IS","LO","LT","PN",
        "SH","ST","TM","UI","UC","UR", NULL
    };
    for (int i = 0; string_vrs[i]; i++)
        if (strcmp(vr, string_vrs[i]) == 0) return 1;
    return 0;
}

/* Trim trailing space/NUL padding DICOM uses to keep elements even-length. */
static void rtrim_dicom_padding(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\0')) {
        s[--len] = '\0';
    }
}

/* ---------- Header / preamble ---------- */

/* Returns 1 if valid DICOM, 0 otherwise. Leaves file position right after
 * the "DICM" magic, ready to read the File Meta group. */
static int read_preamble(FILE *fp) {
    unsigned char preamble[128];
    char magic[4];

    if (fread(preamble, 1, 128, fp) != 128) {
        fprintf(stderr, "Error: file too short to contain a 128-byte preamble.\n");
        return 0;
    }
    if (fread(magic, 1, 4, fp) != 4 || memcmp(magic, "DICM", 4) != 0) {
        fprintf(stderr, "Error: 'DICM' magic not found at offset 128. "
                        "This may not be a Part 10 DICOM file (could be a raw "
                        "dataset with no preamble, which this tool doesn't handle).\n");
        return 0;
    }
    return 1;
}

/* ---------- File Meta group (0002,xxxx) ----------
 * Always Explicit VR Little Endian, no matter what the dataset's transfer
 * syntax turns out to be. We parse it generically and specifically pull
 * out 0002,0010 (Transfer Syntax UID) to decide how to read the rest.
 */
static int read_file_meta(DicomFile *df, int print_tags) {
    FILE *fp = df->fp;
    uint32_t group_length = 0;
    long meta_end = -1;

    while (1) {
        long pos = ftell(fp);
        if (meta_end >= 0 && pos >= meta_end) break;

        uint16_t group = read_u16(fp);
        uint16_t element = read_u16(fp);
        if (feof(fp)) break;
        if (group != 0x0002) {
            /* We've walked past the meta group; rewind so the dataset
             * parser starts cleanly at this tag. */
            fseek(fp, pos, SEEK_SET);
            break;
        }

        char vr[3] = {0};
        if (fread(vr, 1, 2, fp) != 2) break;

        uint32_t length;
        if (vr_uses_4byte_length(vr)) {
            read_u16(fp); /* 2 reserved bytes */
            length = read_u32(fp);
        } else {
            length = read_u16(fp);
        }

        unsigned char *buf = malloc(length + 1);
        if (!buf) { fprintf(stderr, "Error: out of memory.\n"); return 0; }
        if (length > 0 && fread(buf, 1, length, fp) != length) {
            fprintf(stderr, "Error: unexpected EOF reading meta tag (%04X,%04X).\n",
                    group, element);
            free(buf);
            return 0;
        }
        buf[length] = '\0';

        if (group == 0x0002 && element == 0x0000) {
            /* Group Length value tells us where the meta group ends,
             * so we know when to stop and hand off to dataset parsing. */
            if (length == 4) {
                group_length = (uint32_t)(buf[0] | (buf[1]<<8) | (buf[2]<<16) | ((uint32_t)buf[3]<<24));
                meta_end = ftell(fp) + group_length;
            }
        }
        if (group == 0x0002 && element == 0x0010) {
            size_t n = length < sizeof(df->transfer_syntax) - 1
                       ? length : sizeof(df->transfer_syntax) - 1;
            memcpy(df->transfer_syntax, buf, n);
            df->transfer_syntax[n] = '\0';
            rtrim_dicom_padding(df->transfer_syntax);
        }

        if (print_tags) {
            const char *name = lookup_tag_name(group, element);
            if (vr_is_string_like(vr)) {
                rtrim_dicom_padding((char *)buf);
                printf("(%04X,%04X) %s len=%-4u %-32s: %s\n",
                       group, element, vr, length, name, buf);
            } else {
                printf("(%04X,%04X) %s len=%-4u %-32s: <binary, %u bytes>\n",
                       group, element, vr, length, name, length);
            }
        }
        free(buf);
    }

    if (df->transfer_syntax[0] == '\0') {
        fprintf(stderr, "Error: no Transfer Syntax UID (0002,0010) found in "
                        "file meta group. Cannot determine dataset encoding.\n");
        return 0;
    }

    if (strcmp(df->transfer_syntax, TS_IMPLICIT_VR_LE) == 0) {
        df->implicit_vr = 1;
    } else if (strcmp(df->transfer_syntax, TS_EXPLICIT_VR_LE) == 0) {
        df->implicit_vr = 0;
    } else {
        fprintf(stderr,
            "Warning: Transfer Syntax '%s' is not Implicit or Explicit VR LE.\n"
            "This tool only decodes those two. Attempting Explicit VR LE parsing,\n"
            "but tag boundaries will likely desync on compressed/Big-Endian data.\n",
            df->transfer_syntax);
        df->implicit_vr = 0;
    }

    return 1;
}

/* ---------- Dataset parsing ---------- */

/* Implicit VR: tag(4) + length(4) + value. No VR on the wire - we'd need
 * a full data dictionary to know the "real" VR. We print "??" for VR and
 * rely on length + a heuristic (looks-like-text) to decide how to display
 * the value. This is a known simplification vs. a production parser. */
static int read_dataset_implicit(DicomFile *df) {
    FILE *fp = df->fp;

    while (1) {
        uint16_t group = read_u16(fp);
        uint16_t element = read_u16(fp);
        if (feof(fp)) break;

        uint32_t length = read_u32(fp);
        const char *name = lookup_tag_name(group, element);

        /* PixelData is often huge - don't print its bytes, just its size,
         * and skip over it rather than allocating megabytes for nothing. */
        if (group == 0x7FE0 && element == 0x0010) {
            printf("(%04X,%04X) ?? len=%-10u %-32s: <pixel data, %u bytes, skipped>\n",
                   group, element, length, name, length);
            fseek(fp, length, SEEK_CUR);
            continue;
        }

        if (length == 0) {
            printf("(%04X,%04X) ?? len=0          %-32s: (empty)\n", group, element, name);
            continue;
        }
        if (length == 0xFFFFFFFF) {
            /* Undefined length - this is a Sequence (SQ) or item delimiter
             * in implicit VR. Properly parsing nested sequence items needs
             * a recursive descent; we just flag it rather than guess. */
            printf("(%04X,%04X) ?? len=undefined %-32s: <sequence, not expanded>\n",
                   group, element, name);
            continue;
        }

        unsigned char *buf = malloc(length + 1);
        if (!buf) { fprintf(stderr, "Error: out of memory.\n"); return 0; }
        if (fread(buf, 1, length, fp) != length) {
            fprintf(stderr, "Error: unexpected EOF reading tag (%04X,%04X).\n",
                    group, element);
            free(buf);
            return 0;
        }
        buf[length] = '\0';

        /* Heuristic: if every byte is printable/space, show as text. */
        int printable = 1;
        for (uint32_t i = 0; i < length; i++) {
            if (buf[i] != 0 && (buf[i] < 0x20 || buf[i] > 0x7E)) {
                printable = 0;
                break;
            }
        }

        if (printable) {
            rtrim_dicom_padding((char *)buf);
            printf("(%04X,%04X) ?? len=%-10u %-32s: %s\n", group, element, length, name, buf);
        } else {
            printf("(%04X,%04X) ?? len=%-10u %-32s: <binary, %u bytes>\n",
                   group, element, length, name, length);
        }
        free(buf);
    }
    return 1;
}

/* Explicit VR LE: tag(4) + VR(2) + length(2 or 4, per VR) + value. */
static int read_dataset_explicit(DicomFile *df) {
    FILE *fp = df->fp;

    while (1) {
        uint16_t group = read_u16(fp);
        uint16_t element = read_u16(fp);
        if (feof(fp)) break;

        char vr[3] = {0};
        if (fread(vr, 1, 2, fp) != 2) break;

        uint32_t length;
        if (vr_uses_4byte_length(vr)) {
            read_u16(fp); /* reserved */
            length = read_u32(fp);
        } else {
            length = read_u16(fp);
        }

        const char *name = lookup_tag_name(group, element);

        if (group == 0x7FE0 && element == 0x0010) {
            printf("(%04X,%04X) %s len=%-10u %-32s: <pixel data, %u bytes, skipped>\n",
                   group, element, vr, length, name, length);
            fseek(fp, length, SEEK_CUR);
            continue;
        }

        if (strcmp(vr, "SQ") == 0 || length == 0xFFFFFFFF) {
            printf("(%04X,%04X) %s len=undefined %-32s: <sequence, not expanded>\n",
                   group, element, vr, name);
            continue;
        }

        if (length == 0) {
            printf("(%04X,%04X) %s len=0          %-32s: (empty)\n", group, element, vr, name);
            continue;
        }

        unsigned char *buf = malloc(length + 1);
        if (!buf) { fprintf(stderr, "Error: out of memory.\n"); return 0; }
        if (fread(buf, 1, length, fp) != length) {
            fprintf(stderr, "Error: unexpected EOF reading tag (%04X,%04X) VR=%s.\n",
                    group, element, vr);
            free(buf);
            return 0;
        }
        buf[length] = '\0';

        if (vr_is_string_like(vr)) {
            rtrim_dicom_padding((char *)buf);
            printf("(%04X,%04X) %s len=%-10u %-32s: %s\n", group, element, vr, length, name, buf);
        } else if (strcmp(vr, "US") == 0 && length == 2) {
            uint16_t val = (uint16_t)(buf[0] | (buf[1] << 8));
            printf("(%04X,%04X) %s len=%-10u %-32s: %u\n", group, element, vr, length, name, val);
        } else if (strcmp(vr, "UL") == 0 && length == 4) {
            uint32_t val = (uint32_t)(buf[0] | (buf[1]<<8) | (buf[2]<<16) | ((uint32_t)buf[3]<<24));
            printf("(%04X,%04X) %s len=%-10u %-32s: %u\n", group, element, vr, length, name, val);
        } else {
            printf("(%04X,%04X) %s len=%-10u %-32s: <binary, %u bytes>\n",
                   group, element, vr, length, name, length);
        }
        free(buf);
    }
    return 1;
}

/* ---------- main ---------- */

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file.dcm>\n", argv[0]);
        return 1;
    }

    DicomFile df;
    memset(&df, 0, sizeof(df));
    df.fp = fopen(argv[1], "rb");
    if (!df.fp) {
        fprintf(stderr, "Error: could not open '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    fseek(df.fp, 0, SEEK_END);
    df.file_size = ftell(df.fp);
    fseek(df.fp, 0, SEEK_SET);

    printf("File: %s (%ld bytes)\n", argv[1], df.file_size);
    printf("---- Preamble check ----\n");
    if (!read_preamble(df.fp)) {
        fclose(df.fp);
        return 1;
    }
    printf("OK: 128-byte preamble + 'DICM' magic found.\n\n");

    printf("---- File Meta Information (0002,xxxx) ----\n");
    if (!read_file_meta(&df, 1)) {
        fclose(df.fp);
        return 1;
    }
    printf("\nTransfer Syntax: %s (%s)\n\n",
           df.transfer_syntax, df.implicit_vr ? "Implicit VR LE" : "Explicit VR LE");

    printf("---- Dataset ----\n");
    int ok = df.implicit_vr ? read_dataset_implicit(&df) : read_dataset_explicit(&df);

    fclose(df.fp);
    return ok ? 0 : 1;
}
