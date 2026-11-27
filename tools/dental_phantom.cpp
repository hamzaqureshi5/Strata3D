// Writes a synthetic dental scan as a DICOM series, for testing segmentation end to end.
//
//   dental_phantom <output_dir> [--cbct]
//
// Anatomy (all sizes in mm, 0.5 mm isotropic voxels, 160 x 160 x 100 voxels):
//   - soft tissue block (40 HU) in air (-1000 HU)
//   - horseshoe-shaped mandible (900 HU)
//   - 12 teeth along the arch: dentin (1700 HU) with an enamel cap (2500 HU),
//     roots embedded in the bone, crowns above it
//   - mandibular canal: a 2.4 mm soft-tissue tunnel (30 HU) running through the bone
//
// Default output is CT-like: unsigned pixels with RescaleIntercept -1024, so a reader must
// apply the rescale to get Hounsfield units. With --cbct the pixels are HU + 1000 with no
// rescale, which mimics an uncalibrated cone-beam scan.
#include <dcmtk/dcmdata/dctk.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

const int W = 160, H = 160, D = 100;
const double SPACING = 0.5;
const double PI = 3.14159265358979323846;

// Arch: half ring around (ARCH_CX, ARCH_CY), opening towards +y.
const double ARCH_CX = 40.0, ARCH_CY = 50.0;
const double ARCH_R = 27.0, ARCH_HALF_WIDTH = 5.0;
const double BONE_Z0 = 5.0, BONE_Z1 = 25.0;
const double CANAL_Z = 11.0, CANAL_R = 1.2;
const int TEETH = 12;
const double TOOTH_R = 2.6, TOOTH_Z0 = 14.0, TOOTH_Z1 = 36.0, ENAMEL_H = 4.0;

