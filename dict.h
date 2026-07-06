/*
 * dict.h - Minimal DICOM tag dictionary.
 *
 * Real dictionaries (like the one in DCMTK or pydicom) have ~4000 entries.
 * This covers the tags you'll actually want to see when eyeballing a file:
 * patient/study/series identity, image geometry, and pixel data itself.
 *
 * Unknown tags aren't an error - we just print "Unknown" and move on.
 * That's normal; no reader has every private/vendor tag memorized.
 */

#ifndef DICT_H
#define DICT_H

typedef struct {
    unsigned short group;
    unsigned short element;
    const char *name;
} TagEntry;

static const TagEntry TAG_DICT[] = {
    /* File Meta Group (0002,xxxx) */
    {0x0002, 0x0000, "FileMetaInformationGroupLength"},
    {0x0002, 0x0001, "FileMetaInformationVersion"},
    {0x0002, 0x0002, "MediaStorageSOPClassUID"},
    {0x0002, 0x0003, "MediaStorageSOPInstanceUID"},
    {0x0002, 0x0010, "TransferSyntaxUID"},
    {0x0002, 0x0012, "ImplementationClassUID"},
    {0x0002, 0x0013, "ImplementationVersionName"},

    /* Patient (0010,xxxx) */
    {0x0010, 0x0010, "PatientName"},
    {0x0010, 0x0020, "PatientID"},
    {0x0010, 0x0030, "PatientBirthDate"},
    {0x0010, 0x0040, "PatientSex"},
    {0x0010, 0x1010, "PatientAge"},

    /* Study (0008,xxxx / 0020,000D) */
    {0x0008, 0x0020, "StudyDate"},
    {0x0008, 0x0030, "StudyTime"},
    {0x0008, 0x0050, "AccessionNumber"},
    {0x0008, 0x0060, "Modality"},
    {0x0008, 0x0070, "Manufacturer"},
    {0x0008, 0x0080, "InstitutionName"},
    {0x0008, 0x0090, "ReferringPhysicianName"},
    {0x0008, 0x1030, "StudyDescription"},
    {0x0008, 0x103E, "SeriesDescription"},
    {0x0020, 0x000D, "StudyInstanceUID"},

    /* Series (0020,000E) */
    {0x0020, 0x000E, "SeriesInstanceUID"},
    {0x0020, 0x0011, "SeriesNumber"},
    {0x0020, 0x0013, "InstanceNumber"},

    /* SOP Common (0008,0016 / 0008,0018) */
    {0x0008, 0x0016, "SOPClassUID"},
    {0x0008, 0x0018, "SOPInstanceUID"},

    /* Image geometry / pixel description (0028,xxxx) */
    {0x0028, 0x0002, "SamplesPerPixel"},
    {0x0028, 0x0004, "PhotometricInterpretation"},
    {0x0028, 0x0010, "Rows"},
    {0x0028, 0x0011, "Columns"},
    {0x0028, 0x0030, "PixelSpacing"},
    {0x0028, 0x0100, "BitsAllocated"},
    {0x0028, 0x0101, "BitsStored"},
    {0x0028, 0x0102, "HighBit"},
    {0x0028, 0x0103, "PixelRepresentation"},
    {0x0028, 0x1050, "WindowCenter"},
    {0x0028, 0x1051, "WindowWidth"},
    {0x0028, 0x1052, "RescaleIntercept"},
    {0x0028, 0x1053, "RescaleSlope"},

    /* Pixel Data (7FE0,0010) */
    {0x7FE0, 0x0010, "PixelData"},
};

static const int TAG_DICT_SIZE = sizeof(TAG_DICT) / sizeof(TagEntry);

/* Linear scan is fine - ~40 entries, called once per tag in a file. */
static const char *lookup_tag_name(unsigned short group, unsigned short element) {
    for (int i = 0; i < TAG_DICT_SIZE; i++) {
        if (TAG_DICT[i].group == group && TAG_DICT[i].element == element) {
            return TAG_DICT[i].name;
        }
    }
    return "Unknown";
}

#endif /* DICT_H */
