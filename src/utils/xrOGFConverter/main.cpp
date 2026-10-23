// xrOGFConverter: turns compiled game models (.ogf) back into SDK library objects (.object)
// so props of the game can be placed as static objects in the Level Editor.
//
//   xrOGFConverter <model.ogf | folder> <output folder> [-skeletal]
//
// A folder is converted recursively and the relative layout is kept. Skeletal models
// (characters, physics props with bones) are skipped unless -skeletal is given, because
// the Level Editor places them through spawn elements rather than as static geometry.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "xr_ogf.h"

namespace fs = std::filesystem;
using namespace xray_re;

namespace
{
	struct SStats
	{
		unsigned converted = 0;
		unsigned skipped = 0;
		unsigned failed = 0;
	};

	bool ConvertOne(const fs::path& src, const fs::path& dst, bool include_skeletal, SStats& stats)
	{
		xr_ogf* ogf = nullptr;
		try
		{
			ogf = xr_ogf::load_ogf(src.string());
		}
		catch (...)
		{
			ogf = nullptr;
		}

		if (!ogf)
		{
			std::printf("! can't load %s\n", src.string().c_str());
			++stats.failed;
			return false;
		}

		if (ogf->skeletal() && !include_skeletal)
		{
			std::printf("- skip skeletal %s\n", src.string().c_str());
			++stats.skipped;
			delete ogf;
			return true;
		}

		bool ok = false;
		try
		{
			ogf->to_object();
			std::error_code ec;
			fs::create_directories(dst.parent_path(), ec);
			ok = ogf->save_object(dst.string().c_str());
		}
		catch (...)
		{
			ok = false;
		}
		delete ogf;

		if (ok)
		{
			std::printf("+ %s\n", dst.string().c_str());
			++stats.converted;
		}
		else
		{
			std::printf("! can't convert %s\n", src.string().c_str());
			++stats.failed;
		}
		return ok;
	}
}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		std::printf("usage: xrOGFConverter <model.ogf | folder> <output folder> [-skeletal]\n");
		return 1;
	}

	const fs::path input = argv[1];
	const fs::path output = argv[2];
	bool include_skeletal = false;
	for (int i = 3; i < argc; ++i)
		if (0 == std::strcmp(argv[i], "-skeletal"))
			include_skeletal = true;

	SStats stats;
	std::error_code ec;
	if (fs::is_directory(input, ec))
	{
		for (const fs::directory_entry& entry : fs::recursive_directory_iterator(input, ec))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".ogf")
				continue;

			fs::path dst = output / fs::relative(entry.path(), input, ec);
			dst.replace_extension(".object");
			ConvertOne(entry.path(), dst, include_skeletal, stats);
		}
	}
	else
	{
		fs::path dst = output / input.filename();
		dst.replace_extension(".object");
		ConvertOne(input, dst, include_skeletal, stats);
	}

	std::printf("converted %u, skipped %u, failed %u\n", stats.converted, stats.skipped, stats.failed);
	return stats.failed ? 2 : 0;
}