double hounsfield(double x, double y, double z)
{
	double v = -1000.0;
	if (x > 5 && x < 75 && y > 5 && y < 75 && z > 2 && z < 30)
		v = 40.0;

	double dx = x - ARCH_CX, dy = y - ARCH_CY;
	double r = std::sqrt(dx * dx + dy * dy);
	double angle = std::atan2(-dy, dx);  // 0..pi on the arch side (y < ARCH_CY)
	bool onArch = angle >= 0.0 && angle <= PI;

	if (onArch && std::fabs(r - ARCH_R) <= ARCH_HALF_WIDTH && z >= BONE_Z0 && z <= BONE_Z1)
		v = 900.0;

	// canal follows the arch centre line inside the bone
	double dr = r - ARCH_R, dz = z - CANAL_Z;
	if (onArch && dr * dr + dz * dz <= CANAL_R * CANAL_R)
		v = 30.0;

	for (int i = 0; i < TEETH; ++i)
	{
		double a = PI * (i + 0.5) / TEETH;
		double tx = ARCH_CX + ARCH_R * std::cos(a);
		double ty = ARCH_CY - ARCH_R * std::sin(a);
		double ex = x - tx, ey = y - ty;
		if (ex * ex + ey * ey <= TOOTH_R * TOOTH_R && z >= TOOTH_Z0 && z <= TOOTH_Z1)
			v = (z >= TOOTH_Z1 - ENAMEL_H) ? 2500.0 : 1700.0;
	}
	return v;
}

} // namespace

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::fprintf(stderr, "usage: %s <output_dir> [--cbct]\n", argv[0]);
		return 1;
	}
	const std::string outDir = argv[1];
	const bool cbct = argc > 2 && std::strcmp(argv[2], "--cbct") == 0;
	const double intercept = cbct ? 0.0 : -1024.0;
	const double offset = cbct ? 1000.0 : 1024.0;  // stored value = HU + offset

	char studyUID[100], seriesUID[100], frameUID[100];
	dcmGenerateUniqueIdentifier(studyUID, SITE_STUDY_UID_ROOT);
	dcmGenerateUniqueIdentifier(seriesUID, SITE_SERIES_UID_ROOT);
	dcmGenerateUniqueIdentifier(frameUID, SITE_INSTANCE_UID_ROOT);

	char spacing[32];
	std::snprintf(spacing, sizeof(spacing), "%g\\%g", SPACING, SPACING);

	std::vector<Uint16> px(W * H);
	for (int k = 0; k < D; ++k)
	{
		const double z = k * SPACING;
		for (int j = 0; j < H; ++j)
			for (int i = 0; i < W; ++i)
				px[j * W + i] = static_cast<Uint16>(hounsfield(i * SPACING, j * SPACING, z) + offset);

		char sopUID[100];
		dcmGenerateUniqueIdentifier(sopUID, SITE_INSTANCE_UID_ROOT);
		char pos[64], loc[32], interceptStr[32];
		std::snprintf(pos, sizeof(pos), "0\\0\\%g", z);
		std::snprintf(loc, sizeof(loc), "%g", z);
		std::snprintf(interceptStr, sizeof(interceptStr), "%g", intercept);

		DcmFileFormat file;
		DcmDataset* ds = file.getDataset();
		ds->putAndInsertString(DCM_SOPClassUID, UID_CTImageStorage);
		ds->putAndInsertString(DCM_SOPInstanceUID, sopUID);
		ds->putAndInsertString(DCM_StudyInstanceUID, studyUID);
		ds->putAndInsertString(DCM_SeriesInstanceUID, seriesUID);
		ds->putAndInsertString(DCM_FrameOfReferenceUID, frameUID);
		ds->putAndInsertString(DCM_Modality, "CT");
		ds->putAndInsertString(DCM_PatientName, cbct ? "Phantom^DentalCBCT" : "Phantom^DentalCT");
		ds->putAndInsertString(DCM_PatientID, "PHANTOM-DENTAL");
		ds->putAndInsertString(DCM_StudyDescription, "Synthetic dental phantom");
		ds->putAndInsertString(DCM_SeriesDescription, cbct ? "Synthetic CBCT" : "Synthetic CT");
		ds->putAndInsertString(DCM_SeriesNumber, "1");
		ds->putAndInsertString(DCM_InstanceNumber, std::to_string(k + 1).c_str());
		ds->putAndInsertString(DCM_ImagePositionPatient, pos);
		ds->putAndInsertString(DCM_ImageOrientationPatient, "1\\0\\0\\0\\1\\0");
		ds->putAndInsertString(DCM_SliceLocation, loc);
		ds->putAndInsertString(DCM_SliceThickness, "0.5");
		ds->putAndInsertString(DCM_PixelSpacing, spacing);
		ds->putAndInsertUint16(DCM_SamplesPerPixel, 1);
		ds->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
		ds->putAndInsertUint16(DCM_Rows, H);
		ds->putAndInsertUint16(DCM_Columns, W);
		ds->putAndInsertUint16(DCM_BitsAllocated, 16);
		ds->putAndInsertUint16(DCM_BitsStored, 12);
		ds->putAndInsertUint16(DCM_HighBit, 11);
		ds->putAndInsertUint16(DCM_PixelRepresentation, 0);
		ds->putAndInsertString(DCM_RescaleIntercept, interceptStr);
		ds->putAndInsertString(DCM_RescaleSlope, "1");
		ds->putAndInsertString(DCM_RescaleType, cbct ? "US" : "HU");
		ds->putAndInsertUint16Array(DCM_PixelData, px.data(), W * H);

		char name[1024];
		std::snprintf(name, sizeof(name), "%s/slice_%03d.dcm", outDir.c_str(), k);
		OFCondition st = file.saveFile(name, EXS_LittleEndianExplicit);
		if (st.bad())
		{
			std::fprintf(stderr, "writing %s failed: %s\n", name, st.text());
			return 1;
		}
	}
	std::printf("Wrote %d slices (%dx%d, %.1f mm, %s) to %s\n", D, W, H, SPACING, cbct ? "CBCT-like" : "CT/HU", outDir.c_str());
	return 0;
}
